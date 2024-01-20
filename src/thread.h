#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>

#include "board.h"
#include "search.h"
#include "history.h"
#include "nnue.h"

class Thread {

    bool exiting = false;
    bool searching = true;
    std::thread thread;
    std::mutex mutex;
    std::condition_variable cv;

    Board rootBoard;
    BoardStack* rootStack;
    std::deque<BoardStack>* rootStackQueue;

public:

    SearchData searchData;
    SearchParameters* searchParameters;
    History history;
    NNUE nnue;

    Thread(void);
    ~Thread();

    void startSearching(Board board, std::deque<BoardStack>* stackQueue, SearchParameters* parameters);

    void stopSearching();

    void waitForSearchFinished();

    void exit();

private:

    void idle();
    void tsearch();

};

class ThreadPool {

    std::vector<Thread> threads;
    std::deque<BoardStack> rootStackQueue;
    SearchParameters searchParameters;

public:

    ThreadPool(int numThreads) : threads(numThreads), rootStackQueue(), searchParameters {} {}

    void startSearching(Board board, std::deque<BoardStack> stackQueue, SearchParameters parameters) {
        rootStackQueue = std::move(stackQueue);
        searchParameters = std::move(parameters);

        for (Thread& thread : threads) {
            thread.startSearching(board, &rootStackQueue, &searchParameters);
        }
    }

    void stopSearching() {
        for (Thread& thread : threads) {
            thread.stopSearching();
        }
    }

    void waitForSearchFinished() {
        for (Thread& thread : threads) {
            thread.waitForSearchFinished();
        }
    }

    void exit() {
        for (Thread& thread : threads) {
            thread.exit();
        }
    }

    uint64_t nodesSearched() {
        uint64_t sum = 0;
        for (Thread& thread : threads) {
            sum += thread.searchData.nodesSearched;
        }
        return sum;
    }

};