#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <atomic>
#include <array>

#ifdef USE_NUMA
#include <sched.h>
#include <numa.h>
#endif

#include "board.h"
#include "search.h"
#include "history.h"
#include "nnue.h"
#include "tt.h"

inline bool shouldConfigureNuma() {
#ifdef USE_NUMA
    if (numa_available() == -1) {
        return false;
    }

    // Only bind when claiming the majority of the systems cores
    static int availableCores = std::thread::hardware_concurrency();
    if (UCI::Options.threads.value <= availableCores / 2)
        return false;
    
    return true;
#else
    return false;
#endif
}

#ifdef USE_NUMA
inline std::vector<std::pair<int, std::vector<size_t>>> getCoresPerNumaNode() {
    auto getCoresPerNode = []() {
        std::vector<std::pair<int, std::vector<size_t>>> coresPerNode;

        if (numa_available() == -1) {

            std::vector<size_t> cores;
            for (size_t i = 0; i < std::thread::hardware_concurrency(); i++) {
                cores.push_back(i);
            }
            coresPerNode.push_back(std::make_pair(0, cores));

        } else {

            for (int node = 0; node < numa_num_configured_nodes(); ++node) {
                struct bitmask* coreMask = numa_allocate_cpumask();
                if (numa_node_to_cpus(node, coreMask) != 0) {
                    std::cerr << "info string Could not get core mask for NUMA node " << node << std::endl;
                    throw std::bad_alloc();
                }

                cpu_set_t cpuset;
                CPU_ZERO(&cpuset);

                std::vector<size_t> cores;
                for (size_t cpu = 0; cpu < coreMask->size; ++cpu) {
                    if (numa_bitmask_isbitset(coreMask, cpu)) {
                        cores.push_back(cpu);
                    }
                }

                numa_free_cpumask(coreMask);
                if (cores.size()) {
                    coresPerNode.push_back(std::make_pair(node, cores));
                }
            }

        }

        std::cout << "info string NUMA Core to Node association:" << std::endl;
        unsigned int coreCount = 0;
        for (auto& [node, cores] : coresPerNode) {
            std::cout << "info string Node " << node << ": ";
            for (auto& core : cores) std::cout << core << ",";
            std::cout << std::endl;
            coreCount += cores.size();
        }
        if (coreCount != std::thread::hardware_concurrency()) {
            std::cerr << "info string Detected NUMA core count does not match hardware concurrency!" << std::endl;
            throw std::bad_alloc();
        }

        return coresPerNode;
    };

    static auto coresPerNode = getCoresPerNode();
    assert(coresPerNode.size() > 0);
    return coresPerNode;
}

inline int getNumaNodeCount() {
    return getCoresPerNumaNode().size();
}

#endif

inline int getNumaNode(size_t threadId) {
#ifdef USE_NUMA
    if (!shouldConfigureNuma()) {
        return 0;
    }

    auto coresPerNode = getCoresPerNumaNode();
    size_t counter = 0;
    threadId %= std::thread::hardware_concurrency();

    for (auto& [node, cores] : coresPerNode) {
        if (threadId < counter + cores.size())
            return node;
        counter += cores.size();
    }

    std::cerr << "info string This should never happen!" << std::endl;
    throw std::bad_alloc();
#else
    (void) threadId;
    return 0;
#endif
}

inline int getThreadsOnNode(size_t numThreads, int node) {
    int counter = 0;
    for (size_t idx = 0; idx < numThreads; idx++) {
        int threadNode = getNumaNode(idx);
        if (node == threadNode)
            counter++;
    }
    return counter;
}

inline void configureThreadBinding(int threadId) {
#ifdef USE_NUMA
    if (!shouldConfigureNuma())
        return;

    int numaNode = getNumaNode(threadId);
    numa_run_on_node(numaNode);
    numa_set_preferred(numaNode);

#endif
    (void) threadId;
}

struct RootMove {
    Eval value = -EVAL_INFINITE;
    Eval meanScore = EVAL_NONE;
    Depth depth = 0;
    int selDepth = 0;
    Move move = Move::none();
    std::vector<Move> pv;
};

class ThreadPool;

class Worker {

    std::vector<Hash> boardHistory;

public:

    Board rootBoard;
    
    std::mutex mutex;
    std::condition_variable cv;

    bool searching = false;
    std::atomic_bool stopped = false;
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
    std::vector<std::array<MoveGen, 2>> movepickers;

    Worker() = delete;
    Worker(ThreadPool* threadPool, NetworkData* networkData, SharedHistory* sharedHistory, int threadId);

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

    Board* doMove(Board* board, Hash newHash, Move move);
    void undoMove();
    Board* doNullMove(Board* board);
    void undoNullMove();
    bool hasUpcomingRepetition(Board* board, int ply);
    bool isDraw(Board* board, int ply);

    template <NodeType nt>
    Eval search(Board* board, SearchStack* stack, Depth depth, Eval alpha, Eval beta, bool cutNode);

