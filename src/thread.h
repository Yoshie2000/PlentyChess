#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <atomic>

#include "board.h"
#include "search.h"
#include "history.h"
#include "nnue.h"
#include "tt.h"

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

    Worker(ThreadPool* threadPool, int threadId);

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

    ThreadPool() : workers(0), threads(0), searchParameters{}, rootBoardHistory() {
        resize(1);
    }

    ~ThreadPool() {
        exit();
    }

    void resize(size_t numThreads) {
        if (threads.size() == numThreads)
            return;
        
        exit();
        threads.clear();
        workers.clear();
        workers.resize(numThreads);

        startedThreads = 0;

        for (size_t i = 0; i < numThreads; i++) {
            threads.push_back(std::make_unique<std::thread>([this, i]() {
                workers[i] = std::make_unique<Worker>(this, i);
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