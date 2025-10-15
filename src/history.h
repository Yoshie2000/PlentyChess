#pragma once

#include "types.h"
#include "board.h"

struct SearchedMove {
    Move move;
    int8_t searchCount;
};

using SearchedMoveList = ArrayVec<SearchedMove, 32>;

constexpr int PAWN_HISTORY_SIZE = 8192;
constexpr int CORRECTION_HISTORY_SIZE = 16384;
constexpr int CORRECTION_HISTORY_LIMIT = 1024;

class History {

    int16_t quietHistory[2][64][2][64][2];
    Move counterMoves[64][64];
    int16_t captureHistory[2][Piece::TOTAL][64][Piece::TOTAL];

    int16_t correctionHistory[2][CORRECTION_HISTORY_SIZE];
    int16_t nonPawnCorrectionHistory[2][2][CORRECTION_HISTORY_SIZE];
    int16_t minorCorrectionHistory[2][CORRECTION_HISTORY_SIZE];
    int16_t majorCorrectionHistory[2][CORRECTION_HISTORY_SIZE];

    int16_t pawnHistory[PAWN_HISTORY_SIZE][2][Piece::TOTAL][64];

public:

    int16_t continuationHistory[2][Piece::TOTAL][64][Piece::TOTAL * 64 * 2];
    int16_t continuationCorrectionHistory[2][Piece::TOTAL][64];

    void initHistory();

    Eval getCorrectionValue(Board* board, SearchStack* searchStack);
    Eval correctStaticEval(uint8_t rule50, Eval eval, Eval correctionValue);
    void updateCorrectionHistory(Board* board, SearchStack* searchStack, int16_t bonus);

    int getHistory(Board* board, SearchStack* searchStack, Move move, bool isCapture);

    int16_t getPawnHistory(Board* board, Move move);
    void updatePawnHistory(Board* board, Move move, int16_t bonus);

    int16_t getQuietHistory(Move move, Color stm, Board* board);
    void updateQuietHistory(Move move, Color stm, Board* board, int16_t bonus);

    int getContinuationHistory(SearchStack* stack, Color side, Piece piece, Move move);
    void updateContinuationHistory(SearchStack* stack, Color side, Piece piece, Move move, int16_t bonus);

    int16_t* getCaptureHistory(Board* board, Move move);
    void updateSingleCaptureHistory(Board* board, Move move, int16_t bonus);
    void updateCaptureHistory(Depth depth, Board* board, Move move, int moveSearchCount, Move* captureMoves, int* captureSearchCount, int captureMoveCount);

    void updateQuietHistories(Depth depth, Board* board, SearchStack* stack, Move move, int moveSearchCount, Move* quietMoves, int* quietSearchCount, int quietMoveCount);

    Move getCounterMove(Move move);
    void setCounterMove(Move move, Move counter);

};