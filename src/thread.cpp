#include <iostream>
#include <inttypes.h>
#include <deque>
#include <string.h>
#include <mutex>
#include <condition_variable>

#include "thread.h"
#include "search.h"
#include "history.h"

Thread::Thread(ThreadPool* threadPool, int threadId) : thread(std::thread(&Thread::idle, this)), threadPool(threadPool), threadId(threadId), mainThread(threadId == 0) {
    history.initHistory();
}

void Thread::startSearching() {
    memcpy(&rootBoard, &threadPool->rootBoard, sizeof(Board));
    rootStackQueue = &threadPool->rootStackQueue;
    searchParameters = &threadPool->searchParameters;

    rootStack = &rootStackQueue->back();
    rootBoard.stack = rootStack;

    tsearch();
}

void Thread::waitForSearchFinished() {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&] { return !searching; });
}

void Thread::idle() {
    while (!exiting) {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&] { return searching; });

        if (exiting)
            return;

        startSearching();
        searching = false;

        cv.notify_all();
    }
}

void Thread::exit() {
    searching = true;
    exiting = true;
    cv.notify_all();
    if (thread.joinable())
        thread.join();
}

void Thread::ucinewgame() {
    history.initHistory();
}