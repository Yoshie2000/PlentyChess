#include <cstdint>
#include <algorithm>
#include <inttypes.h>
#include <cassert>
#include <chrono>
#include <cmath>
#include <thread>
#include <map>

#include <chrono>
#include <thread>

#include "search.h"
#include "board.h"
#include "move.h"
#include "evaluation.h"
#include "thread.h"
#include "tt.h"
#include "time.h"
#include "spsa.h"
#include "nnue.h"
#include "uci.h"
#include "fathom/src/tbprobe.h"
#include "zobrist.h"
#include "debug.h"

// Time management
TUNE_FLOAT_DISABLED(tmInitialAdjustment, 1.1159139860557399f, 0.5f, 1.5f);
TUNE_INT_DISABLED(tmBestMoveStabilityMax, 18, 10, 30);
TUNE_FLOAT_DISABLED(tmBestMoveStabilityBase, 1.5629851017629899f, 0.75f, 2.5f);
TUNE_FLOAT_DISABLED(tmBestMoveStabilityFactor, 0.05249883159776376f, 0.001f, 0.1f);
TUNE_FLOAT_DISABLED(tmEvalDiffBase, 0.9534973621447117f, 0.5f, 1.5f);
TUNE_FLOAT_DISABLED(tmEvalDiffFactor, 0.004154441079214531f, 0.001f, 0.1f);
TUNE_INT_DISABLED(tmEvalDiffMin, -8, -250, 50);
TUNE_INT_DISABLED(tmEvalDiffMax, 63, -50, 250);
TUNE_FLOAT_DISABLED(tmNodesBase, 1.6802040851149551f, 0.5f, 5.0f);
TUNE_FLOAT_DISABLED(tmNodesFactor, 0.9741686475516691f, 0.1f, 2.5f);

// Aspiration windows
TUNE_INT_DISABLED(aspirationWindowMinDepth, 4, 2, 6);
TUNE_INT_DISABLED(aspirationWindowDelta, 14, 1, 30);
TUNE_INT_DISABLED(aspirationWindowDeltaBase, 10, 1, 30);
TUNE_INT_DISABLED(aspirationWindowMaxFailHighs, 3, 1, 10);
TUNE_FLOAT(aspirationWindowDeltaFactor, 1.6824316885254968f, 1.0f, 3.0f);
TUNE_INT(aspirationWindowDeltaDivisor, 13052, 7500, 17500);

// Reduction / Margin tables
TUNE_FLOAT(lmrReductionNoisyBase, -0.16746504757915998f, -1.0f, 1.0f);
TUNE_FLOAT(lmrReductionNoisyFactor, 3.015294386647946f, 2.0f, 4.0f);
TUNE_FLOAT(lmrReductionImportantNoisyBase, -0.18494142853230522f, -1.0f, 1.0f);
TUNE_FLOAT(lmrReductionImportantNoisyFactor, 3.1771025906820594f, 2.0f, 4.0f);
TUNE_FLOAT(lmrReductionQuietBase, 1.1156812184145881f, 0.0f, 2.0f);
TUNE_FLOAT(lmrReductionQuietFactor, 2.9348373864040274f, 2.0f, 4.0f);

TUNE_FLOAT(lmpMarginWorseningBase, 1.976556330827873f, 0.0f, 3.5f);
TUNE_FLOAT(lmpMarginWorseningFactor, 0.4409114850475385f, 0.1f, 1.5f);
TUNE_FLOAT(lmpMarginWorseningPower, 1.539323819754223f, 0.0f, 4.0f);
TUNE_FLOAT(lmpMarginImprovingBase, 2.837411229046308f, 2.0f, 5.0f);
TUNE_FLOAT(lmpMarginImprovingFactor, 0.8604609467433942f, 0.5f, 2.0f);
TUNE_FLOAT(lmpMarginImprovingPower, 1.9566999909630995f, 1.0f, 3.0f);

// Search values
TUNE_INT(qsFutilityOffset, 83, 1, 125);
TUNE_INT(qsSeeMargin, -68, -200, 50);

// Pre-search pruning
TUNE_INT(ttCutOffset, 43, 0, 100);
TUNE_INT(ttCutFailHighMargin, 123, 0, 240);

TUNE_INT(iirMinDepth, 257, 100, 500);
TUNE_INT(iirCheckDepth, 509, 0, 1000);
TUNE_INT(iirLowTtDepthOffset, 437, 0, 850);
TUNE_INT(iirReduction, 90, 0, 200);

TUNE_INT(staticHistoryFactor, -250, -200, -1);
TUNE_INT(staticHistoryMin, -430, -500, -1);
TUNE_INT(staticHistoryMax, 715, 1, 500);
TUNE_INT(staticHistoryTempo, 170, 1, 60);

TUNE_INT(rfpDepthLimit, 1450, 200, 2000);
TUNE_INT(rfpBase, 17, -100, 100);
TUNE_INT(rfpFactorLinear, 31, 1, 60);
TUNE_INT(rfpFactorQuadratic, 700, 1, 1200);
TUNE_INT(rfpImprovingOffset, 101, 1, 200);
TUNE_INT(rfpBaseCheck, -5, -100, 100);
TUNE_INT(rfpFactorLinearCheck, 39, 1, 80);
TUNE_INT(rfpFactorQuadraticCheck, 507, 1, 1200);
TUNE_INT(rfpImprovingOffsetCheck, 98, 1, 200);

TUNE_INT(razoringDepth, 528, 100, 1000);
TUNE_INT(razoringFactor, 267, 1, 500);

TUNE_INT(nmpMinDepth, 355, 0, 700);
TUNE_INT(nmpRedBase, 365, 100, 700);
TUNE_INT(nmpDepthDiv, 246, 100, 500);
TUNE_INT(nmpMin, 380, 100, 700);
TUNE_INT(nmpDivisor, 211, 10, 500);
TUNE_INT_DISABLED(nmpEvalDepth, 7, 1, 100);
TUNE_INT(nmpEvalBase, 164, 50, 350);

TUNE_INT(probcutReduction, 409, 0, 800);
TUNE_INT(probCutBetaOffset, 206, 1, 400);
TUNE_INT(probCutDepth, 560, 100, 1000);

TUNE_INT(iir2Reduction, 101, 0, 200);
TUNE_INT(iir2MinDepth, 266, 100, 500);

// In-search pruning
TUNE_INT(earlyLmrImproving, 123, 1, 260);

TUNE_INT(earlyLmrHistoryFactorQuiet, 15842, 10000, 20000);
TUNE_INT(earlyLmrHistoryFactorCapture, 14293, 10000, 20000);

TUNE_INT(fpDepth, 1097, 100, 2000);
TUNE_INT(fpBase, 295, 1, 600);
TUNE_INT(fpFactor, 70, 1, 150);
TUNE_INT(fpPvNode, 36, 1, 80);
TUNE_INT(fpPvNodeBadCapture, 117, 1, 250);

TUNE_INT(fpCaptDepth, 846, 100, 1500);
TUNE_INT(fpCaptBase, 432, 150, 800);
TUNE_INT(fpCaptFactor, 397, 100, 800);

TUNE_INT(historyPruningDepth, 457, 100, 1000);
TUNE_INT(historyPruningFactorCapture, -2170, -4000, -1);
TUNE_INT(historyPruningFactorQuiet, -6724, -12000, -1);

TUNE_INT(extensionMinDepth, 620, 0, 1200);
TUNE_INT(extensionTtDepthOffset, 470, 0, 800);
TUNE_INT(doubleExtensionDepthIncreaseFactor, 79, 0, 200);
TUNE_INT_DISABLED(doubleExtensionMargin, 6, 1, 30);
TUNE_INT(doubleExtensionDepthIncrease, 1002, 200, 2000);
TUNE_INT_DISABLED(tripleExtensionMargin, 41, 25, 100);

TUNE_INT_DISABLED(lmrMcBase, 2, 1, 10);
TUNE_INT_DISABLED(lmrMcPv, 2, 1, 10);
TUNE_INT(lmrMinDepth, 307, 100, 600);

TUNE_INT(lmrReductionOffsetQuietOrNormalCapture, 145, 0, 300);
TUNE_INT(lmrReductionOffsetImportantCapture, 7, 0, 100);
TUNE_INT(lmrCheckQuietOrNormalCapture, 108, 0, 230);
TUNE_INT(lmrCheckImportantCapture, 58, 0, 120);
TUNE_INT(lmrTtPvQuietOrNormalCapture, 191, 0, 400);
TUNE_INT(lmrTtPvImportantCapture, 197, 0, 400);
TUNE_INT(lmrCutnode, 267, 0, 500);
TUNE_INT(lmrTtpvFaillowQuietOrNormalCapture, 46, 0, 100);
TUNE_INT(lmrTtpvFaillowImportantCapture, 87, 0, 200);
TUNE_INT(lmrCorrectionDivisorQuietOrNormalCapture, 140128, 100000, 200000);
TUNE_INT(lmrCorrectionDivisorImportantCapture, 146432, 100000, 200000);
TUNE_INT(lmrQuietHistoryDivisor, 28908, 10000, 60000);
TUNE_INT(lmrHistoryFactorCapture, 3122217, 2500000, 4000000);
TUNE_INT(lmrHistoryFactorImportantCapture, 3006170, 2500000, 4000000);
TUNE_INT(lmrImportantBadCaptureOffset, 110, 0, 230);
TUNE_INT(lmrImportantCaptureFactor, 31, 0, 60);
TUNE_INT(lmrQuietPvNodeOffset, 19, 0, 50);
TUNE_INT(lmrQuietImproving, 58, 0, 100);

