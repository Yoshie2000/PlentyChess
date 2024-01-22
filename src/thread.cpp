#include <iostream>
#include <inttypes.h>
#include <deque>
#include <string.h>

#include "thread.h"
#include "search.h"

Thread::Thread(void) {}

Thread::~Thread() {
    exit();
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

void Thread::exit() {
    stopSearching();
    waitForSearchFinished();
}

void Thread::stopSearching() {
    searchData.stopSearching = true;
}

void Thread::waitForSearchFinished() {
    if (thread.joinable())
        thread.join();
}