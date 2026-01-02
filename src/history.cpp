#include <string.h>
#include <cassert>

#include "history.h"
#include "types.h"
#include "move.h"
#include "evaluation.h"
#include "spsa.h"
#include "utils.h"

// Quiet history
TUNE_INT(historyBonusQuietBase, 140, 0, 250);
TUNE_INT(historyBonusQuietFactor, 264, 1, 500);
TUNE_INT(historyBonusQuietMax, 2150, 1, 4000);
TUNE_INT(historyMalusQuietBase, 106, 0, 250);
TUNE_INT(historyMalusQuietFactor, 239, 1, 500);
TUNE_INT(historyMalusQuietMax, 1520, 1, 3000);

// Continuation history
TUNE_INT(historyBonusContinuationBase, -77, -200, 0);
TUNE_INT(historyBonusContinuationFactor, 132, 1, 250);
TUNE_INT(historyBonusContinuationMax, 2035, 1, 4000);
TUNE_INT(historyMalusContinuationBase, 102, 0, 200);
TUNE_INT(historyMalusContinuationFactor, 273, 1, 500);
TUNE_INT(historyMalusContinuationMax, 835, 1, 1500);

// Pawn history
TUNE_INT(historyBonusPawnBase, 39, -100, 100);
TUNE_INT(historyBonusPawnFactor, 170, 1, 250);
TUNE_INT(historyBonusPawnMax, 2070, 1, 4000);
TUNE_INT(historyMalusPawnBase, 29, -100, 100);
TUNE_INT(historyMalusPawnFactor, 288, 1, 500);
TUNE_INT(historyMalusPawnMax, 2148, 1, 4000);

// Capture history
TUNE_INT(historyBonusCaptureBase, 14, -100, 100);
TUNE_INT(historyBonusCaptureFactor, 115, 1, 250);
TUNE_INT(historyBonusCaptureMax, 1325, 1, 2500);
TUNE_INT(historyMalusCaptureBase, 97, 0, 200);
TUNE_INT(historyMalusCaptureFactor, 236, 1, 500);
TUNE_INT(historyMalusCaptureMax, 1486, 1, 3000);

// Correction history
TUNE_INT(pawnCorrectionFactor, 6429, 1000, 7500);
TUNE_INT(nonPawnCorrectionFactor, 5827, 1000, 7500);
TUNE_INT(minorCorrectionFactor, 4178, 1000, 7500);
TUNE_INT(majorCorrectionFactor, 2295, 1000, 7500);
TUNE_INT(continuationCorrectionFactor, 5762, 1000, 7500);

SharedHistory::SharedHistory(int _threadsOnNode): threadsOnNode(_threadsOnNode) {
    threadsPowerOfTwo = _threadsOnNode > 1 ? (2ULL << msb(_threadsOnNode - 1)) : 1;

    for (Color side : {Color::WHITE, Color::BLACK}) {
        correctionHistory[side] = reinterpret_cast<std::atomic<int16_t>*>(alignedAlloc(64, threadsPowerOfTwo * CORRECTION_HISTORY_SIZE * sizeof(int16_t)));
        minorCorrectionHistory[side] = reinterpret_cast<std::atomic<int16_t>*>(alignedAlloc(64, threadsPowerOfTwo * CORRECTION_HISTORY_SIZE * sizeof(int16_t)));
        majorCorrectionHistory[side] = reinterpret_cast<std::atomic<int16_t>*>(alignedAlloc(64, threadsPowerOfTwo * CORRECTION_HISTORY_SIZE * sizeof(int16_t)));
        nonPawnCorrectionHistory[side][Color::WHITE] = reinterpret_cast<std::atomic<int16_t>*>(alignedAlloc(64, threadsPowerOfTwo * CORRECTION_HISTORY_SIZE * sizeof(int16_t)));
        nonPawnCorrectionHistory[side][Color::BLACK] = reinterpret_cast<std::atomic<int16_t>*>(alignedAlloc(64, threadsPowerOfTwo * CORRECTION_HISTORY_SIZE * sizeof(int16_t)));
        continuationCorrectionHistory[side] = reinterpret_cast<std::atomic<int16_t>*>(alignedAlloc(64, CONTCORR_HISTORY_SIZE * sizeof(int16_t)));
    }

    hashMask = threadsPowerOfTwo * CORRECTION_HISTORY_SIZE - 1;
}

