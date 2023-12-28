#include <iostream>
#include <inttypes.h>
#include <deque>
#include <string.h>

#include "thread.h"
#include "search.h"

Thread::Thread(void) : thread(&Thread::idle, this) {
    waitForSearchFinished();
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

        rootBoard.stopSearching = false;
        searching = true;

        // Do the search stuff here
        if (searchParameters.perft) {
            nodesSearched = perft(&rootBoard, searchParameters.depth);
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

void Thread::startSearching(Board board, std::deque<BoardStack> queue, SearchParameters parameters) {
    rootBoard = std::move(board);
    rootStackQueue = std::move(queue);
    searchParameters = std::move(parameters);

    rootStack = rootStackQueue.back();
    rootBoard.stack = &rootStack;

    mutex.lock();
    searching = true;
    mutex.unlock();
    cv.notify_one();
}

void Thread::stopSearching() {
    rootBoard.stopSearching = true;
}

void Thread::waitForSearchFinished() {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&] { return !searching; });
}