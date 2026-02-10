#include <iostream>
#include <inttypes.h>
#include <deque>
#include <string.h>
#include <mutex>
#include <condition_variable>

#include "thread.h"
#include "search.h"
#include "history.h"
#include "uci.h"

Worker::Worker(ThreadPool* _threadPool, NetworkData* _networkData, SharedHistory* _sharedHistory, int _threadId, int _threadIdOnNode) : history(_threadIdOnNode, _sharedHistory), nnue(_networkData), threadPool(_threadPool), threadId(_threadId), mainThread(threadId == 0) {
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
    else if (UCI::Options.datagen.value)
        tdatagen();
    else
        tsearch();
}

void Worker::waitForSearchFinished() {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&] { return !searching; });
}

void Worker::idle() {
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
}

void Worker::exit() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        searching = true;
        exiting = true;
    }
    cv.notify_all();
    if (threadPool->threads[threadId]->joinable())
        threadPool->threads[threadId]->join();
}

void Worker::ucinewgame() {
    history.initHistory();
}