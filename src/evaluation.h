#pragma once

#include "types.h"
#include "board.h"
#include "search.h"
#include "nnue.h"

extern int PIECE_VALUES[Piece::TOTAL + 1];

Eval evaluate(Board* board, NNUE* nnue);

std::string formatEval(Score value);

bool SEE(Board* board, Move move, int threshold);

constexpr Score mateIn(int ply) {
    return SCORE_MATE - ply;
}

constexpr Score matedIn(int ply) {
    return -SCORE_MATE + ply;
}