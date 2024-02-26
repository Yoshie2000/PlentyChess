#pragma once

#include "types.h"
#include "board.h"

#define PAWN_HISTORY_SIZE 512

#define CORRECTION_HISTORY_SIZE 16384
#define CORRECTION_HISTORY_LIMIT 1024

class History {

    int quietHistory[2][64][64];
    Move counterMoves[64][64];
    int continuationHistory[2][PIECE_TYPES][64][PIECE_TYPES][64];
    int captureHistory[2][PIECE_TYPES][64][PIECE_TYPES];
    int pawnHistory[PAWN_HISTORY_SIZE][2][PIECE_TYPES][64];
    Eval correctionHistory[2][CORRECTION_HISTORY_SIZE];

public:
    void initHistory();

    int getPawnHistory(Board* board, Move move);
    void updatePawnHistory(Board* board, Move move, int bonus);

    Eval correctStaticEval(Eval eval, Board* board);
    void updateCorrectionHistory(Board* board, int bonus);

    int getHistory(Board* board, SearchStack* searchStack, Move move, bool isCapture);

    int getQuietHistory(Board* board, Move move);
    void updateQuietHistory(Board* board, Move move, int bonus);

    int getContinuationHistory(Board* board, SearchStack* stack, Move move);
    void updateContinuationHistory(Board* board, SearchStack* stack, Move move, int bonus);

    int* getCaptureHistory(Board* board, Move move);
    void updateSingleCaptureHistory(Board* board, Move move, int bonus);
    void updateCaptureHistory(Board* board, Move move, int bonus, Move* captureMoves, int captureMoveCount);

    void updateQuietHistories(Board* board, SearchStack* stack, Move move, int bonus, Move* quietMoves, int quietMoveCount);

    Move getCounterMove(Move move);
    void setCounterMove(Move move, Move counter);

private:

    constexpr Eval getCorrectionHistory(Board* board) {
        return correctionHistory[board->stm][board->stack->pawnHash & (CORRECTION_HISTORY_SIZE - 1)];
    }

};