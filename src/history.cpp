#include <string.h>
#include <cassert>

#include "history.h"
#include "types.h"
#include "move.h"
#include "evaluation.h"
#include "spsa.h"

TUNE_INT(contHistBonusFactor, 100, 50, 150);
TUNE_INT(contHistMalusFactor, 100, 50, 150);
TUNE_INT(correctionHistoryDivisor, 12061, 5000, 20000);

void History::initHistory() {
    memset(quietHistory, 0, sizeof(quietHistory));
    for (Square s1 = 0; s1 < 64; s1++) {
        for (Square s2 = 0; s2 < 64; s2++) {
            counterMoves[s1][s2] = MOVE_NONE;
        }
    }
    memset(continuationHistory, 0, sizeof(continuationHistory));
    memset(captureHistory, 0, sizeof(captureHistory));
    memset(correctionHistory, 0, sizeof(correctionHistory));
}

Eval History::correctStaticEval(Eval eval, Board* board) {
    Eval history = getCorrectionHistory(board);
    Eval adjustedEval = eval + (history * std::abs(history)) / correctionHistoryDivisor;
    adjustedEval = std::clamp((int)adjustedEval, (int)-EVAL_MATE_IN_MAX_PLY + 1, (int)EVAL_MATE_IN_MAX_PLY - 1);
    return adjustedEval;
}

void History::updateCorrectionHistory(Board* board, int16_t bonus) {
    Eval scaledBonus = (Eval) bonus - getCorrectionHistory(board) * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
    correctionHistory[board->stm][board->stack->pawnHash & (CORRECTION_HISTORY_SIZE - 1)] += scaledBonus;
}

int History::getHistory(Board* board, SearchStack* searchStack, Move move, bool isCapture) {
    if (isCapture) {
        return *getCaptureHistory(board, move);
    }
    else {
        return getQuietHistory(board, move) + 2 * getContinuationHistory(board, searchStack, move);
    }
}

int16_t History::getQuietHistory(Board* board, Move move) {
    return quietHistory[board->stm][moveOrigin(move)][moveTarget(move)];
}

void History::updateQuietHistory(Board* board, Move move, int16_t bonus) {
    int16_t scaledBonus = bonus - getQuietHistory(board, move) * std::abs(bonus) / 32000;
    quietHistory[board->stm][moveOrigin(move)][moveTarget(move)] += scaledBonus;
}

int History::getContinuationHistory(Board* board, SearchStack* stack, Move move) {
    Piece piece = board->pieces[moveOrigin(move)];
    if (piece == NO_PIECE)
        piece = board->pieces[moveTarget(move)];
    if ((0x3000 & move) == MOVE_CASTLING)
        piece = PIECE_KING;

    assert(piece != NO_PIECE);

    Square target = moveTarget(move);

    int score = 0;
    int pieceTo = 64 * piece + target;

    if ((stack - 1)->movedPiece != NO_PIECE)
        score += (stack - 1)->contHist[pieceTo];

    if ((stack - 2)->movedPiece != NO_PIECE)
        score += (stack - 2)->contHist[pieceTo];

    if ((stack - 4)->movedPiece != NO_PIECE)
        score += (stack - 4)->contHist[pieceTo];

    return score;
}

void History::updateContinuationHistory(Board* board, SearchStack* stack, Move move, int16_t bonus) {
    // Update continuationHistory
    Piece piece = board->pieces[moveOrigin(move)];
    if (piece == NO_PIECE)
        piece = board->pieces[moveTarget(move)];
    if ((0x3000 & move) == MOVE_CASTLING)
        piece = PIECE_KING;

    assert(piece != NO_PIECE);

    Square target = moveTarget(move);

    int16_t scaledBonus = bonus - getContinuationHistory(board, stack, move) * std::abs(bonus) / 32000;
    int pieceTo = 64 * piece + target;

    if ((stack - 1)->movedPiece != NO_PIECE)
        (stack - 1)->contHist[pieceTo] += scaledBonus;

    if ((stack - 2)->movedPiece != NO_PIECE)
        (stack - 2)->contHist[pieceTo] += scaledBonus;
    
    if ((stack - 3)->movedPiece != NO_PIECE)
        (stack - 3)->contHist[pieceTo] += scaledBonus / 4;

    if ((stack - 4)->movedPiece != NO_PIECE)
        (stack - 4)->contHist[pieceTo] += scaledBonus;
}

int16_t* History::getCaptureHistory(Board* board, Move move) {
    Piece movedPiece = board->pieces[moveOrigin(move)];
    Piece capturedPiece = board->pieces[moveTarget(move)];
    Square target = moveTarget(move);

    if (capturedPiece == NO_PIECE && (move & 0x3000) != 0) // for ep and promotions, just take pawns
        capturedPiece = PIECE_PAWN;

    assert(movedPiece != NO_PIECE && capturedPiece != NO_PIECE);

    return &captureHistory[board->stm][movedPiece][target][capturedPiece];
}

void History::updateSingleCaptureHistory(Board* board, Move move, int16_t bonus) {
    int16_t* captHistScore = getCaptureHistory(board, move);

    int16_t scaledBonus = bonus - *captHistScore * std::abs(bonus) / 32000;
    *captHistScore += scaledBonus;
}

void History::updateCaptureHistory(Board* board, Move move, int16_t bonus, Move* captureMoves, int captureMoveCount) {
    if (isCapture(board, move)) {
        updateSingleCaptureHistory(board, move, bonus);
    }

    for (int i = 0; i < captureMoveCount; i++) {
        Move cMove = captureMoves[i];
        if (move == cMove) continue;
        updateSingleCaptureHistory(board, cMove, -bonus);
    }
}

void History::updateQuietHistories(Board* board, SearchStack* stack, Move move, int16_t bonus, Move* quietMoves, int quietMoveCount) {
    if (!isCapture(board, move)) {
        // Increase stats for this move
        updateQuietHistory(board, move, bonus);
        updateContinuationHistory(board, stack, move, bonus * 100 / contHistBonusFactor);
    }

    // Decrease stats for all other quiets
    for (int i = 0; i < quietMoveCount; i++) {
        Move qMove = quietMoves[i];
        if (move == qMove) continue;
        updateQuietHistory(board, qMove, -bonus);
        updateContinuationHistory(board, stack, qMove, -bonus * 100 / contHistMalusFactor);
    }
}

Move History::getCounterMove(Move move) {
    return counterMoves[moveOrigin(move)][moveTarget(move)];
}

void History::setCounterMove(Move move, Move counter) {
    counterMoves[moveOrigin(move)][moveTarget(move)] = counter;
}