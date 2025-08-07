#pragma once

#include "types.h"
#include "board.h"
#include "search.h"
#include "nnue.h"

const Eval EVAL_MATE = 30000;
const Eval EVAL_MATE_IN_MAX_PLY = EVAL_MATE - MAX_PLY;

const Eval EVAL_TBWIN = EVAL_MATE_IN_MAX_PLY - 1000;
const Eval EVAL_TBWIN_IN_MAX_PLY = EVAL_TBWIN - MAX_PLY;

const Eval EVAL_INFINITE = 31000;
const Eval EVAL_NONE = 31010;

extern Eval PIECE_VALUES[Piece::TOTAL + 1];

std::pair<Eval, NNHash> evaluate(Board* board, NNUE* nnue);

std::string formatEval(Eval value);

bool SEE(Board* board, Move move, Eval threshold);

constexpr Eval mateIn(int ply) {
    return EVAL_MATE - ply;
}

constexpr Eval matedIn(int ply) {
    return -EVAL_MATE + ply;
}