#include <iostream>

#include "types.h"
#include "board.h"
#include "evaluation.h"

Eval PIECE_VALUES[PIECE_TYPES] = {
    100,
    290,
    310,
    500,
    900,
    EVAL_MATE,
};

template<Color side>
Eval evaluate(Board* board) {
    Eval result = 0;

    // Basic material evaluation
    for (Piece piece = 0; piece < PIECE_TYPES - 1; piece++) {
        result += PIECE_VALUES[piece] * board->stack->pieceCount[side][piece];
    }

    return result;
}

Eval evaluate(Board* board) {
    // std::cout << "White: " << evaluate<COLOR_WHITE>(board) << ", Black: " << evaluate<COLOR_BLACK>(board) << std::endl;
    if (board->stm == COLOR_WHITE)
        return evaluate<COLOR_WHITE>(board) - evaluate<COLOR_BLACK>(board);
    else
        return evaluate<COLOR_BLACK>(board) - evaluate<COLOR_WHITE>(board);
}

std::string formatEval(Eval value) {
    std::string evalString;
    if (value >= EVAL_MATE_IN_MAX_PLY) {
        evalString = "mate " + std::to_string(EVAL_MATE - value);
    } else if (value <= -EVAL_MATE_IN_MAX_PLY) {
        evalString = "mate " + std::to_string(-EVAL_MATE - value);
    } else {
        evalString = "cp " + std::to_string(value);
    }
    return evalString;
}