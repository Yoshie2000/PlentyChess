#include <string.h>
#include <cassert>

#include "history.h"
#include "types.h"
#include "move.h"
#include "evaluation.h"
#include "spsa.h"

TUNE_INT(pawnCorrectionFactor, 5566, 1000, 10000);
TUNE_INT(nonPawnCorrectionFactor, 5500, 1000, 10000);
TUNE_INT(minorCorrectionFactor, 3718, 1000, 10000);
TUNE_INT(majorCorrectionFactor, 2992, 1000, 10000);
TUNE_INT(continuationCorrectionFactor, 5368, 1000, 10000);

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
    memset(nonPawnCorrectionHistory, 0, sizeof(nonPawnCorrectionHistory));
    memset(minorCorrectionHistory, 0, sizeof(minorCorrectionHistory));
    memset(majorCorrectionHistory, 0, sizeof(majorCorrectionHistory));
    memset(continuationCorrectionHistory, 0, sizeof(continuationCorrectionHistory));
    for (int i = 0; i < PAWN_HISTORY_SIZE; i++) {
        for (int j = 0; j < 2; j++) {
            for (int k = 0; k < Piece::TOTAL; k++) {
                for (int l = 0; l < 64; l++) {
                    pawnHistory[i][j][k][l] = -1000;
                }
            }
        }
    }
}

Eval History::correctStaticEval(Eval eval, Board* board, SearchStack* searchStack) {
    int64_t pawnEntry = correctionHistory[board->stm][board->stack->pawnHash & (CORRECTION_HISTORY_SIZE - 1)];
    int64_t nonPawnEntry = nonPawnCorrectionHistory[board->stm][Color::WHITE][board->stack->nonPawnHash[Color::WHITE] & (CORRECTION_HISTORY_SIZE - 1)] + nonPawnCorrectionHistory[board->stm][Color::BLACK][board->stack->nonPawnHash[Color::BLACK] & (CORRECTION_HISTORY_SIZE - 1)];
    int64_t minorEntry = minorCorrectionHistory[board->stm][board->stack->minorHash & (CORRECTION_HISTORY_SIZE - 1)];
    int64_t majorEntry = majorCorrectionHistory[board->stm][board->stack->majorHash & (CORRECTION_HISTORY_SIZE - 1)];
    int64_t contEntry = (searchStack - 1)->movedPiece != Piece::NONE ? *((searchStack - 1)->contCorrHist) : 0;

    int64_t history = pawnEntry * pawnCorrectionFactor + nonPawnEntry * nonPawnCorrectionFactor + minorEntry * minorCorrectionFactor + majorEntry * majorCorrectionFactor + contEntry * continuationCorrectionFactor;

    Eval adjustedEval = eval + history / 65536;
    adjustedEval = std::clamp((int)adjustedEval, (int)-EVAL_TBWIN_IN_MAX_PLY + 1, (int)EVAL_TBWIN_IN_MAX_PLY - 1);
    return adjustedEval;
}

void History::updateCorrectionHistory(Board* board, SearchStack* searchStack, int16_t bonus) {
    // Pawn
    Eval scaledBonus = bonus - correctionHistory[board->stm][board->stack->pawnHash & (CORRECTION_HISTORY_SIZE - 1)] * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
    correctionHistory[board->stm][board->stack->pawnHash & (CORRECTION_HISTORY_SIZE - 1)] += scaledBonus;

    // Non-Pawn
    scaledBonus = bonus - nonPawnCorrectionHistory[board->stm][Color::WHITE][board->stack->nonPawnHash[Color::WHITE] & (CORRECTION_HISTORY_SIZE - 1)] * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
    nonPawnCorrectionHistory[board->stm][Color::WHITE][board->stack->nonPawnHash[Color::WHITE] & (CORRECTION_HISTORY_SIZE - 1)] += scaledBonus;
    scaledBonus = bonus - nonPawnCorrectionHistory[board->stm][Color::BLACK][board->stack->nonPawnHash[Color::BLACK] & (CORRECTION_HISTORY_SIZE - 1)] * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
    nonPawnCorrectionHistory[board->stm][Color::BLACK][board->stack->nonPawnHash[Color::BLACK] & (CORRECTION_HISTORY_SIZE - 1)] += scaledBonus;

    // Minor / Major
    scaledBonus = bonus - minorCorrectionHistory[board->stm][board->stack->minorHash & (CORRECTION_HISTORY_SIZE - 1)] * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
    minorCorrectionHistory[board->stm][board->stack->minorHash & (CORRECTION_HISTORY_SIZE - 1)] += scaledBonus;
    scaledBonus = bonus - majorCorrectionHistory[board->stm][board->stack->majorHash & (CORRECTION_HISTORY_SIZE - 1)] * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
    majorCorrectionHistory[board->stm][board->stack->majorHash & (CORRECTION_HISTORY_SIZE - 1)] += scaledBonus;

    // Continuation
    if ((searchStack - 1)->movedPiece != Piece::NONE) {
        scaledBonus = bonus - *(searchStack - 1)->contCorrHist * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
        *(searchStack - 1)->contCorrHist += scaledBonus;
    }
}

int History::getHistory(Board* board, BoardStack* boardStack, SearchStack* searchStack, Move move, bool isCapture) {
    if (isCapture) {
        return *getCaptureHistory(board, move);
    }
    else {
        return getQuietHistory(move, board->stm, board, boardStack) + 2 * getContinuationHistory(searchStack, board->stm, board->pieces[moveOrigin(move)], move) + getPawnHistory(board, move);
    }
}

int16_t History::getQuietHistory(Move move, Color stm, Board* board, BoardStack* stack) {
    Square origin = moveOrigin(move), target = moveTarget(move);
    return quietHistory[stm][origin][board->isSquareThreatened(origin, stack)][target][board->isSquareThreatened(target, stack)];
}

