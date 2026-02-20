#include <iostream>
#include <cassert>

#include "types.h"
#include "board.h"
#include "evaluation.h"
#include "magic.h"
#include "nnue.h"
#include "spsa.h"

TUNE_INT(materialScalePawnValue, 91, 1, 200);
TUNE_INT(materialScaleKnightValue, 310, 1, 600);
TUNE_INT(materialScaleBishopValue, 315, 1, 600);
TUNE_INT(materialScaleRookValue, 478, 1, 1000);
TUNE_INT(materialScaleQueenValue, 1118, 1, 2000);

TUNE_INT(materialScaleBase, 854, 1, 2000);
TUNE_INT(materialScaleDivisor, 30, 1, 100);

int PIECE_VALUES[Piece::TOTAL + 1] = {
    96, 298, 301, 507, 909, 0, 0
};

constexpr int SEE_VALUES[Piece::TOTAL + 1] = {
    90, 290, 310, 570, 1000, 0, 0
};

int getMaterialScale(Board* board) {
    int pawnCount = BB::popcount(board->byPiece[Piece::PAWN]);
    int knightCount = BB::popcount(board->byPiece[Piece::KNIGHT]);
    int bishopCount = BB::popcount(board->byPiece[Piece::BISHOP]);
    int rookCount = BB::popcount(board->byPiece[Piece::ROOK]);
    int queenCount = BB::popcount(board->byPiece[Piece::QUEEN]);

    int materialValue = materialScalePawnValue * pawnCount + materialScaleKnightValue * knightCount + materialScaleBishopValue * bishopCount + materialScaleRookValue * rookCount + materialScaleQueenValue * queenCount;
    return materialScaleBase + materialValue / materialScaleDivisor;
}

Eval evaluate(Board* board, NNUE* nnue) {
    assert(!board->checkers);

    Eval eval = nnue->evaluate(board);    
    eval = (eval * getMaterialScale(board)) / 1024;

    eval = std::clamp((int)eval, (int)-EVAL_TBWIN_IN_MAX_PLY + 1, (int)EVAL_TBWIN_IN_MAX_PLY - 1);
    return (eval / 16) * 16;
}

std::string formatEval(Eval value) {
    std::string evalString;
    if (value >= EVAL_MATE_IN_MAX_PLY) {
        evalString = "mate " + std::to_string((EVAL_MATE - value) / 2 + 1);
    }
    else if (value <= -EVAL_MATE_IN_MAX_PLY) {
        evalString = "mate " + std::to_string(-(EVAL_MATE + value) / 2);
    }
    else if (value >= EVAL_TBWIN_IN_MAX_PLY) {
        evalString = "cp " + std::to_string(1000 * 100 - ((EVAL_TBWIN - value) / 2 + 1) * 100);
    }
    else if (value <= -EVAL_TBWIN_IN_MAX_PLY) {
        evalString = "cp " + std::to_string(-1000 * 100 + ((EVAL_TBWIN + value) / 2) * 100);
    }
    else {
        evalString = "cp " + std::to_string(100 * value / 253);
    }
    return evalString;
}

bool SEE(Board* board, Move move, int threshold) {
    assert(board->isPseudoLegal(move));

    // "Special" moves pass SEE
    if (move.isSpecial())
        return true;

    Square origin = move.origin();
    Square target = move.target();

    // If winning the piece on the target square (if there is one) for free doesn't pass the threshold, fail
    int value = SEE_VALUES[board->pieces[target]] - threshold;
    if (value < 0) return false;

    // If we beat the threshold after losing our piece, pass
    value -= SEE_VALUES[board->pieces[origin]];
    if (value >= 0) return true;

    Bitboard occupied = (board->byColor[Color::WHITE] | board->byColor[Color::BLACK]) ^ (bitboard(origin));
    Bitboard attackersToTarget = board->attackersTo(target, occupied);

    Bitboard bishops = board->byPiece[Piece::BISHOP] | board->byPiece[Piece::QUEEN];
    Bitboard rooks = board->byPiece[Piece::ROOK] | board->byPiece[Piece::QUEEN];

    Color side = flip(board->stm);

    Square whiteKing = lsb(board->byColor[Color::WHITE] & board->byPiece[Piece::KING]);
    Square blackKing = lsb(board->byColor[Color::BLACK] & board->byPiece[Piece::KING]);
    Bitboard blockers = board->blockers[Color::WHITE] | board->blockers[Color::BLACK];
    Bitboard alignedBlockers = (board->blockers[Color::WHITE] & BB::LINE[target][whiteKing]) | (board->blockers[Color::BLACK] & BB::LINE[target][blackKing]);

    // Make captures until one side has none left / fails to beat the threshold
    while (true) {

        // Remove attackers that have been captured
        attackersToTarget &= occupied;

        // No attackers left for the current side
        Bitboard stmAttackers = board->byColor[side] & attackersToTarget & (~blockers | alignedBlockers);
        if (!stmAttackers) break;

        // Find least valuable piece
        Piece piece;
        for (piece = Piece::PAWN; piece < Piece::KING; ++piece) {
            // If the piece attacks the target square, stop
            if (stmAttackers & board->byPiece[piece])
                break;
        }

        side = flip(side);
        value = -value - 1 - SEE_VALUES[piece];

        // Value beats (or can't beat) threshold (negamax)
        if (value >= 0) {
            if (piece == Piece::KING && (attackersToTarget & board->byColor[side]))
                side = flip(side);
            break;
        }

        // Remove the used piece
        Square pieceSquare = lsb(stmAttackers & board->byPiece[piece]);
        occupied ^= bitboard(pieceSquare);
        blockers &= ~bitboard(pieceSquare);
        alignedBlockers &= ~bitboard(pieceSquare);

        // Add discovered attacks
        if (piece == Piece::PAWN || piece == Piece::BISHOP || piece == Piece::QUEEN)
            attackersToTarget |= getBishopMoves(target, occupied) & bishops;
        if (piece == Piece::ROOK || piece == Piece::QUEEN)
            attackersToTarget |= getRookMoves(target, occupied) & rooks;
    }

    return side != board->stm;
}