inline int lmrReductionOffset(bool importantCapture) { return importantCapture ? lmrReductionOffsetImportantCapture : lmrReductionOffsetQuietOrNormalCapture; }
inline int lmrCheck(bool importantCapture) { return importantCapture ? lmrCheckImportantCapture : lmrCheckQuietOrNormalCapture; }
inline int lmrTtPv(bool importantCapture) { return importantCapture ? lmrTtPvImportantCapture : lmrTtPvQuietOrNormalCapture; }
inline int lmrTtpvFaillow(bool importantCapture) { return importantCapture ? lmrTtpvFaillowQuietOrNormalCapture : lmrTtpvFaillowImportantCapture; }
inline int lmrCaptureHistoryDivisor(bool importantCapture) { return importantCapture ? lmrHistoryFactorImportantCapture : lmrHistoryFactorCapture; }
inline int lmrCorrectionDivisor(bool importantCapture) { return importantCapture ? lmrCorrectionDivisorImportantCapture : lmrCorrectionDivisorQuietOrNormalCapture; }

TUNE_INT(postlmrOppWorseningThreshold, 240, 150, 450);
TUNE_INT(postlmrOppWorseningReduction, 145, 0, 200);

TUNE_INT(lmrPvNodeExtension, 109, 0, 200);
TUNE_INT_DISABLED(lmrDeeperBase, 40, 1, 100);
TUNE_INT_DISABLED(lmrDeeperFactor, 2, 0, 10);
TUNE_INT(lmrDeeperWeight, 112, 0, 200);
TUNE_INT(lmrShallowerWeight, 111, 0, 200);
TUNE_INT(lmrResearchSkipDepthOffset, 432, 0, 800);

TUNE_INT(lmrPassBonusBase, -293, -500, 0);
TUNE_INT(lmrPassBonusFactor, 154, 1, 300);
TUNE_INT(lmrPassBonusMax, 1012, 0, 2000);

TUNE_INT(historyDepthBetaOffset, 218, 1, 400);

TUNE_INT(lowDepthPvDepthReductionMin, 423, 0, 800);
TUNE_INT(lowDepthPvDepthReductionMax, 1095, 0, 2000);
TUNE_INT(lowDepthPvDepthReductionWeight, 110, 0, 200);

TUNE_INT(correctionHistoryFactor, 120, 0, 300);
TUNE_INT(correctionHistoryFactorMulticut, 164, 0, 300);

int REDUCTIONS[3][MAX_PLY][MAX_MOVES];
int LMP_MARGIN[MAX_PLY][2];

void initReductions() {
    REDUCTIONS[0][0][0] = 0;
    REDUCTIONS[1][0][0] = 0;
    REDUCTIONS[2][0][0] = 0;

    for (int i = 1; i < MAX_PLY; i++) {
        for (int j = 1; j < MAX_MOVES; j++) {
            REDUCTIONS[0][i][j] = 100 * (lmrReductionQuietBase + log(i) * log(j) / lmrReductionQuietFactor); // quiet
            REDUCTIONS[1][i][j] = 100 * (lmrReductionNoisyBase + log(i) * log(j) / lmrReductionNoisyFactor); // non-quiet
            REDUCTIONS[2][i][j] = 100 * (lmrReductionImportantNoisyBase + log(i) * log(j) / lmrReductionImportantNoisyFactor); // important capture
        }
    }

    for (Depth depth = 0; depth < MAX_PLY; depth++) {
        LMP_MARGIN[depth][0] = lmpMarginWorseningBase + lmpMarginWorseningFactor * std::pow(depth, lmpMarginWorseningPower); // non-improving
        LMP_MARGIN[depth][1] = lmpMarginImprovingBase + lmpMarginImprovingFactor * std::pow(depth, lmpMarginImprovingPower); // improving
    }
}

uint64_t perftInternal(Board& board, Depth depth) {
    if (depth == 0) return 1;

    MoveList moves;
    generateMoves(&board, moves);

    uint64_t nodes = 0;
    for (auto& move : moves) {
        if (!board.isLegal(move))
            continue;

        Board boardCopy = board;
        boardCopy.doMove(move, boardCopy.hashAfter(move).first, &UCI::nnue);
        uint64_t subNodes = perftInternal(boardCopy, depth - 1);
        UCI::nnue.decrementAccumulator();

        nodes += subNodes;
    }
    return nodes;
}

uint64_t perft(Board& board, Depth depth) {
    clock_t begin = clock();
    UCI::nnue.reset(&board);

    MoveList moves;
    generateMoves(&board, moves);

    uint64_t nodes = 0;
    for (auto& move : moves) {
        if (!board.isLegal(move))
            continue;

        Board boardCopy = board;
        boardCopy.doMove(move, boardCopy.hashAfter(move).first, &UCI::nnue);
        uint64_t subNodes = perftInternal(boardCopy, depth - 1);
        UCI::nnue.decrementAccumulator();

        std::cout << move.toString(UCI::Options.chess960.value) << ": " << subNodes << std::endl;

        nodes += subNodes;
    }

    clock_t end = clock();
    double time = (double)(end - begin) / CLOCKS_PER_SEC;
    uint64_t nps = nodes / time;
    std::cout << "Perft: " << nodes << " nodes in " << time << "s => " << nps << "nps" << std::endl;

    return nodes;
}

void updatePv(SearchStack* stack, Move move) {
    stack->pvLength = (stack + 1)->pvLength;
    stack->pv[stack->ply] = move;

    for (int i = stack->ply + 1; i < (stack + 1)->pvLength; i++)
        stack->pv[i] = (stack + 1)->pv[i];
}

Eval valueToTT(Eval value, int ply) {
    if (value.score == SCORE_NONE) return value;

    if (value.score >= SCORE_TBWIN_IN_MAX_PLY)
        value = value.withScore(value.score + ply);
    else if (value.score <= -SCORE_TBWIN_IN_MAX_PLY)
        value = value.withScore(value.score - ply);

    return value;
}

Eval valueFromTt(Eval value, int ply, int rule50) {
    if (value.score == SCORE_NONE) return value;

    if (value.score >= SCORE_TBWIN_IN_MAX_PLY) {
        // Downgrade potentially false mate score
        if (value.score >= SCORE_MATE_IN_MAX_PLY && SCORE_MATE - value.score > 100 - rule50)
            return value.withScore(SCORE_TBWIN_IN_MAX_PLY - 1);

        // Downgrade potentially false TB score
        if (SCORE_TBWIN - value.score > 100 - rule50)
            return value.withScore(SCORE_TBWIN_IN_MAX_PLY - 1);

        return value.withScore(value.score - ply);
    }
    else if (value.score <= -SCORE_TBWIN_IN_MAX_PLY) {
        // Downgrade potentially false mate score
        if (value.score <= -SCORE_MATE_IN_MAX_PLY && SCORE_MATE + value.score > 100 - rule50)
            return value.withScore(-SCORE_TBWIN_IN_MAX_PLY + 1);

        // Downgrade potentially false TB score
        if (SCORE_TBWIN + value.score > 100 - rule50)
            return value.withScore(-SCORE_TBWIN_IN_MAX_PLY + 1);

        return value.withScore(value.score + ply);
    }
    return value;
}

Eval drawEval(Worker* thread) {
    Score drawScore = 4 - (thread->searchData.nodesSearched.load(std::memory_order_relaxed) & 3);  // Small overhead to avoid 3-fold blindness
    return Eval(drawScore, 0, 255);
}

Board* Worker::doMove(Board* board, Hash newHash, Move move) {
    Board* boardCopy = board + 1;
    *boardCopy = *board;

    boardCopy->doMove(move, newHash, &nnue);
    boardHistory.push_back(newHash);
    return boardCopy;
}

void Worker::undoMove() {
    nnue.decrementAccumulator();
    boardHistory.pop_back();
}

Board* Worker::doNullMove(Board* board) {
    Board* boardCopy = board + 1;
    *boardCopy = *board;

    boardCopy->doNullMove();
    boardHistory.push_back(boardCopy->hashes.hash);
    return boardCopy;
}

void Worker::undoNullMove() {
    boardHistory.pop_back();
}

// Using cuckoo tables, check if the side to move has any move that would lead to a repetition
bool Worker::hasUpcomingRepetition(Board* board, int ply) {

    int maxPlyOffset = std::min(board->rule50_ply, board->nullmove_ply);
    if (maxPlyOffset < 3)
        return false;

    assert(boardHistory.size() >= 2);

    Hash* compareHash = &boardHistory[boardHistory.size() - 2];

    int j = 0;
    for (int i = 3; i <= maxPlyOffset; i += 2) {
        compareHash -= 2;

        assert(compareHash >= &boardHistory[0]);

        Hash moveHash = board->hashes.hash ^ *compareHash;
        if ((j = Zobrist::H1(moveHash), Zobrist::CUCKOO_HASHES[j] == moveHash) || (j = Zobrist::H2(moveHash), Zobrist::CUCKOO_HASHES[j] == moveHash)) {
            Move move = Zobrist::CUCKOO_MOVES[j];
            Square origin = move.origin();
            Square target = move.target();

            if (BB::BETWEEN[origin][target] & (board->byColor[Color::WHITE] | board->byColor[Color::BLACK]))
                continue;

            if (ply > i)
                return true;

            Square pieceSquare = board->pieces[origin] == Piece::NONE ? target : origin;
            Color pieceColor = (board->byColor[Color::WHITE] & bitboard(pieceSquare)) ? Color::WHITE : Color::BLACK;
            if (pieceColor != board->stm)
                continue;

            // Check for 2-fold repetition
            Hash* compareHash2 = compareHash;
            for (int k = i + 4; k <= maxPlyOffset; k += 2) {
                if (k == i + 4)
                    compareHash2 -= 2;
                compareHash2 -= 2;
                if (*compareHash2 == board->hashes.hash)
                    return true;
            }
        }
    }

    return false;
}

