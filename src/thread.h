#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <atomic>

#ifdef USE_NUMA
#include <sched.h>
#include <numa.h>
#endif

#include "board.h"
#include "search.h"
#include "history.h"
#include "nnue.h"
#include "tt.h"

inline int getNumaNode(int threadId) {
#ifdef USE_NUMA
    if (numa_available() == -1) {
        return 0;
    }
    return threadId % numa_num_configured_nodes();
#else
    (void) threadId;
    return 0;
#endif
}

inline bool shouldConfigureNuma() {
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
#else
    return false;
#endif
}

inline void configureThreadBinding(int threadId) {
#ifdef USE_NUMA
    if (!shouldConfigureNuma())
        return;

    auto getCoreMasksPerNumaNode = []() {
        // Credit to Halogen for this code
        std::vector<cpu_set_t> nodeCoreMasks;
        if (numa_available() == -1) {
            return nodeCoreMasks;
        }

        for (int node = 0; node <= numa_num_configured_nodes(); ++node) {
            struct bitmask* coreMask = numa_allocate_cpumask();
            if (numa_node_to_cpus(node, coreMask) != 0) {
                std::cerr << "info string Could not get core mask for NUMA node " << node << std::endl;
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
    
    int result = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &coreMasksPerNumaNode[getNumaNode(threadId)]);
    if (result != 0) {
        std::cerr << "info string Could not set NUMA affinity for thread " << threadId << std::endl;
    }

#endif
    (void) threadId;
}

struct RootMove {
    Eval value = -EVAL_INFINITE;
    Eval meanScore = EVAL_NONE;
    int16_t depth = 0;
    int selDepth = 0;
    Move move = MOVE_NULL;
    std::vector<Move> pv;
};

class ThreadPool;

class Worker {

    std::vector<uint64_t> boardHistory;

public:

    Board rootBoard;
    
    std::mutex mutex;
    std::condition_variable cv;

    bool searching = false;
    bool stopped = false;
    bool exiting = false;

    SearchData searchData;
    SearchParameters searchParameters;
    History history;
    NNUE nnue;

    ThreadPool* threadPool;

    int threadId;
    bool mainThread;

    std::vector<RootMove> rootMoves;
    std::map<Move, uint64_t> rootMoveNodes;
    std::vector<Move> excludedRootMoves;

    Worker() = delete;
    Worker(ThreadPool* threadPool, NetworkData* networkData, int threadId);

    void startSearching();
    void waitForSearchFinished();
    void idle();
    void exit();

    void ucinewgame();

    void tdatagen();

private:

    void tgenfens();

    void tsearch();
    void iterativeDeepening();
    void sortRootMoves();
    void printUCI(Worker* thread, int multiPvCount = 1);
    Worker* chooseBestThread();

    Board* doMove(Board* board, uint64_t newHash, Move move);
    void undoMove();
    Board* doNullMove(Board* board);
    void undoNullMove();
    bool hasUpcomingRepetition(Board* board, int ply);
    bool isDraw(Board* board, int ply);

    template <NodeType nt>
    Eval search(Board* board, SearchStack* stack, int16_t depth, Eval alpha, Eval beta, bool cutNode);

    template <NodeType nodeType>
    Eval qsearch(Board* board, SearchStack* stack, Eval alpha, Eval beta);

};

static_assert(sizeof(Worker) % 64 == 0);

class ThreadPool {

public:

    std::vector<std::unique_ptr<Worker>> workers;
    std::vector<std::unique_ptr<std::thread>> threads;

    SearchParameters searchParameters;
    Board rootBoard;
    std::vector<uint64_t> rootBoardHistory;

    std::atomic<size_t> startedThreads;

    std::vector<NetworkData*> networkWeights;

    ThreadPool() : workers(0), threads(0), searchParameters{}, rootBoardHistory() {
        resize(0);
    }

    ~ThreadPool() {
        exit();
    }

    void resize(size_t numThreads) {
        if (threads.size() == numThreads)
            return;
        
        exit();

#ifdef USE_NUMA
        // Free existing duplicated weights
        if (numa_available() != -1) {
            for (size_t i = 0; i < networkWeights.size(); i++) {
                if (networkWeights[i] != globalNetworkData) {
                    numa_free(networkWeights[i], sizeof(NetworkData));
                }
            }
        }
#endif

        networkWeights.clear();
        networkWeights.resize(1);
        networkWeights[0] = globalNetworkData;

#ifdef USE_NUMA
        // Duplicate network weights across NUMA nodes if necessary
        if (numa_available() != -1 && shouldConfigureNuma()) {

            networkWeights.clear();
            networkWeights.resize(numa_num_configured_nodes());

            for (size_t i = 0; i < networkWeights.size(); i++) {
                NetworkData* weights = reinterpret_cast<NetworkData*>(numa_alloc_onnode(sizeof(NetworkData), i));
                if (!weights) {
                    std::cerr << "info string Could not allocate a NetworkData object on NUMA node " << i << std::endl;
                    throw std::bad_alloc();
                }
                std::cout << "info string Allocated a NetworkData object on NUMA node " << i << std::endl;
                madvise(weights, sizeof(NetworkData), MADV_HUGEPAGE);
                std::memcpy(weights, globalNetworkData, sizeof(NetworkData));
                networkWeights[i] = weights;
            }
        }
#endif

        threads.clear();
        workers.clear();
        workers.resize(numThreads);

        startedThreads = 0;

        for (size_t i = 0; i < numThreads; i++) {
            threads.push_back(std::make_unique<std::thread>([this, i]() {
                configureThreadBinding(i);
                workers[i] = std::make_unique<Worker>(this, networkWeights[getNumaNode(i)], i);
                workers[i]->idle();
            }));
        }

        while (startedThreads < numThreads) {}
    }

    void startSearching(Board board, std::vector<uint64_t> boardHistory, SearchParameters parameters) {
        stopSearching();
        waitForSearchFinished();

        rootBoard = std::move(board);
        rootBoardHistory = std::move(boardHistory);
        searchParameters = std::move(parameters);

        for (auto& worker : workers) {
            worker.get()->rootMoves.clear();
            worker.get()->stopped = false;
            std::lock_guard<std::mutex> lock(worker.get()->mutex);
            worker.get()->searching = true;
        }

        for (auto& worker : workers) {
            worker.get()->cv.notify_all();
        }
    }

    void stopSearching() {
        for (auto& worker : workers) {
            worker.get()->stopped = true;
        }
    }

    void waitForSearchFinished() {
        for (auto& worker : workers) {
            worker.get()->waitForSearchFinished();
        }
    }

    void waitForHelpersFinished() {
        for (std::size_t i = 1; i < workers.size(); i++) {
            workers[i].get()->waitForSearchFinished();
        }
    }

    void exit() {
        for (auto& worker : workers) {
            worker.get()->exit();
        }
    }

    void ucinewgame() {
        TT.clear();
        for (auto& worker : workers) {
            worker.get()->ucinewgame();
        }
    }

    uint64_t nodesSearched() {
        uint64_t sum = 0;
        for (auto& worker : workers) {
            sum += worker.get()->searchData.nodesSearched;
        }
        return sum;
    }

    uint64_t tbhits() {
        uint64_t sum = 0;
        for (auto& worker : workers) {
            sum += worker.get()->searchData.tbHits;
        }
        return sum;
    }

};