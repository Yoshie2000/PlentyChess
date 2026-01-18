#include <iostream>
#include <inttypes.h>
#include <deque>
#include <string.h>
#ifndef ARCH_WASM
#include <mutex>
#include <condition_variable>
#endif

#include "thread.h"
#include "search.h"
#include "history.h"
#include "uci.h"

Worker::Worker(ThreadPool* _threadPool, NetworkData* _networkData, int _threadId) : nnue(_networkData), threadPool(_threadPool), threadId(_threadId), mainThread(threadId == 0) {
    history.initHistory();
}

void Worker::startSearching() {
    rootBoard = threadPool->rootBoard;
    boardHistory.clear();
    boardHistory.reserve(MAX_PLY + 1);
    for (Hash hash : threadPool->rootBoardHistory) {
        boardHistory.push_back(hash);
    }
    memcpy(&rootBoard, &threadPool->rootBoard, sizeof(Board));
    searchParameters = threadPool->searchParameters;

    if (searchParameters.perft)
        searchData.nodesSearched = perft(rootBoard, searchParameters.depth);
    else if (searchParameters.genfens)
        tgenfens();
#ifndef ARCH_WASM
    else if (UCI::Options.datagen.value)
        tdatagen();
#endif
    else
        tsearch();

#ifdef ARCH_WASM
    // In Wasm, mark search as finished immediately
    searching = false;
#endif
}

void Worker::waitForSearchFinished() {
#ifdef ARCH_WASM
    // In Wasm, search is synchronous, nothing to wait for
    (void)0;
#else
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&] { return !searching; });
#endif
}

void Worker::idle() {
#ifdef ARCH_WASM
    // In Wasm, we don't use the idle loop - search is called directly
    threadPool->startedThreads++;
#else
    threadPool->startedThreads++;

    while (!exiting) {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [&] { return searching; });

        if (exiting)
            return;

        startSearching();
        searching = false;

        cv.notify_all();
    }
#endif
}

void Worker::exit() {
#ifdef ARCH_WASM
    // In Wasm, nothing to do - no threads to join
    exiting = true;
#else
    {
        std::lock_guard<std::mutex> lock(mutex);
        searching = true;
        exiting = true;
    }
    cv.notify_all();
    if (threadPool->threads[threadId]->joinable())
        threadPool->threads[threadId]->join();
#endif
}

void Worker::ucinewgame() {
    history.initHistory();
}