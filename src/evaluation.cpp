#include <iostream>
#include <cassert>

#include "types.h"
#include "board.h"
#include "evaluation.h"
#include "magic.h"
#include "nnue.h"
#include "spsa.h"
#include "uci.h"

TUNE_INT_DISABLED(pawnValue, 96, 50, 150);
TUNE_INT_DISABLED(knightValue, 298, 200, 400);
TUNE_INT_DISABLED(bishopValue, 301, 200, 400);
TUNE_INT_DISABLED(rookValue, 507, 400, 600);
TUNE_INT_DISABLED(queenValue, 909, 700, 1100);

TUNE_INT_DISABLED(materialScaleBase, 933, 512, 1536);
TUNE_INT_DISABLED(materialScaleDivisor, 48, 32, 64);

Eval fakePiece = 0;

Eval PIECE_VALUES[Piece::TOTAL + 1] = {
    pawnValue, knightValue, bishopValue, rookValue, queenValue, fakePiece, fakePiece
};

constexpr Eval SEE_VALUES[Piece::TOTAL + 1] = {
    90, 290, 310, 570, 1000, 0, 0
};


int getMaterialScale(Board* board) {
    int knightCount = board->stack->pieceCount[Color::WHITE][Piece::KNIGHT] + board->stack->pieceCount[Color::BLACK][Piece::KNIGHT];
    int bishopCount = board->stack->pieceCount[Color::WHITE][Piece::BISHOP] + board->stack->pieceCount[Color::BLACK][Piece::BISHOP];
    int rookCount = board->stack->pieceCount[Color::WHITE][Piece::ROOK] + board->stack->pieceCount[Color::BLACK][Piece::ROOK];
    int queenCount = board->stack->pieceCount[Color::WHITE][Piece::QUEEN] + board->stack->pieceCount[Color::BLACK][Piece::QUEEN];

    int materialValue = PIECE_VALUES[Piece::KNIGHT] * knightCount + PIECE_VALUES[Piece::BISHOP] * bishopCount + PIECE_VALUES[Piece::ROOK] * rookCount + PIECE_VALUES[Piece::QUEEN] * queenCount;
    return materialScaleBase + materialValue / materialScaleDivisor;
}

Eval evaluate(Board* board, NNUE* nnue) {
    assert(!board->stack->checkers);

    Eval eval = nnue->evaluate(board);

    if (!UCI::Options.datagen.value) {
        eval = (eval * getMaterialScale(board)) / 1024;
        eval = eval * (300 - board->stack->rule50_ply) / 300;
    }

    eval = std::clamp((int) eval, (int) -EVAL_TB_WIN_IN_MAX_PLY + 1, (int) EVAL_TB_WIN_IN_MAX_PLY - 1);
    return eval;
}

std::string formatEval(Eval value) {
    std::string evalString;
    if (value >= EVAL_MATE_IN_MAX_PLY) {
        evalString = "mate " + std::to_string((EVAL_MATE - value) / 2 + 1);
    }
    else if (value <= -EVAL_MATE_IN_MAX_PLY) {
        evalString = "mate " + std::to_string(-(EVAL_MATE + value) / 2);
    }
    else {
        Eval outputEval = value;
        if (!UCI::Options.datagen.value)
            outputEval = 100 * outputEval / 220;
        evalString = "cp " + std::to_string(outputEval);
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

    // Make captures until one side has none left / fails to beat the threshold
    while (true) {

        // Remove attackers that have been captured
        attackersToTarget &= occupied;

        // No attackers left for the current side
        Bitboard stmAttackers = board->byColor[side] & attackersToTarget;
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

        // Add discovered attacks
        if (piece == Piece::PAWN || piece == Piece::BISHOP || piece == Piece::QUEEN)
            attackersToTarget |= getBishopMoves(target, occupied) & bishops;
        if (piece == Piece::ROOK || piece == Piece::QUEEN)
            attackersToTarget |= getRookMoves(target, occupied) & rooks;
    }

    return side != board->stm;
}
