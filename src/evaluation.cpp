#include <iostream>
#include <cassert>

#include "types.h"
#include "board.h"
#include "evaluation.h"
#include "magic.h"
#include "nnue.h"

const Eval PIECE_VALUES[PIECE_TYPES + 1] = {
    100, 300, 300, 500, 900, 0, 0
};

const Eval SEE_VALUES[PIECE_TYPES + 1] = {
    90, 290, 310, 570, 1000, 0, 0
};

int getMaterialScale(Board* board) {
    int knightCount = board->stack->pieceCount[COLOR_WHITE][PIECE_KNIGHT] + board->stack->pieceCount[COLOR_BLACK][PIECE_KNIGHT];
    int bishopCount = board->stack->pieceCount[COLOR_WHITE][PIECE_BISHOP] + board->stack->pieceCount[COLOR_BLACK][PIECE_BISHOP];
    int rookCount = board->stack->pieceCount[COLOR_WHITE][PIECE_ROOK] + board->stack->pieceCount[COLOR_BLACK][PIECE_ROOK];
    int queenCount = board->stack->pieceCount[COLOR_WHITE][PIECE_QUEEN] + board->stack->pieceCount[COLOR_BLACK][PIECE_QUEEN];

    int materialValue = PIECE_VALUES[PIECE_KNIGHT] * knightCount + PIECE_VALUES[PIECE_BISHOP] * bishopCount + PIECE_VALUES[PIECE_ROOK] * rookCount + PIECE_VALUES[PIECE_QUEEN] * queenCount;
    return 980 + materialValue / 48;
}

Eval evaluate(Board* board, NNUE* nnue) {
    assert(!board->stack->checkers);

    Eval eval = nnue->evaluate(board);
    eval = (eval * getMaterialScale(board)) / 1024;
    eval = eval * (220 - board->stack->rule50_ply) / 220;

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
        evalString = "cp " + std::to_string(100 * value / 205);
    }
    return evalString;
}

bool SEE(Board* board, Move move, Eval threshold) {
    assert(isPseudoLegal(board, move));

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

    Bitboard occupied = (board->byColor[COLOR_WHITE] | board->byColor[COLOR_BLACK]) ^ (C64(1) << origin);
    Bitboard attackersToTarget = attackersTo(board, target, occupied);

    Bitboard bishops = board->byPiece[PIECE_BISHOP] | board->byPiece[PIECE_QUEEN];
    Bitboard rooks = board->byPiece[PIECE_ROOK] | board->byPiece[PIECE_QUEEN];

    Color side = 1 - board->stm;

    // Make captures until one side has none left / fails to beat the threshold
    while (true) {

        // Remove attackers that have been captured
        attackersToTarget &= occupied;

        // No attackers left for the current side
        Bitboard stmAttackers = board->byColor[side] & attackersToTarget;
        if (!stmAttackers) break;

        // Find least valuable piece
        Piece piece;
        for (piece = PIECE_PAWN; piece < PIECE_KING; piece++) {
            // If the piece attacks the target square, stop
            if (stmAttackers & board->byPiece[piece])
                break;
        }

        side = 1 - side;
        value = -value - 1 - SEE_VALUES[piece];
        
        // Value beats (or can't beat) threshold (negamax)
        if (value >= 0) {
            if (piece == PIECE_KING && (attackersToTarget & board->byColor[side]))
                side = 1 - side;
            break;
        }

        // Remove the used piece
        occupied ^= C64(1) << lsb(stmAttackers & board->byPiece[piece]);

        // Add discovered attacks
        if (piece == PIECE_PAWN || piece == PIECE_BISHOP || piece == PIECE_QUEEN)
            attackersToTarget |= getBishopMoves(target, occupied) & bishops;
        if (piece == PIECE_ROOK || piece == PIECE_QUEEN)
            attackersToTarget |= getRookMoves(target, occupied) & rooks;
    }

    return side != board->stm;
}