// Checks for 2-fold repetition and rule50 draw
bool Worker::isDraw(Board* board, int ply) {

    // The stack needs to go back far enough
    if (boardHistory.size() < 3)
        return false;

    // 2-fold repetition
    int maxPlyOffset = std::min(board->rule50_ply, board->nullmove_ply);
    Hash* compareHash = &boardHistory[boardHistory.size() - 3];

    bool twofold = false;
    for (int i = 4; i <= maxPlyOffset; i += 2) {
        compareHash -= 2;

        assert(compareHash >= &boardHistory[0]);

        if (board->hashes.hash == *compareHash) {
            if (ply >= i || twofold)
                return true;
            twofold = true;
        }
    }

    // 50 move rule draw
    if (board->rule50_ply > 99) {
        if (!board->checkers)
            return true;

        // If in check, it might be checkmate
        MoveList moves;
        generateMoves(board, moves);
        int legalMoveCount = 0;
        for (auto& move : moves) {
            if (!board->isLegal(move))
                continue;
            legalMoveCount++;
        }

        return legalMoveCount > 0;
    }

    // Otherwise, no draw
    return false;
}

template <NodeType nodeType>
Eval Worker::qsearch(Board* board, SearchStack* stack, Score alpha, Score beta) {
    constexpr bool pvNode = nodeType == PV_NODE;

    if (pvNode)
        stack->pvLength = stack->ply;
    searchData.selDepth = std::max(stack->ply, searchData.selDepth);

    assert(alpha >= -SCORE_INFINITE && alpha < beta && beta <= SCORE_INFINITE);

    if (mainThread && timeOver(searchParameters, searchData))
        threadPool->stopSearching();

    // Check for stop
    if (stopped.load(std::memory_order_relaxed) || exiting || stack->ply >= MAX_PLY - 1 || isDraw(board, stack->ply))
        return (stack->ply >= MAX_PLY - 1 && !board->checkers) ? evaluate(board, &nnue) : drawEval(this);

    stack->inCheck = board->checkerCount > 0;

    // TT Lookup
    bool ttHit = false;
    TTEntry* ttEntry = nullptr;
    Move ttMove = Move::none();
    Eval ttValue = Eval();
    Eval ttEval = Eval();
    uint8_t ttFlag = TT_NOBOUND;
    bool ttPv = pvNode;

    Hash fmrHash = board->hashes.hash ^ Zobrist::FMR[board->rule50_ply / Zobrist::FMR_GRANULARITY];
    ttEntry = TT.probe(fmrHash, &ttHit);
    if (ttHit) {
        ttMove = ttEntry->getMove();
        ttValue = valueFromTt(ttEntry->getValue(), stack->ply, board->rule50_ply);
        ttEval = ttEntry->getEval();
        ttFlag = ttEntry->getFlag();
        ttPv = ttPv || ttEntry->getTtPv();
    }

    // TT cutoff
    if (!pvNode && ttValue.score != SCORE_NONE && ((ttFlag == TT_UPPERBOUND && ttValue.score <= alpha) || (ttFlag == TT_LOWERBOUND && ttValue.score >= beta) || (ttFlag == TT_EXACTBOUND)))
        return ttValue;

    Move bestMove = Move::none();
    Eval bestValue, futilityValue, unadjustedEval;

    int correctionValue = history.getCorrectionValue(board, stack);
    stack->correctionValue = correctionValue;
    if (board->checkers) {
        stack->staticEval = bestValue = unadjustedEval = futilityValue = Eval(-SCORE_INFINITE);

        if (ttValue.score != SCORE_NONE && std::abs(ttValue.score) < SCORE_TBWIN_IN_MAX_PLY && ((ttFlag == TT_UPPERBOUND && ttValue.score < bestValue.score) || (ttFlag == TT_LOWERBOUND && ttValue.score > bestValue.score) || (ttFlag == TT_EXACTBOUND)))
            bestValue = futilityValue = ttValue;

        goto movesLoopQsearch;
    }
    else if (ttHit && ttEval.score != SCORE_NONE) {
        unadjustedEval = ttEval;
        stack->staticEval = bestValue = history.correctStaticEval(board->rule50_ply, unadjustedEval, correctionValue);

        if (ttValue.score != SCORE_NONE && std::abs(ttValue.score) < SCORE_TBWIN_IN_MAX_PLY && ((ttFlag == TT_UPPERBOUND && ttValue.score < bestValue.score) || (ttFlag == TT_LOWERBOUND && ttValue.score > bestValue.score) || (ttFlag == TT_EXACTBOUND)))
            bestValue = ttValue;
    }
    else {
        unadjustedEval = evaluate(board, &nnue);
        stack->staticEval = bestValue = history.correctStaticEval(board->rule50_ply, unadjustedEval, correctionValue);
        ttEntry->update(fmrHash, Move::none(), 0, unadjustedEval, Eval(), board->rule50_ply, ttPv, TT_NOBOUND);
    }
    futilityValue = stack->staticEval.withScore(std::min(stack->staticEval.score + qsFutilityOffset, SCORE_TBWIN_IN_MAX_PLY - 1));

    assert(bestValue.score != SCORE_NONE);

    // Stand pat
    if (bestValue.score >= beta) {
        if (std::abs(bestValue.score) < SCORE_TBWIN_IN_MAX_PLY && std::abs(beta) < SCORE_TBWIN_IN_MAX_PLY)
            bestValue = bestValue.withScore((bestValue.score + beta) / 2);
        ttEntry->update(fmrHash, Move::none(), ttEntry->depth, unadjustedEval, Eval(), board->rule50_ply, ttPv, TT_NOBOUND);
        return bestValue;
    }
    if (alpha < bestValue.score)
        alpha = bestValue.score;

movesLoopQsearch:
    // Mate distance pruning
    alpha = std::max<int>(alpha, matedIn(stack->ply));
    beta = std::min<int>(beta, mateIn(stack->ply + 1));
    if (alpha >= beta)
        return Eval(alpha);

    // Moves loop
    MoveGen& movegen = movepickers[stack->ply][false] = MoveGen(board, &history, stack, ttMove, !board->checkers, 1);
    Move move;
    int moveCount = 0;
    bool playedQuiet = false;
    while ((move = movegen.nextMove())) {

        bool capture = board->isCapture(move);
        if (!capture && playedQuiet && bestValue.score > -SCORE_TBWIN_IN_MAX_PLY)
            continue;

        if (futilityValue.score != SCORE_NONE && bestValue.score > -SCORE_TBWIN_IN_MAX_PLY) { // Only prune when not in check
            if (futilityValue.score <= alpha && !SEE(board, move, 1)) {
                bestValue = bestValue.withScore(std::max(bestValue.score, futilityValue.score));
                continue;
            }

            if (!SEE(board, move, qsSeeMargin))
                break;

            if (!move.isPromotion() && moveCount > 2)
                continue;
        }

        if (!board->isLegal(move))
            continue;

        auto [newHash, newFmrHash] = board->hashAfter(move);
        TT.prefetch(newFmrHash);
        moveCount++;
        searchData.nodesSearched.fetch_add(1, std::memory_order_relaxed);

        Square origin = move.origin();
        Square target = move.target();
        stack->capture = capture;
        stack->move = move;
        stack->movedPiece = board->pieces[origin];
        stack->contHist = history.continuationHistory[board->stm][stack->movedPiece][target];
        stack->contCorrHist = &history.continuationCorrectionHistory[board->stm][stack->movedPiece][target][board->isSquareThreatened(origin)][board->isSquareThreatened(target)];

        playedQuiet |= move != ttMove && !capture;

        Board* boardCopy = doMove(board, newHash, move);
        Eval value = -qsearch<nodeType>(boardCopy, stack + 1, -beta, -alpha);
        undoMove();

        assert(value.score > -SCORE_INFINITE && value.score < SCORE_INFINITE);

        if (stopped.load(std::memory_order_relaxed) || exiting)
            return Eval(0);

        if (value.score > bestValue.score) {
            bestValue = value;

            if (value.score > alpha) {
                bestMove = move;
                alpha = value.score;

                if (pvNode)
                    updatePv(stack, move);

                if (bestValue.score >= beta)
                    break;
            }
        }
    }

    if (bestValue.score == -SCORE_INFINITE) {
        assert(board->checkers && moveCount == 0);
        bestValue = Eval(matedIn(stack->ply), 0, 0); // Checkmate
    }

    assert(bestValue.score > -SCORE_INFINITE && bestValue.score < SCORE_INFINITE);

    if (!pvNode && std::abs(bestValue.score) < SCORE_TBWIN_IN_MAX_PLY && std::abs(beta) < SCORE_TBWIN_IN_MAX_PLY && bestValue.score >= beta) {
        bestValue = bestValue.withScore((bestValue.score + beta) / 2);
    }

    // Insert into TT
    int flags = bestValue.score >= beta ? TT_LOWERBOUND : TT_UPPERBOUND;
    ttEntry->update(fmrHash, bestMove, 0, unadjustedEval, valueToTT(bestValue, stack->ply), board->rule50_ply, ttPv, flags);

    return bestValue;
}

