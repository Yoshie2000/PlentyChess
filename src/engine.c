#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include <pthread.h>
#include <stdlib.h>

#include "engine.h"
#include "board.h"
#include "move.h"
#include "types.h"
#include "uci.h"

pthread_t searchThread;
pthread_cond_t searchCond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t searchMutex = PTHREAD_MUTEX_INITIALIZER;
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

void* runEngine() {
    printf("Engine thread running\n");

    while (true) {
        if (pthread_mutex_lock(&searchMutex) != 0) {
            perror("pthread_mutex_lock() error");
            exit(6);
        }
        if (pthread_cond_wait(&searchCond, &searchMutex) != 0) {
            perror("pthread_cond_timedwait() error");
            exit(7);
        }
        if (pthread_mutex_unlock(&searchMutex) != 0) {
            perror("pthread_mutex_unlock() error");
            exit(8);
        }

        if (searchStopped)
            break;

        printf("Pretending to search...\n");
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
    return NULL;
}

int main() {
    if (pthread_mutex_init(&searchMutex, NULL) != 0) {
        perror("pthread_mutex_init() error");
        exit(1);
    }

    if (pthread_cond_init(&searchCond, NULL) != 0) {
        perror("pthread_cond_init() error");
        exit(2);
    }

    if (pthread_create(&searchThread, NULL, runEngine, NULL) != 0) {
        perror("pthread_create() error");
        exit(3);
    }

    uciLoop();

    if (pthread_join(searchThread, NULL) != 0) {
        perror("pthread_join() error");
        exit(5);
    }

    return 0;
}