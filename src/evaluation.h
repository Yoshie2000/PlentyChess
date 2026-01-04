#pragma once

#include "types.h"
#include "board.h"
#include "search.h"
#include "nnue.h"

const Score SCORE_MATE = 30000;
const Score SCORE_MATE_IN_MAX_PLY = SCORE_MATE - MAX_PLY;

const Score SCORE_TBWIN = SCORE_MATE_IN_MAX_PLY - 1000;
const Score SCORE_TBWIN_IN_MAX_PLY = SCORE_TBWIN - MAX_PLY;

const Score SCORE_INFINITE = 31000;
const Score SCORE_NONE = 31010;

extern int PIECE_VALUES[Piece::TOTAL + 1];

Score evaluate(Board* board, NNUE* nnue);

std::string formatEval(Score value);

bool SEE(Board* board, Move move, int threshold);

constexpr Score mateIn(int ply) {
    return SCORE_MATE - ply;
}

constexpr Score matedIn(int ply) {
    return -SCORE_MATE + ply;
}