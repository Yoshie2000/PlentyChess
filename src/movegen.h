#pragma once

#include "types.h"

struct Board;

using MoveList = ArrayVec<Move, MAX_MOVES>;

enum MoveGenMode {
    CAPTURES = 1,
    QUIETS = 2,
    ALL = 3,
};

namespace MoveGen {

    template<MoveGenMode MODE = MoveGenMode::ALL>
    void generateMoves(Board* board, MoveList& moves);

}

Move stringToMove(const char* string, Board* board);