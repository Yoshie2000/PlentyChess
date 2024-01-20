#include <iostream>
#include <inttypes.h>
#include <deque>
#include <string.h>

#include "thread.h"
#include "search.h"
#include "history.h"

Thread::Thread(std::shared_ptr<std::function<void()>> stopSearchingPtr, std::shared_ptr<std::function<uint64_t()>> nodesSearchedPtr, int threadId) : stopSearchingPtr(stopSearchingPtr), nodesSearchedPtr(nodesSearchedPtr), threadId(threadId), mainThread(threadId == 0) {
    history.initHistory();
    nnue.initNetwork();
}

Thread::~Thread() {
    exit();
}

void Thread::exit() {
    stopSearching();
    waitForSearchFinished();
}

void Thread::startSearching(Board board, std::deque<BoardStack>* queue, SearchParameters* parameters) {
    if (thread.joinable())
        return;

    memcpy(&rootBoard, &board, sizeof(Board));
    rootStackQueue = queue;
    searchParameters = parameters;

    rootStack = &rootStackQueue->back();
    rootBoard.stack = rootStack;
    searchData.stopSearching = false;

    thread = std::thread([this]() {
        // Do the search stuff here
        if (searchParameters->perft) {
            searchData.nodesSearched = perft(&rootBoard, searchParameters->depth);
        }
        else {
            tsearch();
        }
    });
}

void Thread::stopSearching() {
    searchData.stopSearching = true;
}

void Thread::waitForSearchFinished() {
    if (thread.joinable())
        thread.join();
}

void Thread::ucinewgame() {
    history.initHistory();
    nnue.initNetwork();
}