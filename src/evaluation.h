#pragma once

#include "types.h"
#include "board.h"
#include "search.h"

#define PHASE_MG 0
#define PHASE_EG 1

const Eval EVAL_MATE = 30000;
const Eval EVAL_MATE_IN_MAX_PLY = EVAL_MATE - MAX_PLY;

const Eval EVAL_INFINITE = 31000;
const Eval EVAL_NONE = 31010;

extern const Eval PIECE_VALUES[PIECE_TYPES + 1];

Eval evaluate(Board* board);

std::string formatEval(Eval value);

bool SEE(Board* board, Move move, Eval threshold);
void debugSEE(Board* board);

constexpr Eval mateIn(int ply) {
    return EVAL_MATE - ply;
}

constexpr Eval matedIn(int ply) {
    return -EVAL_MATE + ply;
}