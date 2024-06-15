#pragma once

#include "types.h"
#include "board.h"
#include "nnue.h"

constexpr int PAWN_HISTORY_SIZE = 32768;
constexpr int CORRECTION_HISTORY_SIZE = 16384;
constexpr int CORRECTION_HISTORY_LIMIT = 1024;

class History {

    int16_t quietHistory[2][64][2][64][2];
    Move counterMoves[64][64];
    int16_t captureHistory[2][Piece::TOTAL][64][Piece::TOTAL];
    int16_t correctionHistory[2][KING_BUCKETS][KING_BUCKETS][CORRECTION_HISTORY_SIZE];
    int16_t pawnHistory[PAWN_HISTORY_SIZE][2][Piece::TOTAL][64];

public:

    int16_t continuationHistory[2][Piece::TOTAL][64][Piece::TOTAL * 64];

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

private:

    inline Eval getCorrectionHistory(Board* board) {
        return correctionHistory[board->stm][getKingBucket(board->stm, lsb(board->byColor[board->stm] & board->byPiece[Piece::KING])).index][getKingBucket(flip(board->stm), lsb(board->byColor[flip(board->stm)] & board->byPiece[Piece::KING])).index][board->stack->pawnHash & (CORRECTION_HISTORY_SIZE - 1)];
    }

};