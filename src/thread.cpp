#include <iostream>
#include <inttypes.h>
#include <deque>
#include <string.h>
#include <mutex>
#include <condition_variable>

#ifdef USE_NUMA
#include <sched.h>
#include <numa.h>
#endif

#include "thread.h"
#include "search.h"
#include "history.h"
#include "uci.h"

Worker::Worker(ThreadPool* threadPool, int threadId) : threadPool(threadPool), threadId(threadId), mainThread(threadId == 0) {
    history.initHistory();
}

void Worker::startSearching() {
    rootBoard = threadPool->rootBoard;
    boardHistory.clear();
    boardHistory.reserve(MAX_PLY + 1);
    for (uint64_t hash : threadPool->rootBoardHistory) {
        boardHistory.push_back(hash);
    }
    memcpy(&rootBoard, &threadPool->rootBoard, sizeof(Board));
    searchParameters = threadPool->searchParameters;

    if (searchParameters.perft)
        searchData.nodesSearched = perft(rootBoard, searchParameters.depth);
    else if (searchParameters.genfens)
        tgenfens();
    else if (UCI::Options.datagen.value)
        tdatagen();
    else
        tsearch();
}

void Worker::waitForSearchFinished() {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&] { return !searching; });
}

void Worker::configureThreadBinding() {
#ifdef USE_NUMA
    auto getAvailableCores = []() {
        cpu_set_t cs;
        CPU_ZERO(&cs);
        sched_getaffinity(0, sizeof(cs), &cs);
        return CPU_COUNT(&cs);
    };

    // Only bind when claiming the majority of the systems cores
    static int availableCores = getAvailableCores();
    if (UCI::Options.threads.value <= availableCores / 2)
        return;

    auto getCoreMasksPerNumaNode = []() {
        // Credit to Halogen for this code
        std::vector<cpu_set_t> nodeCoreMasks;
        if (numa_available() == -1) {
            return nodeCoreMasks;
        }

        for (int node = 0; node <= numa_max_node(); ++node) {
            struct bitmask* coreMask = numa_allocate_cpumask();
            if (numa_node_to_cpus(node, coreMask) != 0) {
                cerr << "Failed to get CPUs for NUMA node: " << node << endl;
                return nodeCoreMasks;
            }

            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);

            for (size_t cpu = 0; cpu < coreMask->size; ++cpu)
                if (numa_bitmask_isbitset(coreMask, cpu)) {
                    CPU_SET(cpu, &cpuset);
                }

            numa_free_cpumask(coreMask);
            nodeCoreMasks.push_back(cpuset);
        }

        return nodeCoreMasks;
    };

    static auto coreMasksPerNumaNode = getCoreMasksPerNumaNode();
    if (coreMasksPerNumaNode.size() == 0)
        return;
    
    int node = threadId % coreMasksPerNumaNode.size();
    pthread_t self = pthread_self();
    pthread_setaffinity_np(self, sizeof(cpu_set_t), &coreMasksPerNumaNode[node]);

#endif
}

void Worker::idle() {
    threadPool->startedThreads++;

    while (!exiting) {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&] { return searching; });

        if (exiting)
            return;

        startSearching();
        searching = false;

        cv.notify_all();
    }
}

void Worker::exit() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        searching = true;
        exiting = true;
    }
    cv.notify_all();
    if (threadPool->threads[threadId]->joinable())
        threadPool->threads[threadId]->join();
}

void Worker::ucinewgame() {
    history.initHistory();
}