#include <iostream>
#include <inttypes.h>

#include "thread.h"
#include "search.h"

// 2k2Q2/ppp5/4p3/b7/8/1K5p/P4r2/8 b - - 0 32
// r3k1r1/pp3p1p/6q1/2p1nN2/1N2p1B1/4P3/PPP2PPP/R2Q1RK1 w q - 3 15
// rn1qk1nr/ppp1ppbp/6p1/3p1b2/3P3P/4PN2/PPP2PP1/RNBQKB1R w KQkq - 1 5

Thread::Thread(void) : thread(&Thread::idle, this) {
    std::cout << "constructor" << std::endl;
}

Thread::~Thread() {
    thread.join();
}

void Thread::idle() {
    printf("Engine thread running\n");

    while (true) {

        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock);
        lock.unlock();

        if (exiting)
            break;

        searching = true;

        clock_t begin = clock();
        uint64_t nodes = perft(&board, 6, 6);
        clock_t end = clock();
        double time = (double)(end - begin) / CLOCKS_PER_SEC;
        uint64_t nps = nodes / time;
        printf("Perft: %" PRIu64 " nodes in %fs => %" PRIu64 " nps\n", nodes, time, nps);

        searching = false;

        if (exiting)
            break;
    }
    printf("Engine thread stopping\n");
}

void Thread::exit() {
    exiting = true;
    cv.notify_one();
}

void Thread::startSearching(Board* board) {
    this->board = *board;
    cv.notify_one();
}