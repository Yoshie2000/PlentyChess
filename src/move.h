#pragma once

#include <stdint.h>
#include <string>

#include "types.h"
#include "board.h"
#include "history.h"

#define PROMOTION_QUEEN (Move) (0 << 14)
#define PROMOTION_ROOK (Move) (1 << 14)
#define PROMOTION_BISHOP (Move) (2 << 14)
#define PROMOTION_KNIGHT (Move) (3 << 14)

constexpr Move MOVE_NULL = 0;
constexpr Move MOVE_NONE = INT16_MAX;

constexpr Move MOVE_PROMOTION = 1 << 12;
constexpr Move MOVE_ENPASSANT = 2 << 12;
constexpr Move MOVE_CASTLING = 3 << 12;

// Sliding piece stuff
constexpr uint8_t DIRECTION_UP = 0;
constexpr uint8_t DIRECTION_RIGHT = 1;
constexpr uint8_t DIRECTION_DOWN = 2;
constexpr uint8_t DIRECTION_LEFT = 3;
constexpr uint8_t DIRECTION_UP_RIGHT = 4;
constexpr uint8_t DIRECTION_DOWN_RIGHT = 5;
constexpr uint8_t DIRECTION_DOWN_LEFT = 6;
constexpr uint8_t DIRECTION_UP_LEFT = 7;
constexpr uint8_t DIRECTION_NONE = 8;

constexpr Piece SLIDING_PIECES[4] = { PIECE_QUEEN, PIECE_ROOK, PIECE_BISHOP };

// Start & End indices in the direction indices for each piece type
constexpr uint8_t DIRECTIONS[PIECE_TYPES][2] = {
    { DIRECTION_NONE, DIRECTION_NONE }, // pawn
    { DIRECTION_NONE, DIRECTION_NONE }, // knight
    { DIRECTION_UP_RIGHT, DIRECTION_UP_LEFT }, // bishop
    { DIRECTION_UP, DIRECTION_LEFT }, // rook
    { DIRECTION_UP, DIRECTION_UP_LEFT }, // queen
    { DIRECTION_UP, DIRECTION_UP_LEFT }, // king
};

constexpr int8_t DIRECTION_DELTAS[8] = { 8, 1, -8, -1, 9, -7, -9, 7 };

constexpr int8_t UP[2] = { 8, -8 };
constexpr int8_t UP_DOUBLE[2] = { 16, -16 };
constexpr int8_t UP_LEFT[2] = { 7, -9 };
constexpr int8_t UP_RIGHT[2] = { 9, -7 };

constexpr Piece PROMOTION_PIECE[4] = { PIECE_QUEEN, PIECE_ROOK, PIECE_BISHOP, PIECE_KNIGHT };

