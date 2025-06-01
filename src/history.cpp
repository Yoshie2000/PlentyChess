#include <string.h>
#include <cassert>

#include "history.h"
#include "types.h"
#include "move.h"
#include "evaluation.h"
#include "spsa.h"

// Quiet history
TUNE_INT(historyBonusQuietBase, 24, -500, 500);
TUNE_INT(historyBonusQuietFactor, 227, 1, 500);
TUNE_INT(historyBonusQuietMax, 1997, 32, 4096);
TUNE_INT(historyMalusQuietBase, 69, -500, 500);
TUNE_INT(historyMalusQuietFactor, 196, 1, 500);
TUNE_INT(historyMalusQuietMax, 1583, 32, 4096);

// Continuation history
TUNE_INT(historyBonusContinuationBase, 0, -500, 500);
TUNE_INT(historyBonusContinuationFactor, 216, 1, 500);
TUNE_INT(historyBonusContinuationMax, 2316, 32, 4096);
TUNE_INT(historyMalusContinuationBase, 94, -500, 500);
TUNE_INT(historyMalusContinuationFactor, 211, 1, 500);
TUNE_INT(historyMalusContinuationMax, 1556, 32, 4096);

// Pawn history
TUNE_INT(historyBonusPawnBase, -2, -500, 500);
TUNE_INT(historyBonusPawnFactor, 203, 1, 500);
TUNE_INT(historyBonusPawnMax, 2424, 32, 4096);
TUNE_INT(historyMalusPawnBase, 64, -500, 500);
TUNE_INT(historyMalusPawnFactor, 251, 1, 500);
TUNE_INT(historyMalusPawnMax, 1909, 32, 4096);

// Capture history
TUNE_INT(historyBonusCaptureBase, 31, -500, 500);
TUNE_INT(historyBonusCaptureFactor, 141, 1, 500);
TUNE_INT(historyBonusCaptureMax, 1834, 32, 4096);
TUNE_INT(historyMalusCaptureBase, 150, -500, 500);
TUNE_INT(historyMalusCaptureFactor, 243, 1, 500);
TUNE_INT(historyMalusCaptureMax, 1904, 32, 4096);

// Correction history
TUNE_INT(pawnCorrectionFactor, 5920, 1000, 7500);
TUNE_INT(nonPawnCorrectionFactor, 5576, 1000, 7500);
TUNE_INT(minorCorrectionFactor, 3681, 1000, 7500);
TUNE_INT(majorCorrectionFactor, 3125, 1000, 7500);
TUNE_INT(continuationCorrectionFactor, 5129, 1000, 7500);

// History weights
TUNE_INT(quietHistWeight, 1000, 0, 2000);
TUNE_INT(contHistWeight, 2000, 0, 4000);
TUNE_INT(pawnHistWeight, 1000, 0, 2000);

TUNE_INT(contHistWeightPly1, 2000, 0, 4000);
TUNE_INT(contHistWeightPly2, 1000, 0, 2000);
TUNE_INT(contHistWeightPly4, 1000, 0, 2000);
TUNE_INT(contHistWeightPly6, 500, 0, 2000);

TUNE_INT(contHistUpdateWeightPly1, 1000, 0, 2000);
TUNE_INT(contHistUpdateWeightPly2, 1000, 0, 2000);
TUNE_INT(contHistUpdateWeightPly3, 250, 0, 1000);
TUNE_INT(contHistUpdateWeightPly4, 1000, 0, 2000);
TUNE_INT(contHistUpdateWeightPly6, 500, 0, 2000);

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

Eval History::getCorrectionValue(Board* board, SearchStack* searchStack) {
    int64_t pawnEntry = correctionHistory[board->stm][board->stack->pawnHash & (CORRECTION_HISTORY_SIZE - 1)];
    int64_t nonPawnEntry = nonPawnCorrectionHistory[board->stm][Color::WHITE][board->stack->nonPawnHash[Color::WHITE] & (CORRECTION_HISTORY_SIZE - 1)] + nonPawnCorrectionHistory[board->stm][Color::BLACK][board->stack->nonPawnHash[Color::BLACK] & (CORRECTION_HISTORY_SIZE - 1)];
    int64_t minorEntry = minorCorrectionHistory[board->stm][board->stack->minorHash & (CORRECTION_HISTORY_SIZE - 1)];
    int64_t majorEntry = majorCorrectionHistory[board->stm][board->stack->majorHash & (CORRECTION_HISTORY_SIZE - 1)];
    int64_t contEntry = (searchStack - 1)->movedPiece != Piece::NONE ? *((searchStack - 1)->contCorrHist) : 0;

    return pawnEntry * pawnCorrectionFactor + nonPawnEntry * nonPawnCorrectionFactor + minorEntry * minorCorrectionFactor + majorEntry * majorCorrectionFactor + contEntry * continuationCorrectionFactor;
}

