#ifndef SEARCH_H_INCLUDED
#define SEARCH_H_INCLUDED

#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <inttypes.h>

#include "board.h"
#include "move.h"

uint64_t perft(Board* board, int depth, int startDepth);

#endif