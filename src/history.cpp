#include <string.h>
#include <cassert>

#include "history.h"
#include "types.h"
#include "move.h"
#include "evaluation.h"
#include "spsa.h"

// Quiet history
TUNE_INT(historyBonusQuietBase, 138, -500, 500);
TUNE_INT(historyBonusQuietFactor, 262, 1, 500);
TUNE_INT(historyBonusQuietMax, 2124, 32, 4096);
TUNE_INT(historyMalusQuietBase, 100, -500, 500);
TUNE_INT(historyMalusQuietFactor, 241, 1, 500);
TUNE_INT(historyMalusQuietMax, 1482, 32, 4096);

// Continuation history
TUNE_INT(historyBonusContinuationBase, -73, -500, 500);
TUNE_INT(historyBonusContinuationFactor, 128, 1, 500);
TUNE_INT(historyBonusContinuationMax, 2103, 32, 4096);
TUNE_INT(historyMalusContinuationBase, 96, -500, 500);
TUNE_INT(historyMalusContinuationFactor, 239, 1, 500);
TUNE_INT(historyMalusContinuationMax, 813, 32, 4096);

// Pawn history
TUNE_INT(historyBonusPawnBase, 33, -500, 500);
TUNE_INT(historyBonusPawnFactor, 172, 1, 500);
TUNE_INT(historyBonusPawnMax, 2059, 32, 4096);
TUNE_INT(historyMalusPawnBase, 30, -500, 500);
TUNE_INT(historyMalusPawnFactor, 276, 1, 500);
TUNE_INT(historyMalusPawnMax, 2104, 32, 4096);

// Capture history
TUNE_INT(historyBonusCaptureBase, 24, -500, 500);
TUNE_INT(historyBonusCaptureFactor, 115, 1, 500);
TUNE_INT(historyBonusCaptureMax, 1411, 32, 4096);
TUNE_INT(historyMalusCaptureBase, 94, -500, 500);
TUNE_INT(historyMalusCaptureFactor, 241, 1, 500);
TUNE_INT(historyMalusCaptureMax, 1567, 32, 4096);

// Correction history
TUNE_INT(pawnCorrectionFactor, 6401, 1000, 7500);
TUNE_INT(nonPawnCorrectionFactor, 6018, 1000, 7500);
TUNE_INT(minorCorrectionFactor, 3783, 1000, 7500);
TUNE_INT(majorCorrectionFactor, 2693, 1000, 7500);
TUNE_INT(continuationCorrectionFactor, 5801, 1000, 7500);

void History::initHistory() {
    memset(quietHistory, 0, sizeof(quietHistory));
    memset(counterMoves, 0, sizeof(counterMoves));
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
    int64_t pawnEntry = correctionHistory[board->stm][board->hashes.pawnHash & (CORRECTION_HISTORY_SIZE - 1)];
    int64_t nonPawnEntry = nonPawnCorrectionHistory[board->stm][Color::WHITE][board->hashes.nonPawnHash[Color::WHITE] & (CORRECTION_HISTORY_SIZE - 1)] + nonPawnCorrectionHistory[board->stm][Color::BLACK][board->hashes.nonPawnHash[Color::BLACK] & (CORRECTION_HISTORY_SIZE - 1)];
    int64_t minorEntry = minorCorrectionHistory[board->stm][board->hashes.minorHash & (CORRECTION_HISTORY_SIZE - 1)];
    int64_t majorEntry = majorCorrectionHistory[board->stm][board->hashes.majorHash & (CORRECTION_HISTORY_SIZE - 1)];
    int64_t contEntry = (searchStack - 1)->movedPiece != Piece::NONE ? *((searchStack - 1)->contCorrHist) : 0;

    return pawnEntry * pawnCorrectionFactor + nonPawnEntry * nonPawnCorrectionFactor + minorEntry * minorCorrectionFactor + majorEntry * majorCorrectionFactor + contEntry * continuationCorrectionFactor;
}

