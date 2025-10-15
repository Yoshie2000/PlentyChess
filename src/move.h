#pragma once

#include <stdint.h>
#include <string>

#include "types.h"

struct Board;
class History;

using MoveList = ArrayVec<Move, MAX_MOVES>;

void generateMoves(Board* board, MoveList& moves, bool onlyCaptures = false);

Square stringToSquare(const char* string);
Move stringToMove(const char* string, Board* board);

enum MoveGenStage {
    STAGE_TTMOVE,
    STAGE_GEN_CAPTURES,
    STAGE_PLAY_GOOD_CAPTURES,
    STAGE_KILLER,
    STAGE_COUNTERS,
    STAGE_GEN_QUIETS,
    STAGE_PLAY_QUIETS,
    STAGE_PLAY_BAD_CAPTURES,
    STAGE_DONE = 100
};

constexpr MoveGenStage& operator++(MoveGenStage& stage) {
    stage = static_cast<MoveGenStage>(static_cast<int>(stage) + 1);
    return stage;
}

class MoveGen {

public:

    Board* board;
    History* history;
    SearchStack* searchStack;
    Move ttMove, counterMove;
    bool onlyCaptures;
    Move killer;

    MoveList moveList;
    ArrayVec<int, MAX_MOVES> moveListScores;
    int returnedMoves;

    MoveList badCaptureList;
    int returnedBadCaptures;

    MoveGenStage stage;
    Depth depth;

    bool probCut;
    int probCutThreshold;

    bool skipQuiets;

    // Main search
    MoveGen(Board* board, History* history, SearchStack* searchStack, Move ttMove, Depth depth);
    // qSearch
    MoveGen(Board* board, History* history, SearchStack* searchStack, Move ttMove, bool onlyCaptures, Depth depth);
    // ProbCut
    MoveGen(Board* board, History* history, SearchStack* searchStack, Move ttMove, int probCutThreshold, Depth depth);

    Move nextMove();

    void skipQuietMoves() {
        skipQuiets = true;
    }

private:

    int scoreCaptures(int beginIndex, int endIndex);
    int scoreQuiets(int beginIndex, int endIndex);

    void sortMoves(Move* moves, int* scores, int beginIndex, int endIndex);

};