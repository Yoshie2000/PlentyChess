#include <string.h>
#include <cassert>

#include "history.h"
#include "types.h"
#include "move.h"
#include "evaluation.h"
#include "spsa.h"

TUNE_INT(contHistBonusFactor, 100, 50, 150);
TUNE_INT(contHistMalusFactor, 100, 50, 150);
TUNE_INT(correctionHistoryDivisor, 10000, 5000, 20000);

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
    memset(pawnHistory, -1000, sizeof(pawnHistory));
}

Eval History::correctStaticEval(Eval eval, Board* board) {
    Eval history = getCorrectionHistory(board);
    Eval adjustedEval = eval + (history * std::abs(history)) / correctionHistoryDivisor;
    adjustedEval = std::clamp((int)adjustedEval, (int)-EVAL_MATE_IN_MAX_PLY + 1, (int)EVAL_MATE_IN_MAX_PLY - 1);
    return adjustedEval;
}

void History::updateCorrectionHistory(Board* board, int16_t bonus) {
    Eval scaledBonus = (Eval)bonus - getCorrectionHistory(board) * std::abs(bonus) / CORRECTION_HISTORY_LIMIT;
    correctionHistory[board->stm][board->stack->pawnHash & (CORRECTION_HISTORY_SIZE - 1)] += scaledBonus;
}

int History::getHistory(Board* board, SearchStack* searchStack, Move move, bool isCapture) {
    if (isCapture) {
        std::pair<int16_t*, int16_t*> captureHistory = getCaptureHistory(board, move);
        return *captureHistory.first + *captureHistory.second / 2;
    }
    else {
        return getQuietHistory(board, move) + 2 * getContinuationHistory(searchStack, board->pieces[moveOrigin(move)], move) + getPawnHistory(board, move);
    }
}

int16_t History::getQuietHistory(Board* board, Move move) {
    Square origin = moveOrigin(move), target = moveTarget(move);
    return quietHistory[board->stm][origin][board->isSquareThreatened(origin)][target][board->isSquareThreatened(target)];
}

void History::updateQuietHistory(Board* board, Move move, int16_t bonus) {
    int16_t scaledBonus = bonus - getQuietHistory(board, move) * std::abs(bonus) / 32000;
    Square origin = moveOrigin(move), target = moveTarget(move);
    quietHistory[board->stm][origin][board->isSquareThreatened(origin)][target][board->isSquareThreatened(target)] += scaledBonus;
}

int16_t History::getPawnHistory(Board* board, Move move) {
    return pawnHistory[board->stack->pawnHash & (PAWN_HISTORY_SIZE - 1)][board->stm][board->pieces[moveOrigin(move)]][moveTarget(move)];
}

void History::updatePawnHistory(Board* board, Move move, int16_t bonus) {
    int16_t scaledBonus = bonus - getPawnHistory(board, move) * std::abs(bonus) / 32000;
    pawnHistory[board->stack->pawnHash & (PAWN_HISTORY_SIZE - 1)][board->stm][board->pieces[moveOrigin(move)]][moveTarget(move)] += scaledBonus;
}

int History::getContinuationHistory(SearchStack* stack, Piece piece, Move move) {
    assert(piece != Piece::NONE);
    Square target = moveTarget(move);

    int score = 0;
    int pieceTo = 64 * piece + target;

    if ((stack - 1)->movedPiece != Piece::NONE)
        score += (stack - 1)->contHist[pieceTo];

    if ((stack - 2)->movedPiece != Piece::NONE)
        score += (stack - 2)->contHist[pieceTo];

    if ((stack - 4)->movedPiece != Piece::NONE)
        score += (stack - 4)->contHist[pieceTo];

    return score;
}

