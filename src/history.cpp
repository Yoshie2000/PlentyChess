#include <string.h>
#include <cassert>

#include "history.h"
#include "types.h"
#include "move.h"
#include "search.h"

int quietHistory[2][64][64];
Move counterMoves[64][64];
int continuationHistory[2][PIECE_TYPES][64][PIECE_TYPES][64];
int captureHistory[2][PIECE_TYPES][64][PIECE_TYPES];

void initHistory() {
    memset(quietHistory, 0, sizeof(quietHistory));
    memset(counterMoves, MOVE_NONE, sizeof(counterMoves));
    memset(continuationHistory, 0, sizeof(continuationHistory));
}

int getHistory(Board* board, SearchStack* searchStack, Move move, bool isCapture) {
    if (isCapture) {
        return *getCaptureHistory(board, move);
    } else {
        return getQuietHistory(board, move) + 2 * getContinuationHistory(board, searchStack, move);
    }
}

int getQuietHistory(Board* board, Move move) {
    return quietHistory[board->stm][moveOrigin(move)][moveTarget(move)];
}

void updateQuietHistory(Board* board, Move move, int bonus) {
    int scaledBonus = bonus - getQuietHistory(board, move) * std::abs(bonus) / 32768;
    quietHistory[board->stm][moveOrigin(move)][moveTarget(move)] += scaledBonus;
}

int getContinuationHistory(Board* board, SearchStack* stack, Move move) {
    Piece piece = board->pieces[moveOrigin(move)];
    if (piece == NO_PIECE)
        piece = board->pieces[moveTarget(move)];
    Square target = moveTarget(move);

    assert(piece != NO_PIECE);

    int score = 0;
    if (stack->ply > 0) {
        if ((stack - 1)->move != MOVE_NULL)
            score += continuationHistory[board->stm][(stack - 1)->movedPiece][moveTarget((stack - 1)->move)][piece][target];

        if (stack->ply > 1) {
            if ((stack - 2)->move != MOVE_NULL)
                score += continuationHistory[board->stm][(stack - 2)->movedPiece][moveTarget((stack - 2)->move)][piece][target];

            if (stack->ply > 3 && (stack - 4)->move != MOVE_NULL) {
                score += continuationHistory[board->stm][(stack - 4)->movedPiece][moveTarget((stack - 4)->move)][piece][target];
            }
        }
    }
    return score;
}

void updateContinuationHistory(Board* board, SearchStack* stack, Move move, int bonus) {
    // Update continuationHistory
    if (stack->ply > 0) {
        Piece piece = board->pieces[moveOrigin(move)];
        if (piece == NO_PIECE)
            piece = board->pieces[moveTarget(move)];
        assert(piece != NO_PIECE);
        Square target = moveTarget(move);

        assert(piece != NO_PIECE);

        int scaledBonus = bonus - getContinuationHistory(board, stack, move) * std::abs(bonus) / 32768;

        if ((stack - 1)->move != MOVE_NULL)
            continuationHistory[board->stm][(stack - 1)->movedPiece][moveTarget((stack - 1)->move)][piece][target] += scaledBonus;

        if (stack->ply > 1) {
            if ((stack - 2)->move != MOVE_NULL)
                continuationHistory[board->stm][(stack - 2)->movedPiece][moveTarget((stack - 2)->move)][piece][target] += scaledBonus;

            if (stack->ply > 3 && (stack - 4)->move != MOVE_NULL) {
                continuationHistory[board->stm][(stack - 4)->movedPiece][moveTarget((stack - 4)->move)][piece][target] += scaledBonus;
            }
        }
    }
}

int* getCaptureHistory(Board* board, Move move) {
    Piece movedPiece = board->pieces[moveOrigin(move)];
    Piece capturedPiece = board->pieces[moveTarget(move)];
    Square target = moveTarget(move);

    if (capturedPiece == NO_PIECE && (move & 0x3000) != 0) // for ep and promotions, just take pawns
        capturedPiece = PIECE_PAWN;

    assert(movedPiece != NO_PIECE && capturedPiece != NO_PIECE);

    return &captureHistory[board->stm][movedPiece][target][capturedPiece];
}

void updateSingleCaptureHistory(Board* board, Move move, int bonus) {
    int* captHistScore = getCaptureHistory(board, move);

    int scaledBonus = bonus - *captHistScore * std::abs(bonus) / 32768;
    *captHistScore += scaledBonus;
}

void updateCaptureHistory(Board* board, Move move, int bonus, Move* captureMoves, int captureMoveCount) {
    if (isCapture(board, move)) {
        updateSingleCaptureHistory(board, move, bonus);
    }

    for (int i = 0; i < captureMoveCount; i++) {
        Move cMove = captureMoves[i];
        if (move == cMove) continue;
        updateSingleCaptureHistory(board, cMove, -bonus);
    }
}

void updateQuietHistories(Board* board, SearchStack* stack, Move move, int bonus, Move* quietMoves, int quietMoveCount) {
    // Increase stats for this move
    updateQuietHistory(board, move, bonus);
    updateContinuationHistory(board, stack, move, bonus);

    // Decrease stats for all other quiets
    for (int i = 0; i < quietMoveCount; i++) {
        Move qMove = quietMoves[i];
        if (move == qMove) continue;
        updateQuietHistory(board, qMove, -bonus);
        updateContinuationHistory(board, stack, qMove, -bonus);
    }
}