#ifndef THREAD_H_INCLUDED
#define THREAD_H_INCLUDED

#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>

#include "board.h"
#include "search.h"

class Thread {

    std::thread thread;
    std::mutex mutex;
    std::condition_variable cv;
    bool exiting;
    bool searching;

    Board rootBoard;
    BoardStack rootStack;
    std::deque<BoardStack> rootStackQueue;
    SearchParameters searchParameters;

public:

    Thread(void);
    ~Thread();

    void startSearching(Board board, std::deque<BoardStack> stackQueue, SearchParameters parameters);

    void stopSearching();

    void exit();

private:

    void idle();
    void tsearch();

};

#endif