void History::updateContinuationHistory(SearchStack* stack, Piece piece, Move move, int16_t bonus) {
    assert(piece != Piece::NONE);
    Square target = moveTarget(move);

    int16_t scaledBonus = bonus - getContinuationHistory(stack, piece, move) * std::abs(bonus) / 32000;
    int pieceTo = 64 * piece + target;

    if ((stack - 1)->movedPiece != Piece::NONE)
        (stack - 1)->contHist[pieceTo] += scaledBonus;

    if ((stack - 2)->movedPiece != Piece::NONE)
        (stack - 2)->contHist[pieceTo] += scaledBonus;

    if ((stack - 3)->movedPiece != Piece::NONE)
        (stack - 3)->contHist[pieceTo] += scaledBonus / 4;

    if ((stack - 4)->movedPiece != Piece::NONE)
        (stack - 4)->contHist[pieceTo] += scaledBonus;
}

std::pair<int16_t*, int16_t*> History::getCaptureHistory(Board* board, Move move) {
    Piece movedPiece = board->pieces[moveOrigin(move)];
    Piece capturedPiece = board->pieces[moveTarget(move)];
    Square origin = moveOrigin(move);
    Square target = moveTarget(move);

    if (capturedPiece == Piece::NONE && (move & 0x3000) != 0) // for ep and promotions, just take pawns
        capturedPiece = Piece::PAWN;

    Bitboard occupied = (board->byColor[Color::WHITE] | board->byColor[Color::BLACK]) ^ bitboard(origin);
    Bitboard opponentAttackers = board->attackersTo(target, occupied) & board->byColor[flip(board->stm)];

    // Find the least valuable enemy piece that can recapture on the target square (if any)
    Piece recapturePiece;
    for (recapturePiece = Piece::PAWN; recapturePiece < Piece::NONE; ++recapturePiece) {
        if (opponentAttackers & board->byPiece[recapturePiece])
            break;
    }

    assert(movedPiece != Piece::NONE && capturedPiece != Piece::NONE);
    assert(recapturePiece != Piece::ANY);

    return std::make_pair(&captureHistory[board->stm][movedPiece][target][capturedPiece][Piece::ANY], &captureHistory[board->stm][movedPiece][target][capturedPiece][recapturePiece]);
}

void History::updateSingleCaptureHistory(Board* board, Move move, int16_t bonus) {
    std::pair<int16_t*, int16_t*> captHistScore = getCaptureHistory(board, move);
    int totalScore = *captHistScore.first + *captHistScore.second / 2;

    int16_t scaledBonus = bonus - totalScore * std::abs(bonus) / 32000;
    *captHistScore.first += scaledBonus;
    *captHistScore.second += scaledBonus;
}

void History::updateCaptureHistory(Board* board, Move move, int16_t bonus, Move* captureMoves, int captureMoveCount) {
    if (board->isCapture(move)) {
        updateSingleCaptureHistory(board, move, bonus);
    }

    for (int i = 0; i < captureMoveCount; i++) {
        Move cMove = captureMoves[i];
        if (move == cMove) continue;
        updateSingleCaptureHistory(board, cMove, -bonus);
    }
}

void History::updateQuietHistories(Board* board, SearchStack* stack, Move move, int16_t bonus, Move* quietMoves, int quietMoveCount) {
    // Increase stats for this move
    updateQuietHistory(board, move, bonus);
    updateContinuationHistory(stack, board->pieces[moveOrigin(move)], move, bonus * 100 / contHistBonusFactor);
    updatePawnHistory(board, move, bonus);

    // Decrease stats for all other quiets
    for (int i = 0; i < quietMoveCount; i++) {
        Move qMove = quietMoves[i];
        if (move == qMove) continue;
        updateQuietHistory(board, qMove, -bonus);
        updateContinuationHistory(stack, board->pieces[moveOrigin(qMove)], qMove, -bonus * 100 / contHistMalusFactor);
        updatePawnHistory(board, qMove, -bonus);
    }
}

Move History::getCounterMove(Move move) {
    return counterMoves[moveOrigin(move)][moveTarget(move)];
}

void History::setCounterMove(Move move, Move counter) {
    counterMoves[moveOrigin(move)][moveTarget(move)] = counter;
}