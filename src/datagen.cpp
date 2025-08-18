#include "thread.h"

std::vector<Move> generateLegalMoves(Board& board) {
    Move moves[MAX_MOVES] = { MOVE_NONE };
    int m = 0;
    generateMoves(&board, moves, &m);
    std::vector<Move> legalMoves;
    for (int i = 0; i < m; i++) {
        if (board.isLegal(moves[i])) {
            legalMoves.push_back(moves[i]);
        }
    }
    return legalMoves;
}

bool playRandomMoves(Board& board, Worker* thread, int remainingMoves) {
    std::vector<Move> legalMoves = generateLegalMoves(board);

    if (legalMoves.empty())
        return false;

    if (remainingMoves == 0) {

        // Do a verification search that this position isn't completely busted
        thread->ucinewgame();
        thread->rootMoves.clear();
        thread->searchParameters.depth = 10;
        thread->searchParameters.nodes = 1000000;
        thread->rootBoard = board;
        thread->startSearching(true);

        Eval verificationScore = thread->rootMoves[0].value;

        if (std::abs(verificationScore) >= 1000)
            return false;
        
        std::cout << "info string genfens " << board.fen() << std::endl;
        return true;
    }

    Move move = MOVE_NONE;
    while (move == MOVE_NONE) {
        int r = std::rand() % 100;
        Piece randomPiece = r < 35 ? Piece::PAWN : r < 50 ? Piece::KNIGHT : r < 65 ? Piece::BISHOP : r < 80 ? Piece::QUEEN : r < 95 ? Piece::KING : Piece::ROOK;
        std::vector<Move> pieceMoves;
        for (Move m : legalMoves) {
            if (board.pieces[moveOrigin(m)] == randomPiece)
                pieceMoves.push_back(m);
        }
        if (!pieceMoves.empty()) {
            move = pieceMoves[std::rand() % pieceMoves.size()];
        }
    }
    Board boardCopy = board;
    boardCopy.doMove(move, boardCopy.hashAfter(move), &thread->nnue);

    return playRandomMoves(boardCopy, thread, remainingMoves - 1);
}

void Worker::genfens() {
    std::srand(searchParameters.genfensSeed);

    int generatedFens = 0;
    while (generatedFens < searchParameters.genfensFens) {

        // Set up a fresh board
        Board board;
        board.startpos();
        nnue.reset(&board);

        // Play 6-9 random moves
        int randomMoves = 6 + std::rand() % 4;
        if (playRandomMoves(board, this, randomMoves)) {
            generatedFens++;
        }
    }
}