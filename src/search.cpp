#include <cstdint>
#include <inttypes.h>

#include "search.h"
#include "board.h"
#include "move.h"

uint64_t perft(Board* board, int depth, int startDepth) {
    if (depth == 0) return C64(1);

    char string[5];
    BoardStack stack;

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