#include <stdio.h>
#include <inttypes.h>
#include <time.h>

#include "engine.h"
#include "board.h"
#include "move.h"
#include "types.h"

int main() {
    runEngine();

    return 0;
}

uint64_t perft(struct Board* board, int depth, int startDepth) {
    if (depth == 0) return C64(1);

    char string[4];
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
            printf("Move %s: %" PRIu64 "\n", string, subNodes);
        }

        nodes += subNodes;
    }
    return nodes;
}

void runEngine() {
    printf("\nEngine is starting\n");

    struct Board board;
    struct BoardStack stack;
    board.stack = &stack;
    startpos(&board);

    struct BoardStack newStack;

    //generateLastSqTable();

    clock_t begin = clock();
    uint64_t nodes = perft(&board, 6, 6);
    clock_t end = clock();
    double time = (double) (end - begin) / CLOCKS_PER_SEC;
    uint64_t nps = nodes / time;
    printf("Perft: %" PRIu64 " nodes in %fs => %" PRIu64 " nps\n", nodes, time, nps);

}