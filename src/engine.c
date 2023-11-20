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

    uint64_t nodes = 0;
    for (int i = 0; i < moveCount; i++) {
        Move move = moves[i];
        //printf("%s\n", string);
        //if (depth == 3 && i < 3) continue;
        //if (depth == 2 && i < 7) continue;

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
    // debugBoard(&board);

    struct BoardStack newStack;
    // debugBitboard(board.stack->attacked[COLOR_BLACK][PIECE_PAWN]);
    // doMove(&board, &newStack, stringToMove("c2c4"));
    // doMove(&board, &newStack, stringToMove("h7h6"));
    // debugBitboard(board.stack->attacked[COLOR_BLACK][PIECE_PAWN]);
    // doMove(&board, &newStack, stringToMove("a7a6"));
    // doMove(&board, &newStack, stringToMove("d1a4"));
    // e7e5: 1 extra
    // e7e6 c1a3: 1 extra

    //doMove(&board, &newStack, (Move) 0b011111001111); //h4
    //doMove(&board, &newStack, (Move) 0b100000110000); //a5
    //debugBoard(&board);
    // doMove(&board, &newStack, (Move) 0b011000001000); //a4
    // doMove(&board, &newStack, (Move) 0b100111110111); //h5
    // doMove(&board, &newStack, (Move) 0b100000011000); //a5
    // doMove(&board, &newStack, (Move) 0b101001110001); //b6
    // doMove(&board, &newStack, (Move) 0b101001100000); //axb6
    // undoMove(&board, (Move) 0b101001100000); //axb6
    // debugBoard(&board);
    // debugBitboard(board.byPiece[COLOR_WHITE][PIECE_PAWN]);
    // undoMove(&board, (Move) 0b011000001000);

    //generateLastSqTable();

    // Move moves[MAX_MOVES] = { MOVE_NULL };
    // generateMoves(&board, moves);

    // printf("Available moves:\n");
    // for (int i = 0; i < MAX_MOVES; i++) {
    //     Move move = moves[i];
    //     if (move == MOVE_NULL) break;

    //     char moveString[4];
    //     moveToString(moveString, move);
    //     printf("\tMove %d: %s\n", i + 1, moveString);
    // }

    clock_t begin = clock();
    uint64_t nodes = perft(&board, 6, 6);
    clock_t end = clock();
    double time = (double) (end - begin) / CLOCKS_PER_SEC;
    uint64_t nps = nodes / time;
    printf("Perft: %" PRIu64 " nodes in %fs => %" PRIu64 " nps\n", nodes, time, nps);

    /*Move move = moves[0];

    char moveString[4];
    moveToString(moveString, move);
    printf("doMove: %s\n", moveString);

    doMove(&board, move);
    debugBoard(&board);

    moveToString(moveString, move);
    printf("undoMove: %s\n", moveString);

    undoMove(&board, move);
    debugBoard(&board);*/

}