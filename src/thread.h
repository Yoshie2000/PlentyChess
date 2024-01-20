#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <functional>

#include "board.h"
#include "search.h"
#include "history.h"
#include "nnue.h"
#include "tt.h"

class Thread {

    std::thread thread;

    Board rootBoard;
    BoardStack* rootStack;
    std::deque<BoardStack>* rootStackQueue;

public:

    SearchData searchData;
    SearchParameters* searchParameters;
    History history;
    NNUE nnue;

    std::shared_ptr<std::function<void()>> stopSearchingPtr;
    std::shared_ptr<std::function<uint64_t()>> nodesSearchedPtr;
    int threadId;
    bool mainThread;

    Thread(std::shared_ptr<std::function<void()>> stopSearchingPtr, std::shared_ptr<std::function<uint64_t()>> nodesSearchedPtr, int threadId);
    ~Thread();

    void startSearching(Board board, std::deque<BoardStack>* stackQueue, SearchParameters* parameters);

    void stopSearching();

    void waitForSearchFinished();

    void exit();

    void ucinewgame();

private:

    void tsearch();

};

class ThreadPool {

    std::vector<std::unique_ptr<Thread>> threads;
    std::deque<BoardStack> rootStackQueue;
    SearchParameters searchParameters;

    std::shared_ptr<std::function<void()>> stopSearchingPtr;
    std::shared_ptr<std::function<uint64_t()>> nodesSearchedPtr;

public:

    ThreadPool(int numThreads) : threads(0), rootStackQueue(), searchParameters{},
        stopSearchingPtr(std::make_shared<std::function<void()>>(std::bind(&ThreadPool::stopSearching, this))),
        nodesSearchedPtr(std::make_shared<std::function<uint64_t()>>(std::bind(&ThreadPool::nodesSearched, this))) {
        for (int i = 0; i < numThreads; i++) {
            threads.push_back(std::make_unique<Thread>(stopSearchingPtr, nodesSearchedPtr, i));
        }
    }

    void resize(int numThreads) {
        exit();
        threads.resize(0);
        for (int i = 0; i < numThreads; i++) {
            threads.push_back(std::make_unique<Thread>(stopSearchingPtr, nodesSearchedPtr, i));
        }
    }

    void startSearching(Board board, std::deque<BoardStack> stackQueue, SearchParameters parameters) {
        waitForSearchFinished();

        rootStackQueue = std::move(stackQueue);
        searchParameters = std::move(parameters);

        for (auto& thread : threads) {
            thread.get()->startSearching(board, &rootStackQueue, &searchParameters);
        }
    }

    void stopSearching() {
        for (auto& thread : threads) {
            thread.get()->stopSearching();
        }
    }

    void waitForSearchFinished() {
        for (auto& thread : threads) {
            thread.get()->waitForSearchFinished();
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