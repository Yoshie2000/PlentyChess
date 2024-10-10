#pragma once

#include <stdint.h>
#include <string>

#include "types.h"

struct Board;
class History;

constexpr Move MOVE_NULL = 0;
constexpr Move MOVE_NONE = INT16_MAX;

constexpr MoveType MOVE_PROMOTION = 1 << 12;
constexpr MoveType MOVE_ENPASSANT = 2 << 12;
constexpr MoveType MOVE_CASTLING = 3 << 12;

constexpr PromotionType PROMOTION_QUEEN = 0 << 14;
constexpr PromotionType PROMOTION_ROOK = 1 << 14;
constexpr PromotionType PROMOTION_BISHOP = 2 << 14;
constexpr PromotionType PROMOTION_KNIGHT = 3 << 14;

constexpr int8_t UP[2] = { 8, -8 };
constexpr int8_t UP_DOUBLE[2] = { 16, -16 };
constexpr int8_t UP_LEFT[2] = { 7, -9 };
constexpr int8_t UP_RIGHT[2] = { 9, -7 };

constexpr Piece PROMOTION_PIECE[4] = { Piece::QUEEN, Piece::ROOK, Piece::BISHOP, Piece::KNIGHT };

constexpr Move createMove(Square origin, Square target) {
    return Move((origin & 0x3F) | ((target & 0x3F) << 6));
}

constexpr Square moveOrigin(Move move) {
    return Square(move & 0x3F);
}

constexpr Square moveTarget(Move move) {
    return Square((move >> 6) & 0x3F);
}

void generateMoves(Board* board, Move* moves, int* counter, bool onlyCaptures = false);

char fileFromSquare(Square square);
std::string moveToString(Move move, bool chess960);
std::string squareToString(Square square);
Square stringToSquare(const char* string);
Move stringToMove(const char* string, Board* board = nullptr);

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
    int depth;

    bool probCut;
    int probCutThreshold;

public:

    // Main search
    MoveGen(Board* board, History* history, SearchStack* searchStack, Move ttMove, int depth);
    // qSearch
    MoveGen(Board* board, History* history, SearchStack* searchStack, Move ttMove, bool onlyCaptures, int depth);
    // ProbCut
    MoveGen(Board* board, History* history, SearchStack* searchStack, Move ttMove, int probCutThreshold, int depth);

    Move nextMove();

private:

    int scoreCaptures(int beginIndex, int endIndex);
    int scoreQuiets(int beginIndex, int endIndex);

    void sortMoves(Move* moves, int* scores, int beginIndex, int endIndex);

};