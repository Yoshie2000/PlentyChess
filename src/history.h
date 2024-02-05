#pragma once

#include "types.h"
#include "board.h"

class History {

    int quietHistory[2][64][64];
    Move counterMoves[64][64];
    int continuationHistory[2][PIECE_TYPES][64][PIECE_TYPES][64];
    int captureHistory[2][PIECE_TYPES][64][PIECE_TYPES];

public:
    void initHistory();

    int getHistory(Board& board, SearchStack* searchStack, Move move, bool isCapture);

    int getQuietHistory(Board& board, Move move);
    void updateQuietHistory(Board& board, Move move, int bonus);

    int getContinuationHistory(Board& board, SearchStack* stack, Move move);
    void updateContinuationHistory(Board& board, SearchStack* stack, Move move, int bonus);

    int* getCaptureHistory(Board& board, Move move);
    void updateSingleCaptureHistory(Board& board, Move move, int bonus);
    void updateCaptureHistory(Board& board, Move move, int bonus, Move* captureMoves, int captureMoveCount);

    void updateQuietHistories(Board& board, SearchStack* stack, Move move, int bonus, Move* quietMoves, int quietMoveCount);

    Move getCounterMove(Move move);
    void setCounterMove(Move move, Move counter);

};