template <NodeType nt>
Eval Worker::search(Board* board, SearchStack* stack, Depth depth, Score alpha, Score beta, bool cutNode) {
    constexpr bool rootNode = nt == ROOT_NODE;
    constexpr bool pvNode = nt == PV_NODE || nt == ROOT_NODE;
    constexpr NodeType nodeType = nt == ROOT_NODE ? PV_NODE : NON_PV_NODE;

    assert(-SCORE_INFINITE <= alpha && alpha < beta && beta <= SCORE_INFINITE);
    assert(!(pvNode && cutNode));
    assert(pvNode || alpha == beta - 1);

    // Set up PV length and selDepth
    if (pvNode)
        stack->pvLength = stack->ply;
    searchData.selDepth = std::max(stack->ply, searchData.selDepth);

    // Check for upcoming repetition
    if (!rootNode && alpha < 0 && hasUpcomingRepetition(board, stack->ply)) {
        alpha = drawEval(this).score;
        if (alpha >= beta)
            return Eval(alpha);
    }

    if (depth < 100) return qsearch<nodeType>(board, stack, alpha, beta);
    if (depth >= MAX_DEPTH) depth = MAX_DEPTH;

    if (!rootNode) {

        // Check for time / node limits on main thread
        if (mainThread && timeOver(searchParameters, searchData))
            threadPool->stopSearching();

        // Check for stop or max depth
        if (stopped.load(std::memory_order_relaxed) || exiting || stack->ply >= MAX_PLY - 1 || isDraw(board, stack->ply))
            return (stack->ply >= MAX_PLY - 1 && !board->checkers) ? evaluate(board, &nnue) : drawEval(this);

        // Mate distance pruning
        alpha = std::max<int>(alpha, matedIn(stack->ply));
        beta = std::min<int>(beta, mateIn(stack->ply + 1));
        if (alpha >= beta)
            return alpha;
    }

    // Initialize some stuff
    Move bestMove = Move::none();
    Move excludedMove = stack->excludedMove;
    Eval bestValue = Eval(-SCORE_INFINITE), maxValue = Eval(SCORE_INFINITE);
    Score oldAlpha = alpha;
    bool improving = false, excluded = static_cast<bool>(excludedMove);

    (stack + 1)->killer = Move::none();
    (stack + 1)->excludedMove = Move::none();
    stack->inCheck = board->checkerCount > 0;

    // TT Lookup
    Hash fmrHash = board->hashes.hash ^ Zobrist::FMR[board->rule50_ply / Zobrist::FMR_GRANULARITY];
    bool ttHit = false;
    TTEntry* ttEntry = nullptr;
    Move ttMove = Move::none();
    Eval ttValue = Eval(), ttEval = Eval();
    int ttDepth = 0;
    uint8_t ttFlag = TT_NOBOUND;
    stack->ttPv = excluded ? stack->ttPv : pvNode;

    if (!excluded) {
        ttEntry = TT.probe(fmrHash, &ttHit);
        if (ttHit) {
            ttMove = rootNode && rootMoves[0].value.score > -SCORE_INFINITE ? rootMoves[0].move : ttEntry->getMove();
            ttValue = valueFromTt(ttEntry->getValue(), stack->ply, board->rule50_ply);
            ttEval = ttEntry->getEval();
            ttDepth = ttEntry->getDepth();
            ttFlag = ttEntry->getFlag();
            stack->ttPv = stack->ttPv || ttEntry->getTtPv();
        }
    }

    // TT cutoff
    if (!pvNode && ttDepth >= depth - ttCutOffset + ttCutFailHighMargin * (ttValue.score >= beta) && ttValue.score != SCORE_NONE && ((ttFlag == TT_UPPERBOUND && ttValue.score <= alpha) || (ttFlag == TT_LOWERBOUND && ttValue.score >= beta) || (ttFlag == TT_EXACTBOUND)))
        return ttValue;

    // TB Probe
    if (!rootNode && !excluded && BB::popcount(board->byColor[Color::WHITE] | board->byColor[Color::BLACK]) <= std::min(int(TB_LARGEST), UCI::Options.syzygyProbeLimit.value)) {
        unsigned result = tb_probe_wdl(
            board->byColor[Color::WHITE],
            board->byColor[Color::BLACK],
            board->byPiece[Piece::KING],
            board->byPiece[Piece::QUEEN],
            board->byPiece[Piece::ROOK],
            board->byPiece[Piece::BISHOP],
            board->byPiece[Piece::KNIGHT],
            board->byPiece[Piece::PAWN],
            board->rule50_ply,
            board->castling,
            board->enpassantTarget ? lsb(board->enpassantTarget) : 0,
            board->stm == Color::WHITE
        );

        if (result != TB_RESULT_FAILED) {
            searchData.tbHits++;

            Eval tbValue;
            uint8_t tbBound;

            if (result == TB_LOSS) {
                tbValue = Eval(stack->ply - SCORE_TBWIN, 0, 0);
                tbBound = TT_UPPERBOUND;
            }
            else if (result == TB_WIN) {
                tbValue = Eval(SCORE_TBWIN - stack->ply, 255, 0);
                tbBound = TT_LOWERBOUND;
            }
            else {
                tbValue = Eval(0, 0, 255);
                tbBound = TT_EXACTBOUND;
            }

            if (tbBound == TT_EXACTBOUND || (tbBound == TT_LOWERBOUND ? tbValue.score >= beta : tbValue.score <= alpha)) {
                ttEntry->update(fmrHash, Move::none(), depth, SCORE_NONE, valueToTT(tbValue, stack->ply), board->rule50_ply, stack->ttPv, tbBound);
                return tbValue;
            }

            if (pvNode) {
                if (tbBound == TT_LOWERBOUND) {
                    bestValue = tbValue;
                    alpha = std::max(alpha, bestValue.score);
                }
                else {
                    maxValue = tbValue;
                }
            }
        }
    }

    // Static evaluation
    Eval eval = Eval(), unadjustedEval = Eval();
    Score probCutBeta = SCORE_NONE;

    int correctionValue = history.getCorrectionValue(board, stack);
    stack->correctionValue = correctionValue;
    if (board->checkers) {
        stack->staticEval = Eval();

        if (ttHit && ttValue.score != SCORE_NONE && ((ttFlag == TT_UPPERBOUND && ttValue.score <= alpha) || (ttFlag == TT_LOWERBOUND && ttValue.score >= beta) || (ttFlag == TT_EXACTBOUND)))
            eval = ttValue;
    }
    else if (excluded) {
        unadjustedEval = eval = stack->staticEval;
    }
    else if (ttHit) {
        unadjustedEval = ttEval.score != SCORE_NONE ? ttEval : evaluate(board, &nnue);
        eval = stack->staticEval = history.correctStaticEval(board->rule50_ply, unadjustedEval, correctionValue);

        if (ttValue.score != SCORE_NONE && ((ttFlag == TT_UPPERBOUND && ttValue.score < eval.score) || (ttFlag == TT_LOWERBOUND && ttValue.score > eval.score) || (ttFlag == TT_EXACTBOUND)))
            eval = ttValue;
    }
    else {
        unadjustedEval = evaluate(board, &nnue);
        eval = stack->staticEval = history.correctStaticEval(board->rule50_ply, unadjustedEval, correctionValue);

        ttEntry->update(fmrHash, Move::none(), 0, unadjustedEval, SCORE_NONE, board->rule50_ply, stack->ttPv, TT_NOBOUND);
    }

    // Improving
    if (!board->checkers) {
        if ((stack - 2)->staticEval.score != SCORE_NONE) {
            improving = stack->staticEval.score > (stack - 2)->staticEval.score;
        }
        else if ((stack - 4)->staticEval.score != SCORE_NONE) {
            improving = stack->staticEval.score > (stack - 4)->staticEval.score;
        }
    }

    // Adjust quiet history based on how much the previous move changed static eval
    if (!excluded && !board->checkers && (stack - 1)->movedPiece != Piece::NONE && !(stack - 1)->capture && !(stack - 1)->inCheck && stack->ply > 1) {
        int bonus = std::clamp(staticHistoryFactor * int(stack->staticEval.score + (stack - 1)->staticEval.score) / 10, staticHistoryMin, staticHistoryMax) + staticHistoryTempo;
        history.updateQuietHistory((stack - 1)->move, flip(board->stm), board - 1, bonus);
    }

    // IIR
    if (depth >= iirMinDepth + iirCheckDepth * !!board->checkers && (!ttHit || ttDepth + iirLowTtDepthOffset < depth))
        depth -= iirReduction;

    // Post-LMR depth adjustments
    if (!board->checkers && (stack - 1)->inLMR) {
        int additionalReduction = 0;
        if ((stack - 1)->reduction >= postlmrOppWorseningThreshold && stack->staticEval.score <= -(stack - 1)->staticEval.score)
            additionalReduction -= postlmrOppWorseningReduction;

        depth -= additionalReduction;
        (stack - 1)->reduction += additionalReduction;
    }

    // Reverse futility pruning
    if (!rootNode && depth <= rfpDepthLimit && std::abs(eval.score) < SCORE_TBWIN_IN_MAX_PLY) {
        int rfpMargin, rfpDepth;
        if (board->checkers) {
            rfpDepth = depth - rfpImprovingOffsetCheck * (improving && !board->opponentHasGoodCapture());
            rfpMargin = rfpBaseCheck + rfpFactorLinearCheck * rfpDepth / 100 + rfpFactorQuadraticCheck * rfpDepth * rfpDepth / 1000000;
        } else {
            rfpDepth = depth - rfpImprovingOffset * (improving && !board->opponentHasGoodCapture());
            rfpMargin = rfpBase + rfpFactorLinear * rfpDepth / 100 + rfpFactorQuadratic * rfpDepth * rfpDepth / 1000000;
        }
        if (eval.score - rfpMargin >= beta) {
            return eval.withScore(std::min<int>((eval.score + beta) / 2, SCORE_TBWIN_IN_MAX_PLY - 1));
        }
    }

    // Razoring
    if (!rootNode && !board->checkers && depth <= razoringDepth && eval.score + (razoringFactor * depth) / 100 < alpha && alpha < SCORE_TBWIN_IN_MAX_PLY) {
        Eval razorValue = qsearch<NON_PV_NODE>(board, stack, alpha, beta);
        if (razorValue.score <= alpha && std::abs(razorValue.score) < SCORE_TBWIN_IN_MAX_PLY)
            return razorValue;
    }

    // Null move pruning
    if (!pvNode
        && !board->checkers
        && eval.score >= beta
        && eval.score >= stack->staticEval.score
        && stack->staticEval.score + nmpEvalDepth * depth / 100 - nmpEvalBase >= beta
        && std::abs(beta) < SCORE_TBWIN_IN_MAX_PLY
        && !excluded
        && (stack - 1)->movedPiece != Piece::NONE
        && depth >= nmpMinDepth
        && stack->ply >= searchData.nmpPlies
        && board->hasNonPawns()
        ) {
        stack->capture = false;
        stack->move = Move::none();
        stack->movedPiece = Piece::NONE;
        stack->contHist = history.continuationHistory[board->stm][0][0];
        stack->contCorrHist = &history.continuationCorrectionHistory[board->stm][0][0][0][0];
        int R = nmpRedBase + 100 * depth / nmpDepthDiv + std::min(100 * (eval.score - beta) / nmpDivisor, nmpMin);

        Board* boardCopy = doNullMove(board);
        Eval nullValue = -search<NON_PV_NODE>(boardCopy, stack + 1, depth - R, -beta, -beta + 1, !cutNode);
        undoNullMove();

        if (stopped.load(std::memory_order_relaxed) || exiting)
            return Eval(0);

        if (nullValue.score >= beta) {
            if (nullValue.score >= SCORE_TBWIN_IN_MAX_PLY)
                nullValue = nullValue.withScore(beta);

            if (searchData.nmpPlies || depth < 1500)
                return nullValue;

            searchData.nmpPlies = stack->ply + (depth - R) * 2 / 300;
            Eval verificationValue = search<NON_PV_NODE>(board, stack, depth - R, beta - 1, beta, false);
            searchData.nmpPlies = 0;

            if (verificationValue.score >= beta)
                return nullValue;
        }
    }

    // ProbCut
    probCutBeta = std::min(beta + probCutBetaOffset, SCORE_TBWIN_IN_MAX_PLY - 1);
    if (!pvNode
        && !board->checkers
        && !excluded
        && depth > probCutDepth
        && std::abs(beta) < SCORE_TBWIN_IN_MAX_PLY - 1
        && !(ttDepth >= depth - probcutReduction && ttValue.score != SCORE_NONE && ttValue.score < probCutBeta)) {

        assert(probCutBeta > beta);
        assert(probCutBeta < SCORE_TBWIN_IN_MAX_PLY);

        Move probcutTtMove = ttMove && board->isPseudoLegal(ttMove) && SEE(board, ttMove, probCutBeta - stack->staticEval.score) ? ttMove : Move::none();
        MoveGen movegen(board, &history, stack, probcutTtMove, probCutBeta - stack->staticEval.score, depth / 100);
        Move move;
        while ((move = movegen.nextMove())) {
            if (move == excludedMove || !board->isLegal(move))
                continue;

            auto [newHash, newFmrHash] = board->hashAfter(move);
            TT.prefetch(newFmrHash);

            Square origin = move.origin();
            Square target = move.target();
            stack->capture = board->isCapture(move);
            stack->move = move;
            stack->movedPiece = board->pieces[origin];
            stack->contHist = history.continuationHistory[board->stm][stack->movedPiece][target];
            stack->contCorrHist = &history.continuationCorrectionHistory[board->stm][stack->movedPiece][target][board->isSquareThreatened(origin)][board->isSquareThreatened(target)];;

            Board* boardCopy = doMove(board, newHash, move);

            Eval value = -qsearch<NON_PV_NODE>(boardCopy, stack + 1, -probCutBeta, -probCutBeta + 1);

            if (value.score >= probCutBeta)
                value = -search<NON_PV_NODE>(boardCopy, stack + 1, depth - probcutReduction - 100, -probCutBeta, -probCutBeta + 1, !cutNode);

            undoMove();

            if (stopped.load(std::memory_order_relaxed) || exiting)
                return Eval(0);

            if (value.score >= probCutBeta) {
                value = value.withScore(std::min<int>(value.score, SCORE_TBWIN_IN_MAX_PLY - 1));
                ttEntry->update(fmrHash, move, depth - probcutReduction, unadjustedEval, valueToTT(value, stack->ply), board->rule50_ply, stack->ttPv, TT_LOWERBOUND);
                return value;
            }
        }

    }

    // IIR 2: Electric boolagoo
    if (!board->checkers && !ttHit && depth >= iir2MinDepth && pvNode)
        depth -= iir2Reduction;

    if (stopped.load(std::memory_order_relaxed) || exiting)
        return Eval(0);

    SearchedMoveList quietMoves, captureMoves;

    // Moves loop
    MoveGen& movegen = movepickers[stack->ply][excluded] = MoveGen(board, &history, stack, ttMove, depth / 100);
    Move move;
    int moveCount = 0;
    while ((move = movegen.nextMove())) {

        if (move == excludedMove)
            continue;
        if (rootNode && std::find(excludedRootMoves.begin(), excludedRootMoves.end(), move) != excludedRootMoves.end())
            continue;

        if (!board->isLegal(move))
            continue;

        uint64_t nodesBeforeMove = searchData.nodesSearched.load(std::memory_order_relaxed);

        bool capture = board->isCapture(move);
        bool importantCapture = stack->ttPv && capture && !cutNode;
        int moveHistory = history.getHistory(board, stack, move, capture);

        if (!rootNode
            && bestValue.score > -SCORE_TBWIN_IN_MAX_PLY
            && board->hasNonPawns()
            ) {

            Depth reduction = REDUCTIONS[int(capture) + int(importantCapture)][depth / 100][moveCount];
            reduction += earlyLmrImproving * !improving;
            reduction -= 100 * moveHistory / (capture ? earlyLmrHistoryFactorCapture : earlyLmrHistoryFactorQuiet);
            Depth lmrDepth = depth - reduction;

            // Movecount pruning (LMP)
            if (!pvNode && moveCount >= LMP_MARGIN[depth / 100][improving]) {
                movegen.skipQuietMoves();
            }

            // Futility pruning
            int fpValue = eval.score + fpBase + fpFactor * lmrDepth / 100 + pvNode * (fpPvNode + fpPvNodeBadCapture * !bestMove);
            if (!capture && (stack - 1)->movedPiece != Piece::NONE)
                fpValue += (stack - 1)->contHist[2 * 64 * board->pieces[move.origin()] + 2 * move.target() + board->stm] / 500;
            if (lmrDepth < fpDepth && fpValue <= alpha) {
                if (!capture)
                    movegen.skipQuietMoves();
                else if (!move.isPromotion()) {
                    Piece capturedPiece = move.isEnpassant() ? Piece::PAWN : board->pieces[move.target()];
                    if (fpValue + PIECE_VALUES[capturedPiece] <= alpha && movegen.stage >= MoveGenStage::STAGE_PLAY_BAD_CAPTURES)
                        break;
                }
            }

            lmrDepth = std::max<Depth>(0, lmrDepth);

            // Futility pruning for captures
            if (!pvNode && capture && !move.isPromotion()) {
                Piece capturedPiece = move.isEnpassant() ? Piece::PAWN : board->pieces[move.target()];
                if (lmrDepth < fpCaptDepth && eval.score + fpCaptBase + PIECE_VALUES[capturedPiece] + fpCaptFactor * lmrDepth / 100 <= alpha)
                    continue;
            }

            lmrDepth = std::min(std::min<Depth>(depth + 100, MAX_DEPTH), lmrDepth);

            // History pruning
            int hpFactor = capture ? historyPruningFactorCapture : historyPruningFactorQuiet;
            if (!pvNode && lmrDepth < historyPruningDepth && moveHistory < hpFactor * depth / 100)
                continue;

            // SEE Pruning
            int seeMargin = capture ? -22 * depth * depth / 10000 : -73 * lmrDepth / 100;
            if (!SEE(board, move, (2 + pvNode) * seeMargin / 2))
                continue;

        }

        // Extensions
        bool doExtensions = !rootNode && stack->ply < searchData.rootDepth * 2;
        int extension = 0;
        if (doExtensions
            && depth >= extensionMinDepth
            && move == ttMove
            && !excluded
            && (ttFlag & TT_LOWERBOUND)
            && std::abs(ttValue.score) < SCORE_TBWIN_IN_MAX_PLY
            && ttDepth >= depth - extensionTtDepthOffset
            ) {
            Score singularBeta = ttValue.score - (1 + (stack->ttPv && !pvNode)) * depth / 100;
            int singularDepth = (depth - 100) / 2;

            bool currTtPv = stack->ttPv;
            stack->excludedMove = move;
            Eval singularValue = search<NON_PV_NODE>(board, stack, singularDepth, singularBeta - 1, singularBeta, cutNode);
            stack->excludedMove = Move::none();
            stack->ttPv = currTtPv;

            if (stopped.load(std::memory_order_relaxed) || exiting)
                return Eval(0);

            if (singularValue.score < singularBeta) {
                // This move is singular and we should investigate it further
                extension = 1;
                if (!pvNode && singularValue.score + doubleExtensionMargin < singularBeta) {
                    extension = 2;
                    depth += doubleExtensionDepthIncreaseFactor * (depth < doubleExtensionDepthIncrease);
                    if (!board->isCapture(move) && singularValue.score + tripleExtensionMargin < singularBeta)
                        extension = 3;
                }
            }
            // Multicut: If we beat beta, that means there's likely more moves that beat beta and we can skip this node
            else if (singularBeta >= beta) {
                Score value = std::min<Score>(singularBeta, SCORE_TBWIN_IN_MAX_PLY - 1);
                ttEntry->update(fmrHash, ttMove, singularDepth, unadjustedEval, Eval(value), board->rule50_ply, stack->ttPv, TT_LOWERBOUND);

                // Adjust correction history
                if (!board->checkers && singularValue.score > stack->staticEval.score) {
                    int bonus = std::clamp((int(singularValue.score - stack->staticEval.score) * singularDepth / 100) * correctionHistoryFactorMulticut / 1024, -CORRECTION_HISTORY_LIMIT / 4, CORRECTION_HISTORY_LIMIT / 4);
                    history.updateCorrectionHistory(board, stack, bonus);
                }

                return value;
            }
            // We didn't prove singularity and an excluded search couldn't beat beta, but if the ttValue can we still reduce the depth
            else if (ttValue.score >= beta)
                extension = -3;
            // We didn't prove singularity and an excluded search couldn't beat beta, but we are expected to fail low, so reduce
            else if (cutNode)
                extension = -2;
        }

        auto [newHash, newFmrHash] = board->hashAfter(move);
        TT.prefetch(newFmrHash);

        // Some setup stuff
        Square origin = move.origin();
        Square target = move.target();
        stack->capture = capture;
        stack->move = move;
        stack->movedPiece = board->pieces[origin];
        stack->contHist = history.continuationHistory[board->stm][stack->movedPiece][target];
        stack->contCorrHist = &history.continuationCorrectionHistory[board->stm][stack->movedPiece][target][board->isSquareThreatened(origin)][board->isSquareThreatened(target)];;

        moveCount++;
        searchData.nodesSearched.fetch_add(1, std::memory_order_relaxed);

        Board* boardCopy = doMove(board, newHash, move);

        Eval value = Eval(0);
        int newDepth = depth - 100 + 100 * extension;
        int8_t moveSearchCount = 0;

        // Very basic LMR: Late moves are being searched with less depth
        // Check if the move can exceed alpha
        if (moveCount > lmrMcBase + lmrMcPv * rootNode - static_cast<bool>(ttMove) && depth >= lmrMinDepth) {
            Depth reduction = REDUCTIONS[int(capture) + int(importantCapture)][depth / 100][moveCount];
            reduction += lmrReductionOffset(importantCapture);
            reduction -= std::abs(correctionValue / lmrCorrectionDivisor(importantCapture));

            if (boardCopy->checkers)
                reduction -= lmrCheck(importantCapture);

            if (cutNode)
                reduction += lmrCutnode;

            if (stack->ttPv) {
                reduction -= lmrTtPv(importantCapture);
                reduction += lmrTtpvFaillow(importantCapture) * (ttHit && ttValue.score <= alpha);
            }

            if (capture) {
                reduction -= moveHistory * std::abs(moveHistory) / lmrCaptureHistoryDivisor(importantCapture);

                if (importantCapture) {
                    reduction += lmrImportantBadCaptureOffset * (movegen.stage == STAGE_PLAY_BAD_CAPTURES);
                    reduction = lmrImportantCaptureFactor * reduction / 100;
                }
            }
            else {
                reduction -= 100 * moveHistory / lmrQuietHistoryDivisor;
                reduction -= lmrQuietPvNodeOffset * pvNode;
                reduction -= lmrQuietImproving * improving;
            }

            Depth reducedDepth = std::clamp(newDepth - reduction, 100, newDepth + 100) + lmrPvNodeExtension * pvNode;
            stack->reduction = reduction;
            stack->inLMR = true;

            value = -search<NON_PV_NODE>(boardCopy, stack + 1, reducedDepth, -(alpha + 1), -alpha, true);
            moveSearchCount++;

            stack->inLMR = false;
            reducedDepth = std::clamp(newDepth - reduction, 100, newDepth + 100) + lmrPvNodeExtension * pvNode;
            stack->reduction = 0;

            bool doShallowerSearch = !rootNode && value.score < bestValue.score + newDepth / 100;
            bool doDeeperSearch = value.score > (bestValue.score + lmrDeeperBase + lmrDeeperFactor * newDepth / 100);
            newDepth += lmrDeeperWeight * doDeeperSearch - lmrShallowerWeight * doShallowerSearch;

            if (value.score > alpha && reducedDepth < newDepth && !(ttValue.score < alpha && ttDepth - lmrResearchSkipDepthOffset >= newDepth && (ttFlag & TT_UPPERBOUND))) {
                value = -search<NON_PV_NODE>(boardCopy, stack + 1, newDepth, -(alpha + 1), -alpha, !cutNode);
                moveSearchCount++;

                if (!capture) {
                    int bonus = std::min(lmrPassBonusBase + lmrPassBonusFactor * (value.score > alpha ? depth / 100 : reducedDepth / 100), lmrPassBonusMax);
                    history.updateContinuationHistory(stack, board->stm, stack->movedPiece, move, bonus);
                }
            }
        }
        else if (!pvNode || moveCount > 1) {
            if (move == ttMove && searchData.rootDepth > 8 && ttDepth > 1)
                newDepth = std::max(100, newDepth);

            value = -search<NON_PV_NODE>(boardCopy, stack + 1, newDepth, -(alpha + 1), -alpha, !cutNode);
            moveSearchCount++;
        }

        // PV moves will be researched at full depth if good enough
        if (pvNode && (moveCount == 1 || value.score > alpha)) {
            if (move == ttMove && searchData.rootDepth > 8 && ttDepth > 1)
                newDepth = std::max(100, newDepth);

            value = -search<PV_NODE>(boardCopy, stack + 1, newDepth, -beta, -alpha, false);
            moveSearchCount++;
        }

        undoMove();
        assert(value.score > -SCORE_INFINITE && value.score < SCORE_INFINITE);

        SearchedMoveList& list = capture ? captureMoves : quietMoves;
        if (list.size() < list.capacity())
            list.add({move, moveSearchCount});

        if (stopped.load(std::memory_order_relaxed) || exiting)
            return Eval(0);

        if (rootNode) {
            if (rootMoveNodes.count(move) == 0)
                rootMoveNodes[move] = searchData.nodesSearched.load(std::memory_order_relaxed) - nodesBeforeMove;
            else
                rootMoveNodes[move] = searchData.nodesSearched.load(std::memory_order_relaxed) - nodesBeforeMove + rootMoveNodes[move];

            RootMove* rootMove = &rootMoves[0];
            for (RootMove& rm : rootMoves) {
                if (rm.move == move) {
                    rootMove = &rm;
                    break;
                }
            }

            rootMove->meanScore = rootMove->meanScore == SCORE_NONE ? value.score : (rootMove->meanScore + value.score) / 2;

            if (moveCount == 1 || value.score > alpha) {
                rootMove->value = value;
                rootMove->depth = searchData.rootDepth;
                rootMove->selDepth = searchData.selDepth;

                rootMove->pv.resize(0);
                rootMove->pv.push_back(move);
                for (int i = 1; i < (stack + 1)->pvLength; i++)
                    rootMove->pv.push_back((stack + 1)->pv[i]);
            }
            else {
                rootMove->value = Eval(-SCORE_INFINITE);
            }
        }

        if (value.score > bestValue.score) {
            bestValue = value;

            if (value.score > alpha) {
                bestMove = move;
                alpha = value.score;

                if (pvNode)
                    updatePv(stack, move);

                if (bestValue.score >= beta) {

                    int historyUpdateDepth = depth / 100 + (eval.score <= alpha) + (value.score - historyDepthBetaOffset > beta);

                    if (!capture) {
                        // Update quiet killer
                        stack->killer = move;

                        // Update counter move
                        if (stack->ply > 0)
                            history.setCounterMove((stack - 1)->move, move);

                        history.updateQuietHistories(historyUpdateDepth, board, stack, move, moveSearchCount, quietMoves);
                    }
                    if (captureMoves.size())
                        history.updateCaptureHistory(historyUpdateDepth, board, move, moveSearchCount, captureMoves);
                    break;
                }

                if (depth > lowDepthPvDepthReductionMin && depth < lowDepthPvDepthReductionMax && beta < SCORE_TBWIN_IN_MAX_PLY && value.score > -SCORE_TBWIN_IN_MAX_PLY)
                    depth -= lowDepthPvDepthReductionWeight;
            }
        }

    }

    if (stopped.load(std::memory_order_relaxed) || exiting)
        return Eval(0);

    if (!pvNode && bestValue.score >= beta && std::abs(bestValue.score) < SCORE_TBWIN_IN_MAX_PLY && std::abs(beta) < SCORE_TBWIN_IN_MAX_PLY && std::abs(alpha) < SCORE_TBWIN_IN_MAX_PLY)
        bestValue = bestValue.withScore((bestValue.score * depth + 100 * beta) / (depth + 100));

    if (moveCount == 0) {
        if (board->checkers && excluded)
            return -SCORE_INFINITE;
        // Mate / Stalemate
        bestValue = board->checkers ? Eval(matedIn(stack->ply), 0, 0) : Eval(0, 0, 255);
    }

    if (pvNode)
        bestValue = bestValue.withScore(std::min(bestValue.score, maxValue.score));

    // Insert into TT
    bool failLow = alpha == oldAlpha;
    bool failHigh = bestValue.score >= beta;
    int flags = failHigh ? TT_LOWERBOUND : !failLow ? TT_EXACTBOUND : TT_UPPERBOUND;
    if (!excluded)
        ttEntry->update(fmrHash, bestMove, depth, unadjustedEval, valueToTT(bestValue, stack->ply), board->rule50_ply, stack->ttPv, flags);

    // Adjust correction history
    if (!board->checkers && (!bestMove || !board->isCapture(bestMove)) && (!failHigh || bestValue.score > stack->staticEval.score) && (!failLow || bestValue.score <= stack->staticEval.score)) {
        int bonus = std::clamp((int(bestValue.score - stack->staticEval.score) * depth / 100) * correctionHistoryFactor / 1024, -CORRECTION_HISTORY_LIMIT / 4, CORRECTION_HISTORY_LIMIT / 4);
        history.updateCorrectionHistory(board, stack, bonus);
    }

    assert(bestValue.score > -SCORE_INFINITE && bestValue.score < SCORE_INFINITE);

    return bestValue;
}

