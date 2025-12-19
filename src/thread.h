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

inline int getNumaNode(int threadId) {
#ifdef USE_NUMA
    if (!shouldConfigureNuma()) {
        return 0;
    }
    return threadId % numa_num_configured_nodes();
#else
    (void) threadId;
    return 0;
#endif
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
    std::vector<std::array<MoveGen, 2>> movepickers;

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

static_assert(sizeof(Worker) % 64 == 0);

class ThreadPool {

public:

    std::vector<std::unique_ptr<Worker>> workers;
    std::vector<std::unique_ptr<std::thread>> threads;

    SearchParameters searchParameters;
    Board rootBoard;
    std::vector<Hash> rootBoardHistory;

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
        if (shouldConfigureNuma()) {
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
        if (shouldConfigureNuma()) {

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

    void startSearching(Board board, std::vector<Hash> boardHistory, SearchParameters parameters) {
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