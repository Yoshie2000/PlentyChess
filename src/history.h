#pragma once

#include "types.h"
#include "board.h"

#define PAWN_HISTORY_SIZE 32768
#define CORRECTION_HISTORY_SIZE 16384
#define CORRECTION_HISTORY_LIMIT 1024

class History {

    int16_t quietHistory[2][64][64];
    Move counterMoves[64][64];
    int16_t captureHistory[2][PIECE_TYPES][64][PIECE_TYPES];
    int16_t correctionHistory[2][CORRECTION_HISTORY_SIZE];
    int16_t pawnHistory[PAWN_HISTORY_SIZE][2][PIECE_TYPES][64];

public:

    int16_t continuationHistory[2][PIECE_TYPES][64][PIECE_TYPES * 64];

    void initHistory();

    Eval correctStaticEval(Eval eval, Board* board);
    void updateCorrectionHistory(Board* board, int16_t bonus);

    int getHistory(Board* board, SearchStack* searchStack, Move move, bool isCapture);

    int16_t getPawnHistory(Board* board, Move move);
    void updatePawnHistory(Board* board, Move move, int16_t bonus);

    int16_t getQuietHistory(Board* board, Move move);
    void updateQuietHistory(Board* board, Move move, int16_t bonus);

    int getContinuationHistory(SearchStack* stack, Piece piece, Move move);
    void updateContinuationHistory(SearchStack* stack, Piece piece, Move move, int16_t bonus);

    int16_t* getCaptureHistory(Board* board, Move move);
    void updateSingleCaptureHistory(Board* board, Move move, int16_t bonus);
    void updateCaptureHistory(Board* board, Move move, int16_t bonus, Move* captureMoves, int captureMoveCount);

    void updateQuietHistories(Board* board, SearchStack* stack, Move move, int16_t bonus, Move* quietMoves, int quietMoveCount);

    Move getCounterMove(Move move);
    void setCounterMove(Move move, Move counter);

    constexpr Eval getCorrectionHistory(Board* board) {
        return correctionHistory[board->stm][board->stack->pawnHash & (CORRECTION_HISTORY_SIZE - 1)];
    }

};