Move tbProbeMoveRoot(unsigned result) {
    Square origin = TB_GET_FROM(result);
    Square target = TB_GET_TO(result);
    int promotion = TB_GET_PROMOTES(result);

    if (promotion) {
        Piece promotionPiece = Piece::QUEEN;
        switch (promotion) {
            case TB_PROMOTES_KNIGHT:
                promotionPiece = Piece::KNIGHT;
                break;
            case TB_PROMOTES_BISHOP:
                promotionPiece = Piece::BISHOP;
                break;
            case TB_PROMOTES_ROOK:
                promotionPiece = Piece::ROOK;
                break;
        }
        return Move::makePromotion(origin, target, promotionPiece);
    }

    if (TB_GET_EP(result))
        return Move::makeEnpassant(origin, target);

    return Move::makeNormal(origin, target);
}

void Worker::tsearch() {
    if (TUNE_ENABLED)
        initReductions();

    nnue.reset(&rootBoard);

    Move bestTbMove = Move::none();
    if (mainThread && BB::popcount(rootBoard.byColor[Color::WHITE] | rootBoard.byColor[Color::BLACK]) <= std::min(int(TB_LARGEST), UCI::Options.syzygyProbeLimit.value)) {
        unsigned result = tb_probe_root(
            rootBoard.byColor[Color::WHITE],
            rootBoard.byColor[Color::BLACK],
            rootBoard.byPiece[Piece::KING],
            rootBoard.byPiece[Piece::QUEEN],
            rootBoard.byPiece[Piece::ROOK],
            rootBoard.byPiece[Piece::BISHOP],
            rootBoard.byPiece[Piece::KNIGHT],
            rootBoard.byPiece[Piece::PAWN],
            rootBoard.rule50_ply,
            rootBoard.castling,
            rootBoard.enpassantTarget ? lsb(rootBoard.enpassantTarget) : 0,
            rootBoard.stm == Color::WHITE,
            nullptr
        );

        if (result != TB_RESULT_FAILED)
            bestTbMove = tbProbeMoveRoot(result);
    }

    searchData.nodesSearched.store(0, std::memory_order_relaxed);
    searchData.tbHits = 0;
    if (mainThread)
        initTimeManagement(rootBoard, searchParameters, searchData);

    iterativeDeepening();
    sortRootMoves();

    if (mainThread) {
        threadPool->stopSearching();
        threadPool->waitForHelpersFinished();

        Worker* bestThread = chooseBestThread();

        if (bestThread != this || UCI::Options.minimal.value) {
            printUCI(bestThread);
        }

        if (!UCI::Options.ponder.value || bestThread->rootMoves[0].pv.size() < 2) {
            Move bestMove = bestTbMove && std::abs(bestThread->rootMoves[0].value.score) < SCORE_MATE_IN_MAX_PLY ? bestTbMove : bestThread->rootMoves[0].move;
            std::cout << "bestmove " << bestMove.toString(UCI::Options.chess960.value) << std::endl;
        }
        else {
            std::cout << "bestmove " << bestThread->rootMoves[0].move.toString(UCI::Options.chess960.value) << " ponder " << bestThread->rootMoves[0].pv[1].toString(UCI::Options.chess960.value) << std::endl;
        }
    }
}

