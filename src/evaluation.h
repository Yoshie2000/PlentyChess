#ifndef INCLUDE_EVALUATION_H
#define INCLUDE_EVALUATION_H

#include "types.h"
#include "board.h"
#include "search.h"

const Eval EVAL_MATE = 30000;
const Eval EVAL_MATE_IN_MAX_PLY = EVAL_MATE - MAX_PLY;

const Eval EVAL_INFINITE = 31000;

extern Eval PIECE_VALUES[PIECE_TYPES];

Eval evaluate(Board* board);

std::string formatEval(Eval value);

constexpr Eval mateIn(int ply) {
    return EVAL_MATE - ply;
}

constexpr Eval matedIn(int ply) {
    return -EVAL_MATE + ply;
}

#endif