Eval History::correctStaticEval(uint8_t rule50, Eval eval, Eval correctionValue) {
    eval = eval * (300 - rule50) / 300;
    Eval adjustedEval = eval + correctionValue / 65536;
    adjustedEval = std::clamp((int)adjustedEval, (int)-EVAL_TBWIN_IN_MAX_PLY + 1, (int)EVAL_TBWIN_IN_MAX_PLY - 1);
    return adjustedEval;
}

void History::updateCorrectionHistory(Board* board, SearchStack* searchStack, int16_t bonus) {
    // Pawn
    Eval scaledBonus = bonus - correctionHistory[board->stm][board->hashes.pawnHash & (CORRECTION_HISTORY_SIZE - 1)] * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
    correctionHistory[board->stm][board->hashes.pawnHash & (CORRECTION_HISTORY_SIZE - 1)] += scaledBonus;

    // Non-Pawn
    scaledBonus = bonus - nonPawnCorrectionHistory[board->stm][Color::WHITE][board->hashes.nonPawnHash[Color::WHITE] & (CORRECTION_HISTORY_SIZE - 1)] * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
    nonPawnCorrectionHistory[board->stm][Color::WHITE][board->hashes.nonPawnHash[Color::WHITE] & (CORRECTION_HISTORY_SIZE - 1)] += scaledBonus;
    scaledBonus = bonus - nonPawnCorrectionHistory[board->stm][Color::BLACK][board->hashes.nonPawnHash[Color::BLACK] & (CORRECTION_HISTORY_SIZE - 1)] * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
    nonPawnCorrectionHistory[board->stm][Color::BLACK][board->hashes.nonPawnHash[Color::BLACK] & (CORRECTION_HISTORY_SIZE - 1)] += scaledBonus;

    // Minor / Major
    scaledBonus = bonus - minorCorrectionHistory[board->stm][board->hashes.minorHash & (CORRECTION_HISTORY_SIZE - 1)] * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
    minorCorrectionHistory[board->stm][board->hashes.minorHash & (CORRECTION_HISTORY_SIZE - 1)] += scaledBonus;
    scaledBonus = bonus - majorCorrectionHistory[board->stm][board->hashes.majorHash & (CORRECTION_HISTORY_SIZE - 1)] * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
    majorCorrectionHistory[board->stm][board->hashes.majorHash & (CORRECTION_HISTORY_SIZE - 1)] += scaledBonus;

    // Continuation
    if ((searchStack - 1)->movedPiece != Piece::NONE) {
        scaledBonus = bonus - *(searchStack - 1)->contCorrHist * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
        *(searchStack - 1)->contCorrHist += scaledBonus;
    }
}

int History::getHistory(Board* board, SearchStack* searchStack, Move move, bool isCapture) {
    if (isCapture) {
        return *getCaptureHistory(board, move);
    }
    else {
        return getQuietHistory(move, board->stm, board) + 2 * getContinuationHistory(searchStack, board->stm, board->pieces[move.origin()], move) + getPawnHistory(board, move);
    }
}

int16_t History::getQuietHistory(Move move, Color stm, Board* board) {
    Square origin = move.origin(), target = move.target();
    return quietHistory[stm][origin][board->isSquareThreatened(origin)][target][board->isSquareThreatened(target)];
}

void History::updateQuietHistory(Move move, Color stm, Board* board, int16_t bonus) {
    int16_t scaledBonus = bonus - getQuietHistory(move, stm, board) * std::abs(bonus) / 32000;
    Square origin = move.origin(), target = move.target();
    quietHistory[stm][origin][board->isSquareThreatened(origin)][target][board->isSquareThreatened(target)] += scaledBonus;
}

int16_t History::getPawnHistory(Board* board, Move move) {
    return pawnHistory[board->hashes.pawnHash & (PAWN_HISTORY_SIZE - 1)][board->stm][board->pieces[move.origin()]][move.target()];
}