void Worker::iterativeDeepening() {
    for (auto& a : history.quietHistory)
        for (auto& b : a)
            for (auto& c : b)
                for (auto& d : c)
                    for (auto& e : d)
                        e = 3 * e / 4;

    int multiPvCount = 0;
    {
        MoveList moves;
        generateMoves(&rootBoard, moves);
        for (auto& move : moves) {
            if (rootBoard.isLegal(move)) {
                multiPvCount++;

                RootMove rootMove = {};
                rootMove.move = move;
                rootMoves.push_back(rootMove);
            }
        }
    }
    multiPvCount = std::min(multiPvCount, UCI::Options.multiPV.value);

    int maxDepth = searchParameters.depth == 0 ? MAX_PLY - 1 : std::min<Depth>(MAX_PLY - 1, searchParameters.depth);

    Eval baseValue = Eval();
    Eval previousValue = Eval();
    Move previousMove = Move::none();

    int bestMoveStability = 0;

    constexpr int STACK_OVERHEAD = 6;
    std::vector<SearchStack> stackList;
    stackList.reserve(MAX_PLY + STACK_OVERHEAD + 2);
    SearchStack* stack = &stackList[STACK_OVERHEAD];

    std::vector<Board> boardList;
    boardList.reserve(MAX_PLY + 2);
    boardList[0] = rootBoard;
    Board* board = &boardList[0];

    movepickers.reserve(MAX_PLY + 2);

    rootMoveNodes.clear();

    for (Depth depth = 1; depth <= maxDepth; depth++) {
        excludedRootMoves.clear();
        for (int rootMoveIdx = 0; rootMoveIdx < multiPvCount; rootMoveIdx++) {

            for (size_t i = 0; i < stackList.capacity(); i++) {
                stackList[i].pvLength = 0;
                stackList[i].ply = int(i) - STACK_OVERHEAD;
                stackList[i].staticEval = Eval();
                stackList[i].excludedMove = Move::none();
                stackList[i].killer = Move::none();
                stackList[i].movedPiece = Piece::NONE;
                stackList[i].move = Move::none();
                stackList[i].capture = false;
                stackList[i].inCheck = false;
                stackList[i].correctionValue = 0;
                stackList[i].reduction = 0;
                stackList[i].inLMR = false;
                stackList[i].ttPv = false;
            }

            searchData.rootDepth = depth;
            searchData.selDepth = 0;

            // Aspiration Windows
            Score delta = SCORE_INFINITE;
            Score alpha = -SCORE_INFINITE;
            Score beta = SCORE_INFINITE;
            Eval value;

            if (depth >= aspirationWindowMinDepth) {
                // Set up interval for the start of this aspiration window
                if (rootMoves[0].meanScore == SCORE_NONE)
                    delta = aspirationWindowDelta;
                else
                    delta = std::min<int>(aspirationWindowDeltaBase + rootMoves[0].meanScore * rootMoves[0].meanScore / aspirationWindowDeltaDivisor, SCORE_INFINITE);
                assert(delta > 0);

                alpha = std::max<int>(previousValue.score - delta, -SCORE_INFINITE);
                beta = std::min<int>(previousValue.score + delta, SCORE_INFINITE);
            }

            int failHighs = 0;
            while (true) {
                int searchDepth = std::max(1, depth - failHighs);

                value = search<ROOT_NODE>(board, stack, searchDepth * 100, alpha, beta, false);

                sortRootMoves();

                // Stop if we need to
                if (stopped.load(std::memory_order_relaxed) || exiting)
                    break;

                // Our window was too high, lower alpha for next iteration
                if (value.score <= alpha) {
                    beta = (alpha + beta) / 2;
                    alpha = std::max<int>(value.score - delta, -SCORE_INFINITE);
                    failHighs = 0;
                }
                // Our window was too low, increase beta for next iteration
                else if (value.score >= beta) {
                    beta = std::min<int>(value.score + delta, SCORE_INFINITE);
                    failHighs = std::min(failHighs + 1, aspirationWindowMaxFailHighs);
                }
                // Our window was good, increase depth for next iteration
                else
                    break;

                if (value.score >= SCORE_TBWIN_IN_MAX_PLY) {
                    beta = SCORE_INFINITE;
                    failHighs = 0;
                }

                delta = std::clamp<int>(delta * aspirationWindowDeltaFactor, -SCORE_INFINITE, SCORE_INFINITE);
                assert(delta > 0);
            }

            if (stopped.load(std::memory_order_relaxed) || exiting)
                return;

            excludedRootMoves.push_back(stack->pv[0]);
        }

        if (mainThread) {
            if (!UCI::Options.minimal.value)
                printUCI(this, multiPvCount);

            // Adjust time management
            double tmAdjustment = tmInitialAdjustment;

            // Based on best move stability
            if (rootMoves[0].move == previousMove)
                bestMoveStability = std::min(bestMoveStability + 1, tmBestMoveStabilityMax);
            else
                bestMoveStability = 0;
            tmAdjustment *= tmBestMoveStabilityBase - bestMoveStability * tmBestMoveStabilityFactor;

            // Based on score difference to last iteration
            tmAdjustment *= tmEvalDiffBase + std::clamp(previousValue.score - rootMoves[0].value.score, tmEvalDiffMin, tmEvalDiffMax) * tmEvalDiffFactor;

            // Based on fraction of nodes that went into the best move
            tmAdjustment *= tmNodesBase - tmNodesFactor * ((double)rootMoveNodes[rootMoves[0].move] / (double)searchData.nodesSearched.load(std::memory_order_relaxed));

            // Based on search score complexity
            if (baseValue.score != SCORE_NONE) {
                double complexity = 0.6 * std::abs(baseValue.score - rootMoves[0].value.score) * std::log(depth);
                tmAdjustment *= std::max(0.77 + std::clamp(complexity, 0.0, 200.0) / 386.0, 1.0);
            }

            if (searchData.doSoftTM && timeOverDepthCleared(searchParameters, searchData, tmAdjustment)) {
                threadPool->stopSearching();
                return;
            }
        }

        previousMove = rootMoves[0].move;
        previousValue = rootMoves[0].value;
        baseValue = depth == 1 ? previousValue : baseValue;
    }
}