constexpr Square LASTSQ_TABLE[64][8] = {
    { 56, 7, 0, 0, 63, 0, 0, 0 },
    { 57, 7, 1, 0, 55, 1, 1, 8 },
    { 58, 7, 2, 0, 47, 2, 2, 16 },
    { 59, 7, 3, 0, 39, 3, 3, 24 },
    { 60, 7, 4, 0, 31, 4, 4, 32 },
    { 61, 7, 5, 0, 23, 5, 5, 40 },
    { 62, 7, 6, 0, 15, 6, 6, 48 },
    { 63, 7, 7, 0, 7, 7, 7, 56 },
    { 56, 15, 0, 8, 62, 1, 8, 8 },
    { 57, 15, 1, 8, 63, 2, 0, 16 },
    { 58, 15, 2, 8, 55, 3, 1, 24 },
    { 59, 15, 3, 8, 47, 4, 2, 32 },
    { 60, 15, 4, 8, 39, 5, 3, 40 },
    { 61, 15, 5, 8, 31, 6, 4, 48 },
    { 62, 15, 6, 8, 23, 7, 5, 56 },
    { 63, 15, 7, 8, 15, 15, 6, 57 },
    { 56, 23, 0, 16, 61, 2, 16, 16 },
    { 57, 23, 1, 16, 62, 3, 8, 24 },
    { 58, 23, 2, 16, 63, 4, 0, 32 },
    { 59, 23, 3, 16, 55, 5, 1, 40 },
    { 60, 23, 4, 16, 47, 6, 2, 48 },
    { 61, 23, 5, 16, 39, 7, 3, 56 },
    { 62, 23, 6, 16, 31, 15, 4, 57 },
    { 63, 23, 7, 16, 23, 23, 5, 58 },
    { 56, 31, 0, 24, 60, 3, 24, 24 },
    { 57, 31, 1, 24, 61, 4, 16, 32 },
    { 58, 31, 2, 24, 62, 5, 8, 40 },
    { 59, 31, 3, 24, 63, 6, 0, 48 },
    { 60, 31, 4, 24, 55, 7, 1, 56 },
    { 61, 31, 5, 24, 47, 15, 2, 57 },
    { 62, 31, 6, 24, 39, 23, 3, 58 },
    { 63, 31, 7, 24, 31, 31, 4, 59 },
    { 56, 39, 0, 32, 59, 4, 32, 32 },
    { 57, 39, 1, 32, 60, 5, 24, 40 },
    { 58, 39, 2, 32, 61, 6, 16, 48 },
    { 59, 39, 3, 32, 62, 7, 8, 56 },
    { 60, 39, 4, 32, 63, 15, 0, 57 },
    { 61, 39, 5, 32, 55, 23, 1, 58 },
    { 62, 39, 6, 32, 47, 31, 2, 59 },
    { 63, 39, 7, 32, 39, 39, 3, 60 },
    { 56, 47, 0, 40, 58, 5, 40, 40 },
    { 57, 47, 1, 40, 59, 6, 32, 48 },
    { 58, 47, 2, 40, 60, 7, 24, 56 },
    { 59, 47, 3, 40, 61, 15, 16, 57 },
    { 60, 47, 4, 40, 62, 23, 8, 58 },
    { 61, 47, 5, 40, 63, 31, 0, 59 },
    { 62, 47, 6, 40, 55, 39, 1, 60 },
    { 63, 47, 7, 40, 47, 47, 2, 61 },
    { 56, 55, 0, 48, 57, 6, 48, 48 },
    { 57, 55, 1, 48, 58, 7, 40, 56 },
    { 58, 55, 2, 48, 59, 15, 32, 57 },
    { 59, 55, 3, 48, 60, 23, 24, 58 },
    { 60, 55, 4, 48, 61, 31, 16, 59 },
    { 61, 55, 5, 48, 62, 39, 8, 60 },
    { 62, 55, 6, 48, 63, 47, 0, 61 },
    { 63, 55, 7, 48, 55, 55, 1, 62 },
    { 56, 63, 0, 56, 56, 7, 56, 56 },
    { 57, 63, 1, 56, 57, 15, 48, 57 },
    { 58, 63, 2, 56, 58, 23, 40, 58 },
    { 59, 63, 3, 56, 59, 31, 32, 59 },
    { 60, 63, 4, 56, 60, 39, 24, 60 },
    { 61, 63, 5, 56, 61, 47, 16, 61 },
    { 62, 63, 6, 56, 62, 55, 8, 62 },
    { 63, 63, 7, 56, 63, 63, 0, 63 }
};

constexpr Move createMove(Square origin, Square target) {
    return (Move)((origin & 0x3F) | ((target & 0x3F) << 6));
}

constexpr Square moveOrigin(Move move) {
    return (Square)(move & 0x3F);
}

constexpr Square moveTarget(Move move) {
    return (Square)((move >> 6) & 0x3F);
}

constexpr bool isCapture(Board* board, Move move) {
    Move moveType = 0x3000 & move;
    if (moveType == MOVE_CASTLING) return false;
    if (moveType == MOVE_ENPASSANT || (moveType == MOVE_PROMOTION && (move & 0xC000) == PROMOTION_QUEEN)) return true;
    return board->pieces[moveTarget(move)] != NO_PIECE;
}

