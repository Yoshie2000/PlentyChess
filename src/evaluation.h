#ifndef INCLUDE_EVALUATION_H
#define INCLUDE_EVALUATION_H

#include "types.h"
#include "board.h"
#include "search.h"

#define PHASE_MG 0
#define PHASE_EG 1

const Eval EVAL_MATE = 30000;
const Eval EVAL_MATE_IN_MAX_PLY = EVAL_MATE - MAX_PLY;

const Eval EVAL_INFINITE = 31000;
const Eval EVAL_NONE = 31010;

extern const Eval PIECE_VALUES[PIECE_TYPES];
extern const PhaseEval PSQ[PIECE_TYPES][64];

constexpr Square psqIndex(Square square, Color side) {
    if (side == COLOR_BLACK)
        return square;
    return square ^ 56;
}

Eval evaluate(Board* board);

void debugEval(Board* board);

std::string formatEval(Eval value);

constexpr Eval mateIn(int ply) {
    return EVAL_MATE - ply;
}

constexpr Eval matedIn(int ply) {
    return -EVAL_MATE + ply;
}

#endif