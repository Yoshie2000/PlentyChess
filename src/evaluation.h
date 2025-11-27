#pragma once

#include "types.h"
#include "board.h"
#include "search.h"
#include "nnue.h"

namespace Eval {

    constexpr Value MATE = 30000;
    constexpr Value MATE_IN_MAX_PLY = MATE - MAX_PLY;

    constexpr Value TBWIN = MATE - 1000;
    constexpr Value TBWIN_IN_MAX_PLY = TBWIN - MAX_PLY;

    constexpr Value INF = 31000;

}

extern Value PIECE_VALUES[Piece::TOTAL + 1];

Value evaluate(Board* board, NNUE* nnue);

std::string formatEval(Value value);

bool SEE(Board* board, Move move, Value threshold);

constexpr Value mateIn(int ply) {
    return Eval::MATE - ply;
}

constexpr Value matedIn(int ply) {
    return -Eval::MATE + ply;
}

constexpr bool isDecisive(Value value) {
    return std::abs(value) >= Eval::TBWIN_IN_MAX_PLY;
}