void History::updateQuietHistory(Move move, Color stm, Board* board, BoardStack* stack, int16_t bonus) {
    int16_t scaledBonus = bonus - getQuietHistory(move, stm, board, stack) * std::abs(bonus) / 32000;
    Square origin = moveOrigin(move), target = moveTarget(move);
    quietHistory[stm][origin][board->isSquareThreatened(origin, stack)][target][board->isSquareThreatened(target, stack)] += scaledBonus;
}

int16_t History::getPawnHistory(Board* board, Move move) {
    return pawnHistory[board->stack->pawnHash & (PAWN_HISTORY_SIZE - 1)][board->stm][board->pieces[moveOrigin(move)]][moveTarget(move)];
}

void History::updatePawnHistory(Board* board, Move move, int16_t bonus) {
    int16_t scaledBonus = bonus - getPawnHistory(board, move) * std::abs(bonus) / 32000;
    pawnHistory[board->stack->pawnHash & (PAWN_HISTORY_SIZE - 1)][board->stm][board->pieces[moveOrigin(move)]][moveTarget(move)] += scaledBonus;
}

int History::getContinuationHistory(SearchStack* stack, Color side, Piece piece, Move move) {
    assert(piece != Piece::NONE);
    Square target = moveTarget(move);

    int score = 0;
    int pieceTo = 2 * 64 * piece + 2 * target + side;

    if ((stack - 1)->movedPiece != Piece::NONE)
        score += 2 * (stack - 1)->contHist[pieceTo];

    if ((stack - 2)->movedPiece != Piece::NONE)
        score += (stack - 2)->contHist[pieceTo];

    if ((stack - 4)->movedPiece != Piece::NONE)
        score += (stack - 4)->contHist[pieceTo];
    
    if ((stack - 6)->movedPiece != Piece::NONE)
        score += (stack - 6)->contHist[pieceTo] / 2;

    return score;
}

void History::updateContinuationHistory(SearchStack* stack, Color side, Piece piece, Move move, int16_t bonus) {
    assert(piece != Piece::NONE);
    Square target = moveTarget(move);

    int16_t scaledBonus = bonus - getContinuationHistory(stack, side, piece, move) * std::abs(bonus) / 32000;
    int pieceTo = 2 * 64 * piece + 2 * target + side;

    if ((stack - 1)->movedPiece != Piece::NONE)
        (stack - 1)->contHist[pieceTo] += scaledBonus;

    if ((stack - 2)->movedPiece != Piece::NONE)
        (stack - 2)->contHist[pieceTo] += scaledBonus;

    if ((stack - 3)->movedPiece != Piece::NONE)
        (stack - 3)->contHist[pieceTo] += scaledBonus / 4;

    if ((stack - 4)->movedPiece != Piece::NONE)
        (stack - 4)->contHist[pieceTo] += scaledBonus;
    
    if ((stack - 6)->movedPiece != Piece::NONE)
        (stack - 6)->contHist[pieceTo] += scaledBonus / 2;
}

int16_t* History::getCaptureHistory(Board* board, Move move) {
    Piece movedPiece = board->pieces[moveOrigin(move)];
    Piece capturedPiece = board->pieces[moveTarget(move)];
    Square target = moveTarget(move);

    if (capturedPiece == Piece::NONE && (move & 0x3000) != 0) // for ep and promotions, just take pawns
        capturedPiece = Piece::PAWN;

    assert(movedPiece != Piece::NONE && capturedPiece != Piece::NONE);

    return &captureHistory[board->stm][movedPiece][target][capturedPiece];
}

void History::updateSingleCaptureHistory(Board* board, Move move, int16_t bonus) {
    int16_t* captHistScore = getCaptureHistory(board, move);

    int16_t scaledBonus = bonus - *captHistScore * std::abs(bonus) / 32000;
    *captHistScore += scaledBonus;
}

void History::updateCaptureHistory(Board* board, Move move, int moveSearchCount, int16_t bonus, int16_t malus, Move* captureMoves, int* captureSearchCount, int captureMoveCount) {
    if (board->isCapture(move)) {
        updateSingleCaptureHistory(board, move, bonus * moveSearchCount);
    }

    for (int i = 0; i < captureMoveCount; i++) {
        Move cMove = captureMoves[i];
        if (move == cMove) continue;
        updateSingleCaptureHistory(board, cMove, -malus * captureSearchCount[i]);
    }
}

void History::updateQuietHistories(Board* board, BoardStack* boardStack, SearchStack* stack, Move move, int moveSearchCount, int16_t bonus, int16_t malus, Move* quietMoves, int* quietSearchCount, int quietMoveCount) {
    // Increase stats for this move
    updateQuietHistory(move, board->stm, board, boardStack, bonus * moveSearchCount);
    updateContinuationHistory(stack, board->stm, board->pieces[moveOrigin(move)], move, bonus * moveSearchCount);
    updatePawnHistory(board, move, bonus * moveSearchCount);

    // Decrease stats for all other quiets
    for (int i = 0; i < quietMoveCount; i++) {
        Move qMove = quietMoves[i];
        if (move == qMove) continue;
        updateQuietHistory(qMove, board->stm, board, boardStack, -malus * quietSearchCount[i]);
        updateContinuationHistory(stack, board->stm, board->pieces[moveOrigin(qMove)], qMove, -malus * quietSearchCount[i]);
        updatePawnHistory(board, qMove, -malus * quietSearchCount[i]);
    }
}

Move History::getCounterMove(Move move) {
    return counterMoves[moveOrigin(move)][moveTarget(move)];
}

void History::setCounterMove(Move move, Move counter) {
    counterMoves[moveOrigin(move)][moveTarget(move)] = counter;
}