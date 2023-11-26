#ifndef INCLUDE_EVALUATION_H
#define INCLUDE_EVALUATION_H

#include "types.h"
#include "board.h"
#include "search.h"

const Eval EVAL_MATE = 30000;
const Eval EVAL_MATE_IN_MAX_PLY = EVAL_MATE - MAX_PLY;

Eval evaluate(Board* board);

#endif