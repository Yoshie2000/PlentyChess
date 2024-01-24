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

Eval evaluate(Board* board) {
    Eval eval = nnue.evaluate(board->stm);

    eval = eval * (220 - board->stack->rule50_ply) / 220;
    
    eval = std::clamp((int) eval, (int) -EVAL_MATE_IN_MAX_PLY + 1, (int) EVAL_MATE_IN_MAX_PLY - 1);
    return eval;
}

std::string formatEval(Eval value) {
    std::string evalString;
    if (value >= EVAL_MATE_IN_MAX_PLY) {
        evalString = "mate " + std::to_string(EVAL_MATE - value);
    }
    else if (value <= -EVAL_MATE_IN_MAX_PLY) {
        evalString = "mate " + std::to_string(-EVAL_MATE - value);
    }
    else {
        evalString = "cp " + std::to_string(100 * value / 250);
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
    int value = PIECE_VALUES[board->pieces[target]] - threshold;
    if (value < 0) return false;

    // If we beat the threshold after losing our piece, pass
    value -= PIECE_VALUES[board->pieces[origin]];
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
        value = -value - 1 - PIECE_VALUES[piece];
        
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

void debugSEE(Board* board) {
    SearchStack* stack = {};

    Move move;
    Move killers[2] = { MOVE_NONE, MOVE_NONE };
    MoveGen movegen(board, stack, MOVE_NONE, MOVE_NONE, killers);
    int i = 0;
    while ((move = movegen.nextMove()) != MOVE_NONE) {
        if (!isCapture(board, move)) {
            std::cout << i << ": " << moveToString(move) << " is not a capture!" << std::endl;
            i++;
            continue;
        }

        bool goodCapture = SEE(board, move, -107);
        if (goodCapture)
            std::cout << i << ": " << moveToString(move) << " is a good capture!" << std::endl;
        else
            std::cout << i << ": " << moveToString(move) << " is a bad capture!" << std::endl;
        i++;
    }
}