void Worker::printUCI(Worker* thread, int multiPvCount) {
    int64_t ms = getTime() - searchData.startTime;
    int64_t nodes = threadPool->nodesSearched();
    int64_t nps = ms == 0 ? 0 : nodes / ((double)ms / 1000);

    for (int rootMoveIdx = 0; rootMoveIdx < multiPvCount; rootMoveIdx++) {
        RootMove rootMove = thread->rootMoves[rootMoveIdx];
        std::cout << "info depth " << rootMove.depth << " seldepth " << rootMove.selDepth << " score " << formatEval(rootMove.value.score) << " wdl " << (100 * rootMove.value.win / 255) << " " << (100 * rootMove.value.draw / 255) << " " << (100 * (255 - rootMove.value.win - rootMove.value.draw) / 255) << " multipv " << (rootMoveIdx + 1) << " nodes " << nodes << " tbhits " << threadPool->tbhits() << " time " << ms << " nps " << nps << " hashfull " << TT.hashfull() << " pv ";

        // Send PV
        for (Move move : rootMove.pv)
            std::cout << move.toString(UCI::Options.chess960.value) << " ";
        std::cout << std::endl;
    }
}

void Worker::sortRootMoves() {
    std::stable_sort(rootMoves.begin(), rootMoves.end(), [](RootMove rm1, RootMove rm2) {
        if (rm1.depth > rm2.depth) return true;
        if (rm1.depth < rm2.depth) return false;
        return rm1.value.score > rm2.value.score;
        });
}

