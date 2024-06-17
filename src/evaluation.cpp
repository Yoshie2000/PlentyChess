#include <iostream>
#include <cassert>

#include "types.h"
#include "board.h"
#include "evaluation.h"
#include "magic.h"
#include "nnue.h"

const Eval PIECE_VALUES[Piece::TOTAL + 1] = {
    100, 300, 300, 500, 900, 0, 0
};

const Eval SEE_VALUES[Piece::TOTAL + 1] = {
    90, 290, 310, 570, 1000, 0, 0
};

int getMaterialScale(Board* board) {
    int knightCount = board->stack->pieceCount[Color::WHITE][Piece::KNIGHT] + board->stack->pieceCount[Color::BLACK][Piece::KNIGHT];
    int bishopCount = board->stack->pieceCount[Color::WHITE][Piece::BISHOP] + board->stack->pieceCount[Color::BLACK][Piece::BISHOP];
    int rookCount = board->stack->pieceCount[Color::WHITE][Piece::ROOK] + board->stack->pieceCount[Color::BLACK][Piece::ROOK];
    int queenCount = board->stack->pieceCount[Color::WHITE][Piece::QUEEN] + board->stack->pieceCount[Color::BLACK][Piece::QUEEN];

    int materialValue = PIECE_VALUES[Piece::KNIGHT] * knightCount + PIECE_VALUES[Piece::BISHOP] * bishopCount + PIECE_VALUES[Piece::ROOK] * rookCount + PIECE_VALUES[Piece::QUEEN] * queenCount;
    return 980 + materialValue / 48;
}

Eval evaluate(Board* board, NNUE* nnue) {
    assert(!board->stack->checkers);

    Eval eval = nnue->evaluate(board);
    eval = (eval * getMaterialScale(board)) / 1024;
    eval = board->stack->rule50_ply < 75 ? eval * (220 - board->stack->rule50_ply) / 220 : eval * (-((board->stack->rule50_ply - 50) / 8) * ((board->stack->rule50_ply - 50) / 8) + 75) / 100;

    eval = std::clamp((int) eval, (int) -EVAL_MATE_IN_MAX_PLY + 1, (int) EVAL_MATE_IN_MAX_PLY - 1);
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
        evalString = "cp " + std::to_string(100 * value / 266);
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
