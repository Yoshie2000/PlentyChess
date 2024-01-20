#include <iostream>
#include <inttypes.h>
#include <deque>
#include <string.h>

#include "thread.h"
#include "search.h"
#include "history.h"

Thread::Thread(void) : thread(&Thread::idle, this) {
    history.initHistory();
    nnue.initNetwork();
}

Thread::~Thread() {
    exit();
    thread.join();
}

void Thread::idle() {
    printf("Engine thread running\n");

    while (true) {

        std::unique_lock<std::mutex> lock(mutex);
        searching = false;
        cv.notify_one();
        cv.wait(lock, [&] { return searching; });

        if (exiting)
            break;

        lock.unlock();

        searchData.stopSearching = false;
        searching = true;

        // Do the search stuff here
        if (searchParameters->perft) {
            searchData.nodesSearched = perft(&rootBoard, searchParameters->depth);
        }
        else {
            tsearch();
        }

        if (exiting)
            break;
    }
    printf("Engine thread stopping\n");
}

void Thread::exit() {
    exiting = true;
    stopSearching();
    mutex.lock();
    searching = true;
    mutex.unlock();
    cv.notify_one();
}

void Thread::startSearching(Board board, std::deque<BoardStack>* queue, SearchParameters* parameters) {
    memcpy(&rootBoard, &board, sizeof(Board));
    rootStackQueue = queue;
    searchParameters = parameters;

    rootStack = &rootStackQueue->back();
    rootBoard.stack = rootStack;

    mutex.lock();
    searching = true;
    mutex.unlock();
    cv.notify_one();
}

void Thread::stopSearching() {
    searchData.stopSearching = true;
}

void Thread::waitForSearchFinished() {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&] { return !searching; });
}