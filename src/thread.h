#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>

#include "board.h"
#include "search.h"
#include "history.h"
#include "nnue.h"
#include "tt.h"

struct RootMove {
    Eval value = -EVAL_INFINITE;
    int depth = 0;
    int selDepth = 0;
    Move move = MOVE_NULL;
    std::vector<Move> pv;
};

class ThreadPool;

class Thread {

    std::thread thread;
    std::mutex mutex;

    Board rootBoard;
    BoardStack* rootStack;
    std::deque<BoardStack>* rootStackQueue;

public:

    std::condition_variable cv;

    bool searching = false;
    bool stopped = false;
    bool exiting = false;

    SearchData searchData;
    SearchParameters* searchParameters;
    History history;
    NNUE nnue;

    ThreadPool* threadPool;

    int threadId;
    bool mainThread;

    std::vector<RootMove> rootMoves;
    std::map<Move, uint64_t> rootMoveNodes;
    std::vector<Move> excludedRootMoves;

    Thread(ThreadPool* threadPool, int threadId);

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
    void printUCI(Thread* thread, int multiPvCount = 1);
    Thread* chooseBestThread();

    template <NodeType nt>
    Eval search(Board* board, SearchStack* stack, int depth, Eval alpha, Eval beta, bool cutNode);

    template <NodeType nodeType>
    Eval qsearch(Board* board, SearchStack* stack, Eval alpha, Eval beta);

};

class ThreadPool {

public:

    std::vector<std::unique_ptr<Thread>> threads;

    std::deque<BoardStack> rootStackQueue;
    SearchParameters searchParameters;
    Board rootBoard;

    ThreadPool() : threads(0), rootStackQueue(), searchParameters{} {
        resize(1);
    }

    ~ThreadPool() {
        exit();
    }

    void resize(int numThreads) {
        exit();
        threads.clear();
        for (int i = 0; i < numThreads; i++) {
            threads.push_back(std::make_unique<Thread>(this, i));
        }
    }

    void startSearching(Board board, std::deque<BoardStack> stackQueue, SearchParameters parameters) {
        stopSearching();
        waitForSearchFinished();

        rootBoard = std::move(board);
        rootStackQueue = std::move(stackQueue);
        searchParameters = std::move(parameters);

        for (auto& thread : threads) {
            thread.get()->rootMoves.clear();
            thread.get()->stopped = false;
            thread.get()->searching = true;
        }

        for (auto& thread : threads) {
            thread.get()->cv.notify_all();
        }
    }

    void stopSearching() {
        for (auto& thread : threads) {
            thread.get()->stopped = true;
        }
    }

    void waitForSearchFinished() {
        for (auto& thread : threads) {
            thread.get()->waitForSearchFinished();
        }
    }

    void waitForHelpersFinished() {
        for (std::size_t i = 1; i < threads.size(); i++) {
            threads[i].get()->waitForSearchFinished();
        }
    }

    void exit() {
        for (auto& thread : threads) {
            thread.get()->exit();
        }
    }

    void ucinewgame() {
        TT.clear();
        for (auto& thread : threads) {
            thread.get()->ucinewgame();
        }
    }

    uint64_t nodesSearched() {
        uint64_t sum = 0;
        for (auto& thread : threads) {
            sum += thread.get()->searchData.nodesSearched;
        }
        return sum;
    }

};