void History::updatePawnHistory(Board* board, Move move, int16_t bonus) {
    int16_t scaledBonus = bonus - getPawnHistory(board, move) * std::abs(bonus) / 32000;
    pawnHistory[board->hashes.pawnHash & (PAWN_HISTORY_SIZE - 1)][board->stm][board->pieces[move.origin()]][move.target()] += scaledBonus;
}

int History::getContinuationHistory(SearchStack* stack, Color side, Piece piece, Move move) {
    assert(piece != Piece::NONE);
    Square target = move.target();

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
    Square target = move.target();

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
    Piece movedPiece = board->pieces[move.origin()];
    Piece capturedPiece = board->pieces[move.target()];
    Square target = move.target();

    if (capturedPiece == Piece::NONE && move.isSpecial()) // for ep and promotions, just take pawns
        capturedPiece = Piece::PAWN;

    assert(movedPiece != Piece::NONE && capturedPiece != Piece::NONE);

    return &captureHistory[board->stm][movedPiece][target][capturedPiece];
}

void History::updateSingleCaptureHistory(Board* board, Move move, int16_t bonus) {
    int16_t* captHistScore = getCaptureHistory(board, move);

    int16_t scaledBonus = bonus - *captHistScore * std::abs(bonus) / 32000;
    *captHistScore += scaledBonus;
}

void History::updateCaptureHistory(Depth depth, Board* board, Move bestMove, int bestMoveSearchCount, SearchedMoveList& searchedCaptures) {
    int captHistBonus = std::min(historyBonusCaptureBase + historyBonusCaptureFactor * depth, historyBonusCaptureMax);
    int captHistMalus = std::min(historyMalusCaptureBase + historyMalusCaptureFactor * depth, historyMalusCaptureMax);

    if (board->isCapture(bestMove)) {
        updateSingleCaptureHistory(board, bestMove, captHistBonus * bestMoveSearchCount);
    }

    for (auto [move, moveSearchCount] : searchedCaptures) {
        if (move == bestMove) continue;
        updateSingleCaptureHistory(board, move, -captHistMalus * moveSearchCount);
    }
}

void History::updateQuietHistories(Depth depth, Board* board, SearchStack* stack, Move bestMove, int bestMoveSearchCount, SearchedMoveList& searchedQuiets) {
    int quietHistBonus = std::min(historyBonusQuietBase + historyBonusQuietFactor * depth, historyBonusQuietMax);
    int quietHistMalus = std::min(historyMalusQuietBase + historyMalusQuietFactor * depth, historyMalusQuietMax);
    int contHistBonus = std::min(historyBonusContinuationBase + historyBonusContinuationFactor * depth, historyBonusContinuationMax);
    int contHistMalus = std::min(historyMalusContinuationBase + historyMalusContinuationFactor * depth, historyMalusContinuationMax);
    int pawnHistBonus = std::min(historyBonusPawnBase + historyBonusPawnFactor * depth, historyBonusPawnMax);
    int pawnHistMalus = std::min(historyMalusPawnBase + historyMalusPawnFactor * depth, historyMalusPawnMax);
    
    // Increase stats for this move
    updateQuietHistory(bestMove, board->stm, board, quietHistBonus * bestMoveSearchCount);
    updateContinuationHistory(stack, board->stm, board->pieces[bestMove.origin()], bestMove, contHistBonus * bestMoveSearchCount);
    updatePawnHistory(board, bestMove, pawnHistBonus * bestMoveSearchCount);

    // Decrease stats for all other quiets
    for (auto [move, moveSearchCount] : searchedQuiets) {
        if (move == bestMove) continue;
        updateQuietHistory(move, board->stm, board, -quietHistMalus * moveSearchCount);
        updateContinuationHistory(stack, board->stm, board->pieces[move.origin()], move, -contHistMalus * moveSearchCount);
        updatePawnHistory(board, move, -pawnHistMalus * moveSearchCount);
    }
}

Move History::getCounterMove(Move move) {
    return counterMoves[move.origin()][move.target()];
}

void History::setCounterMove(Move move, Move counter) {
    counterMoves[move.origin()][move.target()] = counter;
}