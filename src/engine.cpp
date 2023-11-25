#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include <stdlib.h>
#include <thread>
#include <condition_variable>
#include <mutex>

#include "engine.h"
#include "board.h"
#include "move.h"
#include "types.h"
#include "uci.h"

std::thread searchThread;
std::mutex searchMutex;
std::condition_variable searchCv;
bool searchStopped = false;

uint64_t perft(struct Board* board, int depth, int startDepth) {
    if (depth == 0) return C64(1);

    char string[5];
    struct BoardStack stack;

    Move moves[MAX_MOVES] = { MOVE_NULL };
    int moveCount = 0;
    generateMoves(board, moves, &moveCount);
    // if (depth == 1) return (uint64_t) moveCount; // does not work yet, because i don't have legal movegen

    uint64_t nodes = 0;
    for (int i = 0; i < moveCount; i++) {
        Move move = moves[i];

        doMove(board, &stack, move);

        // This move was illegal, we remain in check after
        if (isInCheck(board, 1 - board->stm)) {
            undoMove(board, move);
            continue;
        }

        uint64_t subNodes = perft(board, depth - 1, startDepth);

        undoMove(board, move);

        if (depth == startDepth) {
            moveToString(string, move);
            printf("%s: %" PRIu64 "\n", string, subNodes);
        }

        nodes += subNodes;
    }
    return nodes;
}

void runEngine() {
    printf("Engine thread running\n");

    while (true) {

        std::unique_lock<std::mutex> lock(searchMutex);
        searchCv.wait(lock);
        lock.unlock();

        if (searchStopped)
            break;

        // printf("Pretending to search...\n");

        struct Board board;
        struct BoardStack stack, newStack;
        board.stack = &stack;
        startpos(&board);

        // 2k2Q2/ppp5/4p3/b7/8/1K5p/P4r2/8 b - - 0 32
        // r3k1r1/pp3p1p/6q1/2p1nN2/1N2p1B1/4P3/PPP2PPP/R2Q1RK1 w q - 3 15
        // rn1qk1nr/ppp1ppbp/6p1/3p1b2/3P3P/4PN2/PPP2PP1/RNBQKB1R w KQkq - 1 5

        parseFen(&board, "rn1qk1nr/ppp1ppbp/6p1/3p1b2/3P3P/4PN2/PPP2PP1/RNBQKB1R w KQkq - 1 5");
        debugBoard(&board);
        // generateLastSqTable();

        clock_t begin = clock();
        uint64_t nodes = perft(&board, 5, 5);
        clock_t end = clock();
        double time = (double)(end - begin) / CLOCKS_PER_SEC;
        uint64_t nps = nodes / time;
        printf("Perft: %" PRIu64 " nodes in %fs => %" PRIu64 " nps\n", nodes, time, nps);

    }
    printf("Engine thread stopping\n");

    // struct Board board;
    // struct BoardStack stack, newStack;
    // board.stack = &stack;
    // startpos(&board);

    // 2k2Q2/ppp5/4p3/b7/8/1K5p/P4r2/8 b - - 0 32
    // r3k1r1/pp3p1p/6q1/2p1nN2/1N2p1B1/4P3/PPP2PPP/R2Q1RK1 w q - 3 15
    // rn1qk1nr/ppp1ppbp/6p1/3p1b2/3P3P/4PN2/PPP2PP1/RNBQKB1R w KQkq - 1 5

    // parseFen(&board, "rn1qk1nr/ppp1ppbp/6p1/3p1b2/3P3P/4PN2/PPP2PP1/RNBQKB1R w KQkq - 1 5");
    // debugBoard(&board);
    //generateLastSqTable();

    // clock_t begin = clock();
    // uint64_t nodes = perft(&board, 6, 6);
    // clock_t end = clock();
    // double time = (double)(end - begin) / CLOCKS_PER_SEC;
    // uint64_t nps = nodes / time;
    // printf("Perft: %" PRIu64 " nodes in %fs => %" PRIu64 " nps\n", nodes, time, nps);
}

int main() {

    searchThread = std::thread(runEngine);

    uciLoop();

    searchThread.join();

    return 0;
}