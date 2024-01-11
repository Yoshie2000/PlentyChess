#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>

#include "board.h"
#include "search.h"

class Thread {

    bool exiting = false;
    bool searching = true;
    std::thread thread;
    std::mutex mutex;
    std::condition_variable cv;

    Board rootBoard;
    BoardStack rootStack;
    std::deque<BoardStack> rootStackQueue;

public:

    SearchParameters searchParameters;
    SearchData searchData;

    Thread(void);
    ~Thread();

    void startSearching(Board board, std::deque<BoardStack> stackQueue, SearchParameters parameters);

    void stopSearching();

    void waitForSearchFinished();

    void exit();

private:

    void idle();
    void tsearch();

};