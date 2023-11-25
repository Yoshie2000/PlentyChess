#ifndef THREAD_H_INCLUDED
#define THREAD_H_INCLUDED

#include <thread>
#include <mutex>
#include <condition_variable>

#include "board.h"

class Thread {

    std::thread thread;
    std::mutex mutex;
    std::condition_variable cv;
    bool exiting;
    bool searching;

    Board board;

public:

    Thread(void);
    ~Thread();

    void startSearching(Board* board);
    void exit();

private:

    void idle();

};

#endif