void SharedHistory::free() {
    for (Color side : {Color::WHITE, Color::BLACK}) {
        alignedFree(correctionHistory[side]);
        alignedFree(minorCorrectionHistory[side]);
        alignedFree(majorCorrectionHistory[side]);
        alignedFree(nonPawnCorrectionHistory[side][Color::WHITE]);
        alignedFree(nonPawnCorrectionHistory[side][Color::BLACK]);
        alignedFree(continuationCorrectionHistory[side]);
    }
}

void SharedHistory::initHistory(int threadIdx) {
    size_t size = threadsPowerOfTwo * CORRECTION_HISTORY_SIZE / threadsOnNode;
    size_t start = threadIdx * size;
    if (threadIdx == threadsOnNode - 1)
        size = threadsPowerOfTwo * CORRECTION_HISTORY_SIZE - start;

    for (Color side : {Color::WHITE, Color::BLACK}) {
        memset(&correctionHistory[side][start], 0, size * sizeof(int16_t));
        memset(&minorCorrectionHistory[side][start], 0, size * sizeof(int16_t));
        memset(&majorCorrectionHistory[side][start], 0, size * sizeof(int16_t));
        memset(&nonPawnCorrectionHistory[side][Color::WHITE][start], 0, size * sizeof(int16_t));
        memset(&nonPawnCorrectionHistory[side][Color::BLACK][start], 0, size * sizeof(int16_t));

        if (threadIdx == 0)
            memset(&continuationCorrectionHistory[side][0], 0, CONTCORR_HISTORY_SIZE * sizeof(int16_t));
    }
}

int16_t SharedHistory::getPawnCorrectionEntry(Board* board) {
    return correctionHistory[board->stm][board->hashes.pawnHash & hashMask].load(std::memory_order_relaxed);
}

void SharedHistory::storePawnCorrectionEntry(Board* board, int16_t value) {
    correctionHistory[board->stm][board->hashes.pawnHash & hashMask].store(value, std::memory_order_relaxed);
}

int16_t SharedHistory::getNonPawnCorrectionEntryWhite(Board* board) {
    return nonPawnCorrectionHistory[board->stm][Color::WHITE][board->hashes.nonPawnHash[Color::WHITE] & hashMask].load(std::memory_order_relaxed);
}

void SharedHistory::storeNonPawnCorrectionEntryWhite(Board* board, int16_t value) {
    nonPawnCorrectionHistory[board->stm][Color::WHITE][board->hashes.nonPawnHash[Color::WHITE] & hashMask].store(value, std::memory_order_relaxed);
}

int16_t SharedHistory::getNonPawnCorrectionEntryBlack(Board* board) {
    return nonPawnCorrectionHistory[board->stm][Color::BLACK][board->hashes.nonPawnHash[Color::BLACK] & hashMask].load(std::memory_order_relaxed);
}

void SharedHistory::storeNonPawnCorrectionEntryBlack(Board* board, int16_t value) {
    nonPawnCorrectionHistory[board->stm][Color::BLACK][board->hashes.nonPawnHash[Color::BLACK] & hashMask].store(value, std::memory_order_relaxed);
}

int16_t SharedHistory::getMinorCorrectionEntry(Board* board) {
    return minorCorrectionHistory[board->stm][board->hashes.minorHash & hashMask].load(std::memory_order_relaxed);
}

void SharedHistory::storeMinorCorrectionEntry(Board* board, int16_t value) {
    minorCorrectionHistory[board->stm][board->hashes.minorHash & hashMask].store(value, std::memory_order_relaxed);
}

int16_t SharedHistory::getMajorCorrectionEntry(Board* board) {
    return majorCorrectionHistory[board->stm][board->hashes.majorHash & hashMask].load(std::memory_order_relaxed);
}