Eval History::correctStaticEval(Eval eval, Eval correctionValue) {
    Eval adjustedEval = eval + correctionValue / 65536;
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
        return quietHistWeight * getQuietHistory(move, board->stm, board, boardStack) / 1000
             + contHistWeight * getContinuationHistory(searchStack, board->stm, board->pieces[moveOrigin(move)], move) / 1000
             + pawnHistWeight *  getPawnHistory(board, move) / 1000;
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
        score += contHistWeightPly1 * (stack - 1)->contHist[pieceTo] / 1000;

    if ((stack - 2)->movedPiece != Piece::NONE)
        score += contHistWeightPly2 * (stack - 2)->contHist[pieceTo] / 1000;

    if ((stack - 4)->movedPiece != Piece::NONE)
        score += contHistWeightPly4 * (stack - 4)->contHist[pieceTo] / 1000;
    
    if ((stack - 6)->movedPiece != Piece::NONE)
        score += contHistWeightPly6 * (stack - 6)->contHist[pieceTo] / 1000;

    return score;
}

void History::updateContinuationHistory(SearchStack* stack, Color side, Piece piece, Move move, int16_t bonus) {
    assert(piece != Piece::NONE);
    Square target = moveTarget(move);

    int16_t scaledBonus = bonus - getContinuationHistory(stack, side, piece, move) * std::abs(bonus) / 32000;
    int pieceTo = 2 * 64 * piece + 2 * target + side;

    if ((stack - 1)->movedPiece != Piece::NONE)
        (stack - 1)->contHist[pieceTo] += contHistUpdateWeightPly1 * scaledBonus / 1000;

    if ((stack - 2)->movedPiece != Piece::NONE)
        (stack - 2)->contHist[pieceTo] += contHistUpdateWeightPly2 * scaledBonus / 1000;

    if ((stack - 3)->movedPiece != Piece::NONE)
        (stack - 3)->contHist[pieceTo] += contHistUpdateWeightPly3 * scaledBonus / 1000;

    if ((stack - 4)->movedPiece != Piece::NONE)
        (stack - 4)->contHist[pieceTo] += contHistUpdateWeightPly4 * scaledBonus / 1000;
    
    if ((stack - 6)->movedPiece != Piece::NONE)
        (stack - 6)->contHist[pieceTo] += contHistUpdateWeightPly6 * scaledBonus / 1000;
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

void History::updateCaptureHistory(int depth, Board* board, Move move, int moveSearchCount, Move* captureMoves, int* captureSearchCount, int captureMoveCount) {
    int captHistBonus = std::min(historyBonusCaptureBase + historyBonusCaptureFactor * depth, historyBonusCaptureMax);
    int captHistMalus = std::min(historyMalusCaptureBase + historyMalusCaptureFactor * depth, historyMalusCaptureMax);

    if (board->isCapture(move)) {
        updateSingleCaptureHistory(board, move, captHistBonus * moveSearchCount);
    }

    for (int i = 0; i < captureMoveCount; i++) {
        Move cMove = captureMoves[i];
        if (move == cMove) continue;
        updateSingleCaptureHistory(board, cMove, -captHistMalus * captureSearchCount[i]);
    }
}

void History::updateQuietHistories(int depth, Board* board, BoardStack* boardStack, SearchStack* stack, Move move, int moveSearchCount, Move* quietMoves, int* quietSearchCount, int quietMoveCount) {
    int quietHistBonus = std::min(historyBonusQuietBase + historyBonusQuietFactor * depth, historyBonusQuietMax);
    int quietHistMalus = std::min(historyMalusQuietBase + historyMalusQuietFactor * depth, historyMalusQuietMax);
    int contHistBonus = std::min(historyBonusContinuationBase + historyBonusContinuationFactor * depth, historyBonusContinuationMax);
    int contHistMalus = std::min(historyMalusContinuationBase + historyMalusContinuationFactor * depth, historyMalusContinuationMax);
    int pawnHistBonus = std::min(historyBonusPawnBase + historyBonusPawnFactor * depth, historyBonusPawnMax);
    int pawnHistMalus = std::min(historyMalusPawnBase + historyMalusPawnFactor * depth, historyMalusPawnMax);
    
    // Increase stats for this move
    updateQuietHistory(move, board->stm, board, boardStack, quietHistBonus * moveSearchCount);
    updateContinuationHistory(stack, board->stm, board->pieces[moveOrigin(move)], move, contHistBonus * moveSearchCount);
    updatePawnHistory(board, move, pawnHistBonus * moveSearchCount);

    // Decrease stats for all other quiets
    for (int i = 0; i < quietMoveCount; i++) {
        Move qMove = quietMoves[i];
        if (move == qMove) continue;
        updateQuietHistory(qMove, board->stm, board, boardStack, -quietHistMalus * quietSearchCount[i]);
        updateContinuationHistory(stack, board->stm, board->pieces[moveOrigin(qMove)], qMove, -contHistMalus * quietSearchCount[i]);
        updatePawnHistory(board, qMove, -pawnHistMalus * quietSearchCount[i]);
    }
}

Move History::getCounterMove(Move move) {
    return counterMoves[moveOrigin(move)][moveTarget(move)];
}

void History::setCounterMove(Move move, Move counter) {
    counterMoves[moveOrigin(move)][moveTarget(move)] = counter;
}