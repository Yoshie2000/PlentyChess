#include <iostream>
#include <cassert>

#include "types.h"
#include "board.h"
#include "evaluation.h"
#include "magic.h"
#include "nnue.h"
#include "spsa.h"

TUNE_INT_DISABLED(pawnValue, 96, 50, 150);
TUNE_INT_DISABLED(knightValue, 298, 200, 400);
TUNE_INT_DISABLED(bishopValue, 301, 200, 400);
TUNE_INT_DISABLED(rookValue, 507, 400, 600);
TUNE_INT_DISABLED(queenValue, 909, 700, 1100);

TUNE_INT_DISABLED(materialScaleBase, 920, 512, 1536);
TUNE_INT_DISABLED(materialScaleDivisor, 48, 32, 64);

Eval fakePiece = 0;

Eval PIECE_VALUES[Piece::TOTAL + 1] = {
    pawnValue, knightValue, bishopValue, rookValue, queenValue, fakePiece, fakePiece
};

constexpr Eval SEE_VALUES[Piece::TOTAL + 1] = {
    90, 290, 310, 570, 1000, 0, 0
};


int getMaterialScale(Board* board) {
    int pawnCount = BB::popcount(board->byPiece[Piece::PAWN]);
    int knightCount = BB::popcount(board->byPiece[Piece::KNIGHT]);
    int bishopCount = BB::popcount(board->byPiece[Piece::BISHOP]);
    int rookCount = BB::popcount(board->byPiece[Piece::ROOK]);
    int queenCount = BB::popcount(board->byPiece[Piece::QUEEN]);

    int materialValue = PIECE_VALUES[Piece::PAWN] * pawnCount + PIECE_VALUES[Piece::KNIGHT] * knightCount + PIECE_VALUES[Piece::BISHOP] * bishopCount + PIECE_VALUES[Piece::ROOK] * rookCount + PIECE_VALUES[Piece::QUEEN] * queenCount;
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
        evalString = "cp " + std::to_string(100 * value / 312);
    }
    return evalString;
}

bool SEE(Board* board, Move move, Eval threshold) {
    assert(board->isPseudoLegal(move));

    // "Special" moves pass SEE
    if (move >> 12)
        return true;

    Square origin = moveOrigin(move);
    Square target = moveTarget(move);

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