bool isPseudoLegal(Board* board, Move move);
bool isLegal(Board* board, Move move);
bool givesCheck(Board* board, Move move);

void generateLastSqTable();

void generateMoves(Board* board, Move* moves, int* counter, bool onlyCaptures = false);

std::string moveToString(Move move, bool chess960);

Square stringToSquare(char* string);
Move stringToMove(char* string, Board* board = nullptr);

#define GEN_STAGE_TTMOVE 0

#define GEN_STATE_GEN_CAPTURES 1
#define GEN_STAGE_CAPTURES 2

#define GEN_STAGE_KILLERS 3
#define GEN_STAGE_COUNTERMOVES 4

#define GEN_STAGE_GEN_REMAINING 5
#define GEN_STAGE_REMAINING 6

#define GEN_STAGE_GEN_BAD_CAPTURES 7
#define GEN_STAGE_BAD_CAPTURES 8

#define GEN_STAGE_DONE 100

class MoveGen {

    Board* board;
    History* history;
    SearchStack* searchStack;
    Move ttMove;
    bool onlyCaptures;
    Move killers[2];

    Move moveList[MAX_MOVES];
    int moveListScores[MAX_MOVES];
    int generatedMoves;
    int returnedMoves;
    int killerCount;

    Move badCaptureList[48]; // There can never be more than 32 pieces on the board => never more than 32 captures possible
    int badCaptureScores[48];
    int generatedBadCaptures; // Bad captures only count as "generated" when the corresponding stage sorts them
    int flaggedBadCaptures; // Before that, this counter will keep track of them
    int returnedBadCaptures;

    int generationStage;
    int depth;

    Move counterMove;

public:
    MoveGen(Board* board, History* history, SearchStack* searchStack, Move ttMove, Move _killers[2], int depth) : board(board), history(history), searchStack(searchStack), ttMove(ttMove), onlyCaptures(false), killers{ _killers[0], _killers[1] }, moveList{ MOVE_NONE }, generatedMoves(0), returnedMoves(0), killerCount(0), badCaptureList{ MOVE_NONE }, generatedBadCaptures(0), flaggedBadCaptures(0), returnedBadCaptures(0), generationStage(GEN_STAGE_TTMOVE), depth(depth) {
        std::fill(moveList, moveList + MAX_MOVES, MOVE_NONE);
        std::fill(badCaptureList, badCaptureList + 32, MOVE_NONE);
        counterMove = searchStack->ply > 0 ? history->getCounterMove((searchStack - 1)->move) : MOVE_NONE;
    }
    MoveGen(Board* board, History* history, SearchStack* searchStack, Move ttMove, bool onlyCaptures, int depth) : board(board), history(history), searchStack(searchStack), ttMove(ttMove), onlyCaptures(onlyCaptures), killers{ MOVE_NONE, MOVE_NONE }, moveList{ MOVE_NONE }, generatedMoves(0), returnedMoves(0), killerCount(0), badCaptureList{ MOVE_NONE }, generatedBadCaptures(0), flaggedBadCaptures(0), returnedBadCaptures(0), generationStage(GEN_STAGE_TTMOVE), depth(depth) {
        std::fill(moveList, moveList + MAX_MOVES, MOVE_NONE);
        std::fill(badCaptureList, badCaptureList + 32, MOVE_NONE);
        counterMove = onlyCaptures || searchStack->ply == 0 ? MOVE_NONE : history->getCounterMove((searchStack - 1)->move);
    }

    Move nextMove();

private:

    int scoreGoodCaptures(int beginIndex, int endIndex);
    int scoreQuiets(int beginIndex, int endIndex);
    void scoreBadCaptures();

    void sortMoves(Move* moves, int* scores, int beginIndex, int endIndex);

};