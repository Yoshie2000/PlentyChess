#pragma once

#include <stdint.h>
#include <string>

#include "types.h"

struct Board;
class History;

constexpr int8_t UP[2] = { 8, -8 };
constexpr int8_t UP_DOUBLE[2] = { 16, -16 };
constexpr int8_t UP_LEFT[2] = { 7, -9 };
constexpr int8_t UP_RIGHT[2] = { 9, -7 };

void generateMoves(Board* board, Move* moves, int* counter, bool onlyCaptures = false);

Square stringToSquare(const char* string);
Move stringToMove(const char* string, Board* board);

typedef int MoveGenStage;
constexpr MoveGenStage STAGE_TTMOVE = 0;
constexpr MoveGenStage STAGE_GEN_CAPTURES = 1;
constexpr MoveGenStage STAGE_PLAY_GOOD_CAPTURES = 2;
constexpr MoveGenStage STAGE_KILLER = 3;
constexpr MoveGenStage STAGE_COUNTERS = 4;
constexpr MoveGenStage STAGE_GEN_QUIETS = 5;
constexpr MoveGenStage STAGE_PLAY_QUIETS = 6;
constexpr MoveGenStage STAGE_PLAY_BAD_CAPTURES = 7;
constexpr MoveGenStage STAGE_DONE = 100;

class MoveGen {

public:

    Board* board;
    History* history;
    SearchStack* searchStack;
    Move ttMove, counterMove;
    bool onlyCaptures;
    Move killer;

    Move moveList[MAX_MOVES];
    int moveListScores[MAX_MOVES];
    int generatedMoves;
    int returnedMoves;

    Move badCaptureList[MAX_CAPTURES];
    int generatedBadCaptures; // Bad captures only count as "generated" when they are sorted out by SEE
    int returnedBadCaptures;

    MoveGenStage stage;
    int16_t depth;

    bool probCut;
    int probCutThreshold;

    bool skipQuiets;

    // Main search
    MoveGen(Board* board, History* history, SearchStack* searchStack, Move ttMove, int16_t depth);
    // qSearch
    MoveGen(Board* board, History* history, SearchStack* searchStack, Move ttMove, bool onlyCaptures, int16_t depth);
    // ProbCut
    MoveGen(Board* board, History* history, SearchStack* searchStack, Move ttMove, int probCutThreshold, int16_t depth);

    Move nextMove();

    void skipQuietMoves() {
        skipQuiets = true;
    }

private:

    int scoreCaptures(int beginIndex, int endIndex);
    int scoreQuiets(int beginIndex, int endIndex);

    void sortMoves(Move* moves, int* scores, int beginIndex, int endIndex);

};