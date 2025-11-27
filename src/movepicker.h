#pragma once

#include <stdint.h>
#include <string>

#include "types.h"
#include "movegen.h"

class History;

enum ModePickerStage {
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

constexpr ModePickerStage& operator++(ModePickerStage& stage) {
    stage = static_cast<ModePickerStage>(static_cast<int>(stage) + 1);
    return stage;
}

class MovePicker {

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

    ModePickerStage stage;
    Depth depth;

    bool probCut;
    int probCutThreshold;

    bool skipQuiets;

    // Main search
    MovePicker(Board* board, History* history, SearchStack* searchStack, Move ttMove, Depth depth);
    // qSearch
    MovePicker(Board* board, History* history, SearchStack* searchStack, Move ttMove, bool onlyCaptures, Depth depth);
    // ProbCut
    MovePicker(Board* board, History* history, SearchStack* searchStack, Move ttMove, int probCutThreshold, Depth depth);

    Move nextMove();

    void skipQuietMoves() {
        skipQuiets = true;
    }

private:

    void scoreCaptures();
    void scoreQuiets();

    void sortMoves();

};