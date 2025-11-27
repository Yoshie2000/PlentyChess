#include <string.h>
#include <cassert>

#include "history.h"
#include "types.h"
#include "movepicker.h"
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

auto& getEntry(int16_t* table, Hash hash) {
    return table[hash & (CORRECTION_HISTORY_SIZE - 1)];
};

Value History::getCorrectionValue(Board* board, SearchStack* searchStack) {
    auto pawnEntry = getEntry(correctionHistory[board->stm], board->hashes.pawnHash);
    auto nonPawnEntry = getEntry(nonPawnCorrectionHistory[board->stm][Color::WHITE], board->hashes.nonPawnHash[Color::WHITE]) + getEntry(nonPawnCorrectionHistory[board->stm][Color::BLACK], board->hashes.nonPawnHash[Color::BLACK]);
    auto minorEntry = getEntry(minorCorrectionHistory[board->stm], board->hashes.minorHash);
    auto majorEntry = getEntry(majorCorrectionHistory[board->stm], board->hashes.majorHash);
    auto contEntry = (searchStack - 1)->movedPiece != Piece::NONE ? *((searchStack - 1)->contCorrHist) : 0;

    return pawnEntry * pawnCorrectionFactor + nonPawnEntry * nonPawnCorrectionFactor + minorEntry * minorCorrectionFactor + majorEntry * majorCorrectionFactor + contEntry * continuationCorrectionFactor;
}

Value History::correctStaticEval(uint8_t rule50, Value eval, Value correctionValue) {
    eval = eval * (300 - rule50) / 300;
    eval += correctionValue / 65536;
    return std::clamp<Value>(eval, -Eval::TBWIN_IN_MAX_PLY + 1, Eval::TBWIN_IN_MAX_PLY - 1);
}

void History::updateCorrectionHistory(Board* board, SearchStack* searchStack, int16_t bonus) {
    // Pawn
    Value scaledBonus = bonus - getEntry(correctionHistory[board->stm], board->hashes.pawnHash) * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
    getEntry(correctionHistory[board->stm], board->hashes.pawnHash) += scaledBonus;

    // Non-Pawn
    scaledBonus = bonus - getEntry(nonPawnCorrectionHistory[board->stm][Color::WHITE], board->hashes.nonPawnHash[Color::WHITE]) * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
    getEntry(nonPawnCorrectionHistory[board->stm][Color::WHITE], board->hashes.nonPawnHash[Color::WHITE]) += scaledBonus;
    scaledBonus = bonus - getEntry(nonPawnCorrectionHistory[board->stm][Color::BLACK], board->hashes.nonPawnHash[Color::BLACK]) * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
    getEntry(nonPawnCorrectionHistory[board->stm][Color::BLACK], board->hashes.nonPawnHash[Color::BLACK]) += scaledBonus;

    // Minor / Major
    scaledBonus = bonus - getEntry(minorCorrectionHistory[board->stm], board->hashes.minorHash) * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
    getEntry(minorCorrectionHistory[board->stm], board->hashes.minorHash) += scaledBonus;
    scaledBonus = bonus - getEntry(majorCorrectionHistory[board->stm], board->hashes.majorHash) * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
    getEntry(majorCorrectionHistory[board->stm], board->hashes.majorHash) += scaledBonus;

    // Continuation
    if ((searchStack - 1)->movedPiece != Piece::NONE) {
        scaledBonus = bonus - *(searchStack - 1)->contCorrHist * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
        *(searchStack - 1)->contCorrHist += scaledBonus;
    }
}

int History::getHistory(Board* board, SearchStack* searchStack, Move move, bool isCapture) {
    if (isCapture) {
        return getCaptureHistory(board, move);
    }
    else {
        return getQuietHistory(move, board->stm, board) + 2 * getContinuationHistory(searchStack, board->stm, board->pieces[move.origin()], move) + getPawnHistory(board, move);
    }
}

int16_t& History::getQuietHistory(Move move, Color stm, Board* board) {
    Square origin = move.origin();
    Square target = move.target();
    return quietHistory[stm][origin][board->isSquareThreatened(origin)][target][board->isSquareThreatened(target)];
}

void History::updateQuietHistory(Move move, Color stm, Board* board, int16_t bonus) {
    auto& quietHistory = getQuietHistory(move, stm, board);
    quietHistory += bonus - quietHistory * std::abs(bonus) / 32000;
}

int16_t& History::getPawnHistory(Board* board, Move move) {
    Hash pawnHash = board->hashes.pawnHash & (PAWN_HISTORY_SIZE - 1);
    return pawnHistory[pawnHash][board->stm][board->pieces[move.origin()]][move.target()];
}

void History::updatePawnHistory(Board* board, Move move, int16_t bonus) {
    auto& pawnHistory = getPawnHistory(board, move);
    pawnHistory += bonus - pawnHistory * std::abs(bonus) / 32000;
}

auto getPieceTo(Piece piece, Square target, Color color) {
    return 2 * 64 * piece + 2 * target + color;
}

int History::getContinuationHistory(SearchStack* stack, Color side, Piece piece, Move move) {
    assert(piece != Piece::NONE);

    int score = 0;
    int pieceTo = getPieceTo(piece, move.target(), side);

    constexpr std::pair<int, int> indices[] = {
        {1, 1},
        {2, 2},
        {4, 2},
        {6, 4}
    };

    for (auto [idx, div] : indices) {
        if ((stack - idx)->movedPiece != Piece::NONE) {
            score += 2 * (stack - idx)->contHist[pieceTo] / div;
        }
    }

    return score;
}

void History::updateContinuationHistory(SearchStack* stack, Color side, Piece piece, Move move, int16_t bonus) {
    assert(piece != Piece::NONE);

    int scaledBonus = bonus - getContinuationHistory(stack, side, piece, move) * std::abs(bonus) / 32000;
    int pieceTo = getPieceTo(piece, move.target(), side);

    constexpr std::pair<int, int> indices[] = {
        {1, 1},
        {2, 1},
        {3, 4},
        {4, 1},
        {6, 2}
    };

    for (auto [idx, div] : indices) {
        if ((stack - idx)->movedPiece != Piece::NONE) {
            (stack - idx)->contHist[pieceTo] += scaledBonus / div;
        }
    }
}

int16_t& History::getCaptureHistory(Board* board, Move move) {
    Piece movedPiece = board->pieces[move.origin()];
    Piece capturedPiece = board->pieces[move.target()];
    Square target = move.target();

    if (capturedPiece == Piece::NONE && move.type() != MoveType::NORMAL) // for ep and promotions, just take pawns
        capturedPiece = Piece::PAWN;

    assert(movedPiece != Piece::NONE && capturedPiece != Piece::NONE);

    return captureHistory[board->stm][movedPiece][target][capturedPiece];
}

void History::updateSingleCaptureHistory(Board* board, Move move, int16_t bonus) {
    auto& captHistScore = getCaptureHistory(board, move);
    captHistScore += bonus - captHistScore * std::abs(bonus) / 32000;
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