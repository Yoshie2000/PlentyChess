#pragma once

#include "types.h"
#include "board.h"
#include "search.h"

extern int quietHistory[2][64][64];
extern Move counterMoves[64][64];
extern int continuationHistory[2][PIECE_TYPES][64][PIECE_TYPES][64];
extern int captureHistory[2][PIECE_TYPES][64][PIECE_TYPES];

void initHistory();

int getQuietHistory(Board* board, Move move);
void updateQuietHistory(Board* board, Move move, int bonus);

int getContinuationHistory(Board* board, SearchStack* stack, Move move);
void updateContinuationHistory(Board* board, SearchStack* stack, Move move, int bonus);

int* getCaptureHistory(Board* board, Move move);
void updateCaptureHistory(Board* board, Move move, int bonus, Move* captureMoves, int captureMoveCount);

void updateQuietHistories(Board* board, SearchStack* stack, Move move, int bonus, Move* quietMoves, int quietMoveCount);