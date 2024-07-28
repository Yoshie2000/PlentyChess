#include "thread.h"

bool playRandomMoves(Board* board, NNUE* nnue, int remainingMoves) {
    if (remainingMoves == 0) {
        std::cout << "info string genfens " << board->fen() << std::endl;
        return true;
    }

    BoardStack boardStack;
    Move moves[MAX_MOVES] = { MOVE_NONE };
    int m = 0;
    generateMoves(board, moves, &m);
    std::vector<Move> legalMoves;
    for (int i = 0; i < m; i++) {
        if (board->isLegal(moves[i])) {
            legalMoves.push_back(moves[i]);
        }
    }
    if (legalMoves.empty())
        return false;
    Move move = legalMoves[std::rand() % legalMoves.size()];
    board->doMove(&boardStack, move, board->hashAfter(move), nnue);

    return playRandomMoves(board, nnue, remainingMoves - 1);
}

void Thread::tgenfens() {
    std::srand(searchParameters->genfensSeed);

    int generatedFens = 0;
    while (generatedFens < searchParameters->genfensFens) {

        // Set up a fresh board
        Board board = rootBoard;
        BoardStack boardStack = *rootBoard.stack;
        board.stack = &boardStack;
        nnue.reset(&board);

        // Play 6-9 random moves
        int randomMoves = 6 + std::rand() % 4;
        if (playRandomMoves(&board, &nnue, randomMoves)) {
            generatedFens++;
        }
    }
}