    template <NodeType nodeType>
    Eval qsearch(Board* board, SearchStack* stack, Eval alpha, Eval beta);

};

class ThreadPool {

public:

    std::vector<std::unique_ptr<Worker>> workers;
    std::vector<std::unique_ptr<std::thread>> threads;

    SearchParameters searchParameters;
    Board rootBoard;
    std::vector<Hash> rootBoardHistory;

    std::atomic<size_t> startedThreads;

    std::vector<NetworkData*> networkWeights;
    std::vector<SharedHistory*> sharedHistories;

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

        // Free previous allocations
#ifdef USE_NUMA
        if (shouldConfigureNuma()) {

            for (size_t i = 0; i < networkWeights.size(); i++) {
                if (networkWeights[i] != globalNetworkData) {
                    numa_free(networkWeights[i], sizeof(NetworkData));
                }
            }
            networkWeights.clear();

            for (size_t i = 0; i < sharedHistories.size(); i++) {
                sharedHistories[i]->free();
                numa_free(sharedHistories[i], sizeof(SharedHistory));
            }
            sharedHistories.clear();

        }
#endif
        for (size_t i = 0; i < sharedHistories.size(); i++) {
            sharedHistories[i]->free();
            alignedFree(sharedHistories[i]);
        }
        sharedHistories.clear();

        // Allocate new objects
#ifdef USE_NUMA
        if (shouldConfigureNuma()) {

            auto coresPerNode = getCoresPerNumaNode();

            networkWeights.resize(coresPerNode.size());
            for (size_t i = 0; i < networkWeights.size(); i++) {
                int nodeIdx = coresPerNode[i].first;
                NetworkData* weights = reinterpret_cast<NetworkData*>(numa_alloc_onnode(sizeof(NetworkData), nodeIdx));
                if (!weights) {
                    std::cerr << "info string Could not allocate a NetworkData object on NUMA node " << nodeIdx << std::endl;
                    throw std::bad_alloc();
                }
                std::cout << "info string Allocated a NetworkData object on NUMA node " << nodeIdx << std::endl;
                madvise(weights, sizeof(NetworkData), MADV_HUGEPAGE);
                std::memcpy(weights, globalNetworkData, sizeof(NetworkData));
                networkWeights[i] = weights;
            }

            sharedHistories.resize(coresPerNode.size());
            for (size_t i = 0; i < sharedHistories.size(); i++) {
                int nodeIdx = coresPerNode[i].first;
                SharedHistory* history = reinterpret_cast<SharedHistory*>(numa_alloc_onnode(sizeof(SharedHistory), nodeIdx));
                if (!history) {
                    std::cerr << "info string Could not allocate a SharedHistory object on NUMA node " << nodeIdx << std::endl;
                    throw std::bad_alloc();
                }
                std::cout << "info string Allocated a SharedHistory object on NUMA node " << nodeIdx << std::endl;
                madvise(history, sizeof(SharedHistory), MADV_HUGEPAGE);
                *history = SharedHistory(getThreadsOnNode(numThreads, nodeIdx));
                sharedHistories[i] = history;
            }
        }
#endif

        // Standard allocation when NUMA is not used
        if (networkWeights.empty()) {
            networkWeights.resize(1);
            networkWeights[0] = globalNetworkData;
        }

        if (sharedHistories.empty()) {
            sharedHistories.resize(1);
            SharedHistory* history = reinterpret_cast<SharedHistory*>(alignedAlloc(64, sizeof(SharedHistory)));
            *history = SharedHistory(numThreads);
            sharedHistories[0] = history;
        }

        threads.clear();
        workers.clear();
        workers.resize(numThreads);

        startedThreads = 0;

        for (size_t i = 0; i < numThreads; i++) {
            threads.push_back(std::make_unique<std::thread>([this, i]() {
                configureThreadBinding(i);
                int nodeIdx = getNumaNode(i);
                workers[i] = std::make_unique<Worker>(this, networkWeights[nodeIdx], sharedHistories[nodeIdx], i);
                workers[i]->idle();
            }));
        }

        while (startedThreads < numThreads) {}
    }

    void startSearching(Board board, std::vector<Hash> boardHistory, SearchParameters parameters) {
        stopSearching();
        waitForSearchFinished();

        rootBoard = std::move(board);
        rootBoardHistory = std::move(boardHistory);
        searchParameters = std::move(parameters);

        for (auto& worker : workers) {
            worker.get()->rootMoves.clear();
            worker.get()->stopped.store(false, std::memory_order_relaxed);
            std::lock_guard<std::mutex> lock(worker.get()->mutex);
            worker.get()->searching = true;
        }

        for (auto& worker : workers) {
            worker.get()->cv.notify_all();
        }
    }

    void stopSearching() {
        for (auto& worker : workers) {
            worker.get()->stopped.store(true, std::memory_order_relaxed);
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
            sum += worker.get()->searchData.nodesSearched.load(std::memory_order_relaxed);
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