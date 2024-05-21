#pragma once

#include "types.h"
#include "board.h"

#define CORRECTION_HISTORY_SIZE 16384
#define CORRECTION_HISTORY_LIMIT 1024

class History {

    int16_t quietHistory[2][64][64];
    Move counterMoves[64][64];
    int16_t captureHistory[2][PIECE_TYPES][64][PIECE_TYPES];
    int16_t correctionHistory[2][CORRECTION_HISTORY_SIZE];

public:

    int16_t continuationHistory[2][PIECE_TYPES][64][PIECE_TYPES * 64];

    void initHistory();

    Eval correctStaticEval(Eval eval, Board* board);
    void updateCorrectionHistory(Board* board, int16_t bonus);

    int getHistory(Board* board, SearchStack* searchStack, Move move, bool isCapture);

    int16_t getQuietHistory(Board* board, Move move);
    void updateQuietHistory(Board* board, Move move, int16_t bonus);

    template<int... indices>
    int getContinuationHistory(Board* board, SearchStack* stack, Move move);
    template<int... indices>
    void updateContinuationHistory(Board* board, SearchStack* stack, Move move, int16_t bonus);

    int16_t* getCaptureHistory(Board* board, Move move);
    void updateSingleCaptureHistory(Board* board, Move move, int16_t bonus);
    void updateCaptureHistory(Board* board, Move move, int16_t bonus, Move* captureMoves, int captureMoveCount);

    void updateQuietHistories(Board* board, SearchStack* stack, Move move, int16_t bonus, Move* quietMoves, int quietMoveCount);

    Move getCounterMove(Move move);
    void setCounterMove(Move move, Move counter);

private:

    constexpr Eval getCorrectionHistory(Board* board) {
        return correctionHistory[board->stm][board->stack->pawnHash & (CORRECTION_HISTORY_SIZE - 1)];
    }

};