Worker* Worker::chooseBestThread() {
    Worker* bestThread = this;

    if (threadPool->workers.size() > 1 && UCI::Options.multiPV.value == 1) {
        std::map<Move, int64_t> votes;
        Eval minValue = Eval(SCORE_INFINITE);

        for (auto& worker : threadPool->workers) {
            if (worker->rootMoves[0].value.score == -SCORE_INFINITE)
                break;
            minValue = minValue.score < worker->rootMoves[0].value.score ? minValue : worker->rootMoves[0].value;
        }

        for (auto& worker : threadPool->workers) {
            auto& rm = worker->rootMoves[0];
            if (rm.value.score == -SCORE_INFINITE)
                break;
            votes[rm.move] += (rm.value.score - minValue.score + 11) * rm.depth;
        }

        for (auto& worker : threadPool->workers) {
            Worker* thread = worker.get();
            RootMove& rm = thread->rootMoves[0];
            if (rm.value.score == -SCORE_INFINITE)
                break;

            Eval thValue = rm.value;
            Eval bestValue = bestThread->rootMoves[0].value;
            Move thMove = rm.move;
            Move bestMove = bestThread->rootMoves[0].move;

            // In case of checkmate, take the shorter mate / longer getting mated
            if (std::abs(bestValue.score) >= SCORE_TBWIN_IN_MAX_PLY) {
                if (thValue.score > bestValue.score) {
                    bestThread = thread;
                }
            }
            // We have found a mate, take it without voting
            else if (thValue.score >= SCORE_TBWIN_IN_MAX_PLY) {
                bestThread = thread;
            }
            // No mate found by any thread so far, take the thread with more votes
            else if (votes[thMove] > votes[bestMove]) {
                bestThread = thread;
            }
            // In case of same move, choose the thread with the highest score
            else if (thMove == bestMove && std::abs(thValue.score) > std::abs(bestValue.score) && rm.pv.size() > 2) {
                bestThread = thread;
            }
        }
    }

    return bestThread;
}

void Worker::tdatagen() {
    nnue.reset(&rootBoard);

    searchData.nodesSearched.store(0, std::memory_order_relaxed);
    searchData.tbHits = 0;
    initTimeManagement(rootBoard, searchParameters, searchData);
    {
        MoveList moves;
        generateMoves(&rootBoard, moves);
        for (auto& move : moves) {
            if (rootBoard.isLegal(move)) {
                RootMove rootMove = {};
                rootMove.move = move;
                rootMoves.push_back(rootMove);
            }
        }
    }

    Eval previousValue = Eval();

    constexpr int STACK_OVERHEAD = 6;
    SearchStack stackList[MAX_PLY + STACK_OVERHEAD + 2];
    SearchStack* stack = &stackList[STACK_OVERHEAD];

    Board boardList[MAX_PLY + 2];
    boardList[0] = rootBoard;
    Board* board = &boardList[0];

    rootMoveNodes.clear();

    int maxDepth = searchParameters.depth == 0 ? MAX_PLY - 1 : std::min<Depth>(MAX_PLY - 1, searchParameters.depth);

    for (Depth depth = 1; depth <= maxDepth; depth++) {
        for (int i = 0; i < MAX_PLY + STACK_OVERHEAD + 2; i++) {
            stackList[i].pvLength = 0;
            stackList[i].ply = int(i) - STACK_OVERHEAD;
            stackList[i].staticEval = Eval();
            stackList[i].excludedMove = Move::none();
            stackList[i].killer = Move::none();
            stackList[i].movedPiece = Piece::NONE;
            stackList[i].move = Move::none();
            stackList[i].capture = false;
            stackList[i].inCheck = false;
            stackList[i].correctionValue = 0;
            stackList[i].reduction = 0;
            stackList[i].inLMR = false;
            stackList[i].ttPv = false;
        }

        searchData.rootDepth = depth;
        searchData.selDepth = 0;

        // Aspiration Windows
        Score delta = SCORE_INFINITE;
        Score alpha = -SCORE_INFINITE;
        Score beta = SCORE_INFINITE;
        Eval value;

        if (depth >= aspirationWindowMinDepth) {
            // Set up interval for the start of this aspiration window
            delta = aspirationWindowDelta;
            alpha = std::max<int>(previousValue.score - delta, -SCORE_INFINITE);
            beta = std::min<int>(previousValue.score + delta, SCORE_INFINITE);
        }

        int failHighs = 0;
        while (true) {
            int searchDepth = std::max(1, depth - failHighs);
            value = search<ROOT_NODE>(board, stack, searchDepth * 100, alpha, beta, false);

            sortRootMoves();

            // Stop if we need to
            if (stopped.load(std::memory_order_relaxed) || exiting || searchData.nodesSearched.load(std::memory_order_relaxed) >= searchParameters.nodes)
                break;

            // Our window was too high, lower alpha for next iteration
            if (value.score <= alpha) {
                beta = (alpha + beta) / 2;
                alpha = std::max(value.score - delta, -SCORE_INFINITE);
                failHighs = 0;
            }
            // Our window was too low, increase beta for next iteration
            else if (value.score >= beta) {
                beta = std::min(value.score + delta, (int)SCORE_INFINITE);
                failHighs = std::min(failHighs + 1, aspirationWindowMaxFailHighs);
            }
            // Our window was good, increase depth for next iteration
            else
                break;

            if (value.score >= SCORE_TBWIN_IN_MAX_PLY) {
                beta = SCORE_INFINITE;
                failHighs = 0;
            }

            delta *= aspirationWindowDeltaFactor;
        }

        if (stopped.load(std::memory_order_relaxed) || exiting || searchData.nodesSearched.load(std::memory_order_relaxed) >= searchParameters.nodes)
            break;

        previousValue = rootMoves[0].value;
    }

    sortRootMoves();
    printUCI(this);

    std::cout << "bestmove " << rootMoves[0].move.toString(UCI::Options.chess960.value) << std::endl;
}