void SharedHistory::storeMajorCorrectionEntry(Board* board, int16_t value) {
    majorCorrectionHistory[board->stm][board->hashes.majorHash & hashMask].store(value, std::memory_order_relaxed);
}

std::atomic<int16_t>* SharedHistory::getContinuationCorrectionPointer(Board* board, Move move) {
    if (!move) return &continuationCorrectionHistory[board->stm][0];

    int pieceIdx = board->pieces[move.origin()];
    int targetIdx = move.target();
    int originThreatenedIdx = board->isSquareThreatened(move.origin());
    int targetThreatenedIdx = board->isSquareThreatened(move.target());

    int idx = 256 * pieceIdx + 4 * targetIdx + 2 * originThreatenedIdx + targetThreatenedIdx;
    return &continuationCorrectionHistory[board->stm][idx];
}

void History::initHistory() {
    memset(quietHistory, 0, sizeof(quietHistory));
    memset(counterMoves, 0, sizeof(counterMoves));
    memset(continuationHistory, 0, sizeof(continuationHistory));
    memset(captureHistory, 0, sizeof(captureHistory));

    for (int i = 0; i < PAWN_HISTORY_SIZE; i++) {
        for (int j = 0; j < 2; j++) {
            for (int k = 0; k < Piece::TOTAL; k++) {
                for (int l = 0; l < 64; l++) {
                    pawnHistory[i][j][k][l] = -1000;
                }
            }
        }
    }

    sharedHistory->initHistory(threadIdx);
}

Eval History::getCorrectionValue(Board* board, SearchStack* searchStack) {
    auto pawnEntry = sharedHistory->getPawnCorrectionEntry(board);
    int64_t nonPawnEntry = sharedHistory->getNonPawnCorrectionEntryWhite(board) + sharedHistory->getNonPawnCorrectionEntryBlack(board);
    int64_t minorEntry = sharedHistory->getMinorCorrectionEntry(board);
    int64_t majorEntry = sharedHistory->getMajorCorrectionEntry(board);
    int64_t contEntry = (searchStack - 1)->movedPiece != Piece::NONE ? (searchStack - 1)->contCorrHist->load(std::memory_order_relaxed) : 0;

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
    int16_t value = sharedHistory->getPawnCorrectionEntry(board);
    Eval scaledBonus = bonus - value * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
    sharedHistory->storePawnCorrectionEntry(board, value + scaledBonus);

    // Non-Pawn
    value = sharedHistory->getNonPawnCorrectionEntryWhite(board);
    scaledBonus = bonus - value * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
    sharedHistory->storeNonPawnCorrectionEntryWhite(board, value + scaledBonus);

    value = sharedHistory->getNonPawnCorrectionEntryBlack(board);
    scaledBonus = bonus - value * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
    sharedHistory->storeNonPawnCorrectionEntryBlack(board, value + scaledBonus);

    // Minor / Major
    value = sharedHistory->getMinorCorrectionEntry(board);
    scaledBonus = bonus - value * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
    sharedHistory->storeMinorCorrectionEntry(board, value + scaledBonus);

    value = sharedHistory->getMajorCorrectionEntry(board);
    scaledBonus = bonus - value * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
    sharedHistory->storeMajorCorrectionEntry(board, value + scaledBonus);

    // Continuation
    if ((searchStack - 1)->movedPiece != Piece::NONE) {
        value = (searchStack - 1)->contCorrHist->load(std::memory_order_relaxed);
        scaledBonus = bonus - value / CORRECTION_HISTORY_LIMIT;
        (searchStack - 1)->contCorrHist->store(value + scaledBonus, std::memory_order_relaxed);
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

    if (capturedPiece == Piece::NONE && move.type() != MoveType::NORMAL) // for ep and promotions, just take pawns
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

    for (auto& [move, moveSearchCount] : searchedCaptures) {
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
    for (auto& [move, moveSearchCount] : searchedQuiets) {
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