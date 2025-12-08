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
TUNE_INT(aspirationWindowDeltaDivisor, 12671, 7500, 17500);
TUNE_INT_DISABLED(aspirationWindowMaxFailHighs, 3, 1, 10);
TUNE_FLOAT_DISABLED(aspirationWindowDeltaFactor, 1.5804938062670641f, 1.0f, 3.0f);

// Reduction / Margin tables
TUNE_FLOAT(lmrReductionNoisyBase, -0.13976368934259956f, -2.0f, -0.1f);
TUNE_FLOAT(lmrReductionNoisyFactor, 3.075360070013846f, 2.0f, 4.0f);
TUNE_FLOAT(lmrReductionImportantNoisyBase, -0.23652154081998214f, -2.0f, -0.1f);
TUNE_FLOAT(lmrReductionImportantNoisyFactor, 3.1132686274533334f, 2.0f, 4.0f);
TUNE_FLOAT(lmrReductionQuietBase, 1.1563468004387576f, 0.50f, 1.5f);
TUNE_FLOAT(lmrReductionQuietFactor, 2.8567475668420714f, 2.0f, 4.0f);

TUNE_FLOAT(lmpMarginWorseningBase, 1.7582461098733024f, -1.0f, 2.5f);
TUNE_FLOAT(lmpMarginWorseningFactor, 0.41135134367388143f, 0.1f, 1.5f);
TUNE_FLOAT(lmpMarginWorseningPower, 1.5862076441507922f, 1.0f, 3.0f);
TUNE_FLOAT(lmpMarginImprovingBase, 2.6163580912369113f, 2.0f, 5.0f);
TUNE_FLOAT(lmpMarginImprovingFactor, 0.8211099981265848f, 0.5f, 2.0f);
TUNE_FLOAT(lmpMarginImprovingPower, 2.1008509291144177f, 1.0f, 3.0f);

// Search values
TUNE_INT(qsFutilityOffset, 79, 1, 125);
TUNE_INT(qsSeeMargin, -84, -200, 50);

// Pre-search pruning
TUNE_INT(ttCutOffset, 40, -100, 200);
TUNE_INT(ttCutFailHighMargin, 105, 0, 200);

TUNE_INT(iirMinDepth, 287, 100, 1000);
TUNE_INT(iirCheckDepth, 534, 0, 1000);
TUNE_INT(iirLowTtDepthOffset, 412, 0, 800);
TUNE_INT(iirReduction, 85, 0, 200);

TUNE_INT(staticHistoryFactor, -92, -500, -1);
TUNE_INT(staticHistoryMin, -174, -1000, -1);
TUNE_INT(staticHistoryMax, 318, 1, 1000);
TUNE_INT(staticHistoryTempo, 29, 1, 200);

TUNE_INT(rfpDepth, 1476, 200, 2000);
TUNE_INT(rfpBase, 19, -100, 100);
TUNE_INT(rfpFactorLinear, 33, 1, 250);
TUNE_INT(rfpFactorQuadratic, 739, 1, 1800);
TUNE_INT(rfpImprovingOffset, 101, 1, 200);
TUNE_INT(rfpBaseCheck, -3, -100, 100);
TUNE_INT(rfpFactorLinearCheck, 38, 1, 250);
TUNE_INT(rfpFactorQuadraticCheck, 611, 1, 1800);
TUNE_INT(rfpImprovingOffsetCheck, 110, 1, 200);

TUNE_INT(razoringDepth, 545, 200, 2000);
TUNE_INT(razoringFactor, 266, 1, 1000);

TUNE_INT(nmpMinDepth, 281, 0, 600);
TUNE_INT(nmpRedBase, 357, 100, 800);
TUNE_INT(nmpDepthDiv, 248, 100, 600);
TUNE_INT(nmpMin, 373, 100, 800);
TUNE_INT(nmpDivisor, 257, 10, 600);
TUNE_INT(nmpEvalDepth, 7, 1, 100);
TUNE_INT(nmpEvalBase, 161, 50, 300);

TUNE_INT(probcutReduction, 420, 0, 600);
TUNE_INT(probCutBetaOffset, 216, 1, 500);
TUNE_INT(probCutDepth, 545, 100, 1000);

TUNE_INT(iir2Reduction, 103, 0, 200);
TUNE_INT(iir2MinDepth, 265, 100, 1000);

// In-search pruning
TUNE_INT(earlyLmrImproving, 119, 1, 500);

TUNE_INT(earlyLmrHistoryFactorQuiet, 15603, 10000, 20000);
TUNE_INT(earlyLmrHistoryFactorCapture, 14501, 10000, 20000);

TUNE_INT(earlyLmrQhWeight, 98, 0, 200);
TUNE_INT(earlyLmrPhWeight, 101, 0, 200);
TUNE_INT(earlyLmrCh1Weight, 430, 0, 800);
TUNE_INT(earlyLmrCh2Weight, 190, 0, 400);
TUNE_INT(earlyLmrCh3Weight, 0, 0, 50);
TUNE_INT(earlyLmrCh4Weight, 195, 0, 400);
TUNE_INT(earlyLmrCh6Weight, 106, 0, 200);

TUNE_INT(fpDepth, 1125, 100, 2000);
TUNE_INT(fpBase, 299, 1, 1000);
TUNE_INT(fpFactor, 83, 1, 500);
TUNE_INT(fpPvNode, 33, 1, 250);
TUNE_INT(fpPvNodeNoBestMove, 120, 1, 500);

TUNE_INT(fpHistoryWeight, 567, 0, 1000);
TUNE_INT(fpQhWeight, 1, 0, 50);
TUNE_INT(fpPhWeight, 0, 0, 50);
TUNE_INT(fpCh1Weight, 101, 0, 200);
TUNE_INT(fpCh2Weight, 1, 0, 50);
TUNE_INT(fpCh3Weight, 1, 0, 50);
TUNE_INT(fpCh4Weight, 0, 0, 50);
TUNE_INT(fpCh6Weight, 1, 0, 50);

TUNE_INT(fpCaptDepth, 705, 100, 2000);
TUNE_INT(fpCaptBase, 442, 150, 750);
TUNE_INT(fpCaptFactor, 439, 100, 600);

TUNE_INT(historyPruningDepth, 494, 100, 1000);
TUNE_INT(historyPruningFactorCapture, -2543, -8192, -128);
TUNE_INT(historyPruningFactorQuiet, -6802, -8192, -128);

TUNE_INT(hpQhWeight, 92, 0, 200);
TUNE_INT(hpPhWeight, 99, 0, 200);
TUNE_INT(hpCh1Weight, 371, 0, 800);
TUNE_INT(hpCh2Weight, 185, 0, 400);
TUNE_INT(hpCh3Weight, 2, 0, 50);
TUNE_INT(hpCh4Weight, 219, 0, 400);
TUNE_INT(hpCh6Weight, 104, 0, 200);

TUNE_INT(seeFactorCapture, 19, 0, 50);
TUNE_INT(seeFactorQuiet, 77, 0, 150);

TUNE_INT(extensionMinDepth, 670, 0, 1200);
TUNE_INT(extensionTtDepthOffset, 466, 0, 600);
TUNE_INT(doubleExtensionDepthIncreaseFactor, 98, 0, 200);
TUNE_INT_DISABLED(doubleExtensionMargin, 6, 1, 30);
TUNE_INT(doubleExtensionDepthIncrease, 1046, 200, 2000);
TUNE_INT_DISABLED(tripleExtensionMargin, 41, 25, 100);

TUNE_INT(singleExtension, 117, 0, 200);
TUNE_INT(doubleExtension, 205, 100, 300);
TUNE_INT(tripleExtension, 300, 200, 400);
TUNE_INT(ttValueNegExt, -315, -400, -200);
TUNE_INT(cutNodeNegExt, -194, -300, -100);

TUNE_INT_DISABLED(lmrMcBase, 2, 1, 10);
TUNE_INT_DISABLED(lmrMcPv, 2, 1, 10);
TUNE_INT(lmrMinDepth, 312, 100, 600);

TUNE_INT(lmrReductionOffsetQuietOrNormalCapture, 160, 0, 500);
TUNE_INT(lmrReductionOffsetImportantCapture, 5, 0, 500);
TUNE_INT(lmrCheckQuietOrNormalCapture, 136, 0, 500);
TUNE_INT(lmrCheckImportantCapture, 53, 0, 500);
TUNE_INT(lmrTtPvQuietOrNormalCapture, 186, 0, 500);
TUNE_INT(lmrTtPvImportantCapture, 182, 0, 500);
TUNE_INT(lmrCutnode, 280, 0, 500);
TUNE_INT(lmrTtpvFaillowQuietOrNormalCapture, 67, 0, 500);
TUNE_INT(lmrTtpvFaillowImportantCapture, 119, 0, 500);
TUNE_INT(lmrCorrectionDivisorQuietOrNormalCapture, 139623, 10000, 200000);
TUNE_INT(lmrCorrectionDivisorImportantCapture, 141865, 10000, 200000);
TUNE_INT(lmrQuietHistoryDivisor, 29832, 10000, 30000);
TUNE_INT(lmrHistoryFactorCapture, 3076542, 2500000, 4000000);
TUNE_INT(lmrHistoryFactorImportantCapture, 2980405, 2500000, 4000000);
TUNE_INT(lmrImportantBadCaptureOffset, 115, 0, 500);
TUNE_INT(lmrImportantCaptureFactor, 36, 0, 250);
TUNE_INT(lmrQuietPvNodeOffset, 8, 0, 250);
TUNE_INT(lmrQuietImproving, 60, 0, 250);

inline int lmrReductionOffset(bool importantCapture) { return importantCapture ? lmrReductionOffsetImportantCapture : lmrReductionOffsetQuietOrNormalCapture; };
inline int lmrCheck(bool importantCapture) { return importantCapture ? lmrCheckImportantCapture : lmrCheckQuietOrNormalCapture; };
inline int lmrTtPv(bool importantCapture) { return importantCapture ? lmrTtPvImportantCapture : lmrTtPvQuietOrNormalCapture; };
inline int lmrTtpvFaillow(bool importantCapture) { return importantCapture ? lmrTtpvFaillowQuietOrNormalCapture : lmrTtpvFaillowImportantCapture; };
inline int lmrCaptureHistoryDivisor(bool importantCapture) { return importantCapture ? lmrHistoryFactorImportantCapture : lmrHistoryFactorCapture; };
inline int lmrCorrectionDivisor(bool importantCapture) { return importantCapture ? lmrCorrectionDivisorImportantCapture : lmrCorrectionDivisorQuietOrNormalCapture; };

TUNE_INT(lmrQhWeight, 103, 0, 200);
TUNE_INT(lmrPhWeight, 107, 0, 200);
TUNE_INT(lmrCh1Weight, 411, 0, 800);
TUNE_INT(lmrCh2Weight, 212, 0, 400);
TUNE_INT(lmrCh3Weight, 0, 0, 50);
TUNE_INT(lmrCh4Weight, 183, 0, 400);
TUNE_INT(lmrCh6Weight, 97, 0, 200);

TUNE_INT(postlmrOppWorseningThreshold, 256, 150, 450);
TUNE_INT(postlmrOppWorseningReduction, 140, 0, 200);

TUNE_INT(lmrPvNodeExtension, 109, 0, 200);
TUNE_INT_DISABLED(lmrDeeperBase, 40, 1, 100);
TUNE_INT_DISABLED(lmrDeeperFactor, 2, 0, 10);
TUNE_INT(lmrDeeperWeight, 116, 0, 200);
TUNE_INT(lmrShallowerWeight, 110, 0, 200);
TUNE_INT(lmrResearchSkipDepthOffset, 386, 0, 800);

TUNE_INT(lmrPassBonusBase, -248, -500, 500);
TUNE_INT(lmrPassBonusFactor, 177, 1, 500);
TUNE_INT(lmrPassBonusMax, 967, 32, 4096);

TUNE_INT(historyDepthBetaOffset, 215, 1, 500);

TUNE_INT(lowDepthPvDepthReductionMin, 442, 0, 800);
TUNE_INT(lowDepthPvDepthReductionMax, 1200, 0, 2000);
TUNE_INT(lowDepthPvDepthReductionWeight, 109, 0, 200);

TUNE_INT(correctionHistoryFactor, 140, 32, 512);
TUNE_INT(correctionHistoryFactorMulticut, 177, 32, 512);

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
        boardCopy.doMove(move, boardCopy.hashAfter(move), &UCI::nnue);
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
        boardCopy.doMove(move, boardCopy.hashAfter(move), &UCI::nnue);
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

int valueToTT(int value, int ply) {
    if (value == EVAL_NONE) return EVAL_NONE;
    if (value >= EVAL_TBWIN_IN_MAX_PLY) value += ply;
    else if (value <= -EVAL_TBWIN_IN_MAX_PLY) value -= ply;
    return value;
}

int valueFromTt(int value, int ply, int rule50) {
    if (value == EVAL_NONE) return EVAL_NONE;

    if (value >= EVAL_TBWIN_IN_MAX_PLY) {
        // Downgrade potentially false mate score
        if (value >= EVAL_MATE_IN_MAX_PLY && EVAL_MATE - value > 100 - rule50)
            return EVAL_TBWIN_IN_MAX_PLY - 1;

        // Downgrade potentially false TB score
        if (EVAL_TBWIN - value > 100 - rule50)
            return EVAL_TBWIN_IN_MAX_PLY - 1;

        return value - ply;
    }
    else if (value <= -EVAL_TBWIN_IN_MAX_PLY) {
        // Downgrade potentially false mate score
        if (value <= -EVAL_MATE_IN_MAX_PLY && EVAL_MATE + value > 100 - rule50)
            return -EVAL_TBWIN_IN_MAX_PLY + 1;

        // Downgrade potentially false TB score
        if (EVAL_TBWIN + value > 100 - rule50)
            return -EVAL_TBWIN_IN_MAX_PLY + 1;

        return value + ply;
    }
    return value;
}

Eval drawEval(Worker* thread) {
    return 4 - (thread->searchData.nodesSearched & 3);  // Small overhead to avoid 3-fold blindness
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
Eval Worker::qsearch(Board* board, SearchStack* stack, Eval alpha, Eval beta) {
    constexpr bool pvNode = nodeType == PV_NODE;

    if (pvNode)
        stack->pvLength = stack->ply;
    searchData.selDepth = std::max(stack->ply, searchData.selDepth);

    assert(alpha >= -EVAL_INFINITE && alpha < beta && beta <= EVAL_INFINITE);

    if (mainThread && timeOver(searchParameters, searchData))
        threadPool->stopSearching();

    // Check for stop
    if (stopped || exiting || stack->ply >= MAX_PLY - 1 || isDraw(board, stack->ply))
        return (stack->ply >= MAX_PLY - 1 && !board->checkers) ? evaluate(board, &nnue) : drawEval(this);

    stack->inCheck = board->checkerCount > 0;

    // TT Lookup
    bool ttHit = false;
    TTEntry* ttEntry = nullptr;
    Move ttMove = Move::none();
    Eval ttValue = EVAL_NONE;
    Eval ttEval = EVAL_NONE;
    uint8_t ttFlag = TT_NOBOUND;
    bool ttPv = pvNode;

    ttEntry = TT.probe(board->hashes.hash, &ttHit);
    if (ttHit) {
        ttMove = ttEntry->getMove();
        ttValue = valueFromTt(ttEntry->getValue(), stack->ply, board->rule50_ply);
        ttEval = ttEntry->getEval();
        ttFlag = ttEntry->getFlag();
        ttPv = ttPv || ttEntry->getTtPv();
    }

    // TT cutoff
    if (!pvNode && ttValue != EVAL_NONE && ((ttFlag == TT_UPPERBOUND && ttValue <= alpha) || (ttFlag == TT_LOWERBOUND && ttValue >= beta) || (ttFlag == TT_EXACTBOUND)))
        return ttValue;

    Move bestMove = Move::none();
    Eval bestValue, futilityValue, unadjustedEval;

    Eval correctionValue = history.getCorrectionValue(board, stack);
    stack->correctionValue = correctionValue;
    if (board->checkers) {
        stack->staticEval = bestValue = unadjustedEval = futilityValue = -EVAL_INFINITE;
        goto movesLoopQsearch;
    }
    else if (ttHit && ttEval != EVAL_NONE) {
        unadjustedEval = ttEval;
        stack->staticEval = bestValue = history.correctStaticEval(board->rule50_ply, unadjustedEval, correctionValue);

        if (ttValue != EVAL_NONE && std::abs(ttValue) < EVAL_TBWIN_IN_MAX_PLY && ((ttFlag == TT_UPPERBOUND && ttValue < bestValue) || (ttFlag == TT_LOWERBOUND && ttValue > bestValue) || (ttFlag == TT_EXACTBOUND)))
            bestValue = ttValue;
    }
    else {
        unadjustedEval = evaluate(board, &nnue);
        stack->staticEval = bestValue = history.correctStaticEval(board->rule50_ply, unadjustedEval, correctionValue);
        ttEntry->update(board->hashes.hash, Move::none(), 0, unadjustedEval, EVAL_NONE, board->rule50_ply, ttPv, TT_NOBOUND);
    }
    futilityValue = std::min(stack->staticEval + qsFutilityOffset, EVAL_TBWIN_IN_MAX_PLY - 1);

    // Stand pat
    if (bestValue >= beta) {
        if (std::abs(bestValue) < EVAL_TBWIN_IN_MAX_PLY && std::abs(beta) < EVAL_TBWIN_IN_MAX_PLY)
            bestValue = (bestValue + beta) / 2;
        ttEntry->update(board->hashes.hash, Move::none(), ttEntry->depth, unadjustedEval, EVAL_NONE, board->rule50_ply, ttPv, TT_NOBOUND);
        return bestValue;
    }
    if (alpha < bestValue)
        alpha = bestValue;

movesLoopQsearch:
    // Mate distance pruning
    alpha = std::max((int)alpha, (int)matedIn(stack->ply));
    beta = std::min((int)beta, (int)mateIn(stack->ply + 1));
    if (alpha >= beta)
        return alpha;

    // Moves loop
    MoveGen& movegen = movepickers[stack->ply][false] = MoveGen(board, &history, stack, ttMove, !board->checkers, 1);
    Move move;
    int moveCount = 0;
    bool playedQuiet = false;
    while ((move = movegen.nextMove())) {

        bool capture = board->isCapture(move);
        if (!capture && playedQuiet && bestValue > -EVAL_TBWIN_IN_MAX_PLY)
            continue;

        if (futilityValue > -EVAL_INFINITE && bestValue > -EVAL_TBWIN_IN_MAX_PLY) { // Only prune when not in check
            if (futilityValue <= alpha && !SEE(board, move, 1)) {
                bestValue = std::max(bestValue, futilityValue);
                continue;
            }

            if (!SEE(board, move, qsSeeMargin))
                break;

            if (!move.isPromotion() && moveCount > 2)
                continue;
        }

        if (!board->isLegal(move))
            continue;

        Hash newHash = board->hashAfter(move);
        TT.prefetch(newHash);
        moveCount++;
        searchData.nodesSearched++;

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

        assert(value > -EVAL_INFINITE && value < EVAL_INFINITE);

        if (stopped || exiting)
            return 0;

        if (value > bestValue) {
            bestValue = value;

            if (value > alpha) {
                bestMove = move;
                alpha = value;

                if (pvNode)
                    updatePv(stack, move);

                if (bestValue >= beta)
                    break;
            }
        }
    }

    if (bestValue == -EVAL_INFINITE) {
        assert(board->checkers && moveCount == 0);
        bestValue = matedIn(stack->ply); // Checkmate
    }

    if (!pvNode && std::abs(bestValue) < EVAL_TBWIN_IN_MAX_PLY && std::abs(beta) < EVAL_TBWIN_IN_MAX_PLY && bestValue >= beta) {
        bestValue = (bestValue + beta) / 2;
    }

    // Insert into TT
    int flags = bestValue >= beta ? TT_LOWERBOUND : TT_UPPERBOUND;
    ttEntry->update(board->hashes.hash, bestMove, 0, unadjustedEval, valueToTT(bestValue, stack->ply), board->rule50_ply, ttPv, flags);

    return bestValue;
}

template <NodeType nt>
Eval Worker::search(Board* board, SearchStack* stack, Depth depth, Eval alpha, Eval beta, bool cutNode) {
    constexpr bool rootNode = nt == ROOT_NODE;
    constexpr bool pvNode = nt == PV_NODE || nt == ROOT_NODE;
    constexpr NodeType nodeType = nt == ROOT_NODE ? PV_NODE : NON_PV_NODE;

    assert(-EVAL_INFINITE <= alpha && alpha < beta && beta <= EVAL_INFINITE);
    assert(!(pvNode && cutNode));
    assert(pvNode || alpha == beta - 1);

    // Set up PV length and selDepth
    if (pvNode)
        stack->pvLength = stack->ply;
    searchData.selDepth = std::max(stack->ply, searchData.selDepth);

    // Check for upcoming repetition
    if (!rootNode && alpha < 0 && hasUpcomingRepetition(board, stack->ply)) {
        alpha = drawEval(this);
        if (alpha >= beta)
            return alpha;
    }

    if (depth < 100) return qsearch<nodeType>(board, stack, alpha, beta);
    if (depth >= MAX_DEPTH) depth = MAX_DEPTH;

    if (!rootNode) {

        // Check for time / node limits on main thread
        if (mainThread && timeOver(searchParameters, searchData))
            threadPool->stopSearching();

        // Check for stop or max depth
        if (stopped || exiting || stack->ply >= MAX_PLY - 1 || isDraw(board, stack->ply))
            return (stack->ply >= MAX_PLY - 1 && !board->checkers) ? evaluate(board, &nnue) : drawEval(this);

        // Mate distance pruning
        alpha = std::max((int)alpha, (int)matedIn(stack->ply));
        beta = std::min((int)beta, (int)mateIn(stack->ply + 1));
        if (alpha >= beta)
            return alpha;
    }

    // Initialize some stuff
    Move bestMove = Move::none();
    Move excludedMove = stack->excludedMove;
    Eval bestValue = -EVAL_INFINITE, maxValue = EVAL_INFINITE;
    Eval oldAlpha = alpha;
    bool improving = false, excluded = static_cast<bool>(excludedMove);

    (stack + 1)->killer = Move::none();
    (stack + 1)->excludedMove = Move::none();
    stack->inCheck = board->checkerCount > 0;

    // TT Lookup
    bool ttHit = false;
    TTEntry* ttEntry = nullptr;
    Move ttMove = Move::none();
    Eval ttValue = EVAL_NONE, ttEval = EVAL_NONE;
    int ttDepth = 0;
    uint8_t ttFlag = TT_NOBOUND;
    stack->ttPv = excluded ? stack->ttPv : pvNode;

    if (!excluded) {
        ttEntry = TT.probe(board->hashes.hash, &ttHit);
        if (ttHit) {
            ttMove = rootNode && rootMoves[0].value > -EVAL_INFINITE ? rootMoves[0].move : ttEntry->getMove();
            ttValue = valueFromTt(ttEntry->getValue(), stack->ply, board->rule50_ply);
            ttEval = ttEntry->getEval();
            ttDepth = ttEntry->getDepth();
            ttFlag = ttEntry->getFlag();
            stack->ttPv = stack->ttPv || ttEntry->getTtPv();
        }
    }

    // TT cutoff
    if (!pvNode && ttDepth >= depth - ttCutOffset + ttCutFailHighMargin * (ttValue >= beta) && ttValue != EVAL_NONE && ((ttFlag == TT_UPPERBOUND && ttValue <= alpha) || (ttFlag == TT_LOWERBOUND && ttValue >= beta) || (ttFlag == TT_EXACTBOUND)))
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
                tbValue = stack->ply - EVAL_TBWIN;
                tbBound = TT_UPPERBOUND;
            }
            else if (result == TB_WIN) {
                tbValue = EVAL_TBWIN - stack->ply;
                tbBound = TT_LOWERBOUND;
            }
            else {
                tbValue = 0;
                tbBound = TT_EXACTBOUND;
            }

            if (tbBound == TT_EXACTBOUND || (tbBound == TT_LOWERBOUND ? tbValue >= beta : tbValue <= alpha)) {
                ttEntry->update(board->hashes.hash, Move::none(), depth, EVAL_NONE, valueToTT(tbValue, stack->ply), board->rule50_ply, stack->ttPv, tbBound);
                return tbValue;
            }

            if (pvNode) {
                if (tbBound == TT_LOWERBOUND) {
                    bestValue = tbValue;
                    alpha = std::max(alpha, bestValue);
                }
                else {
                    maxValue = tbValue;
                }
            }
        }
    }

    // Static evaluation
    Eval eval = EVAL_NONE, unadjustedEval = EVAL_NONE, probCutBeta = EVAL_NONE;

    Eval correctionValue = history.getCorrectionValue(board, stack);
    stack->correctionValue = correctionValue;
    if (board->checkers) {
        stack->staticEval = EVAL_NONE;

        if (ttHit && ttValue != EVAL_NONE && ((ttFlag == TT_UPPERBOUND && ttValue <= alpha) || (ttFlag == TT_LOWERBOUND && ttValue >= beta) || (ttFlag == TT_EXACTBOUND)))
            eval = ttValue;
    }
    else if (excluded) {
        unadjustedEval = eval = stack->staticEval;
    }
    else if (ttHit) {
        unadjustedEval = ttEval != EVAL_NONE ? ttEval : evaluate(board, &nnue);
        eval = stack->staticEval = history.correctStaticEval(board->rule50_ply, unadjustedEval, correctionValue);

        if (ttValue != EVAL_NONE && ((ttFlag == TT_UPPERBOUND && ttValue < eval) || (ttFlag == TT_LOWERBOUND && ttValue > eval) || (ttFlag == TT_EXACTBOUND)))
            eval = ttValue;
    }
    else {
        unadjustedEval = evaluate(board, &nnue);
        eval = stack->staticEval = history.correctStaticEval(board->rule50_ply, unadjustedEval, correctionValue);

        ttEntry->update(board->hashes.hash, Move::none(), 0, unadjustedEval, EVAL_NONE, board->rule50_ply, stack->ttPv, TT_NOBOUND);
    }

    // Improving
    if (!board->checkers) {
        if ((stack - 2)->staticEval != EVAL_NONE) {
            improving = stack->staticEval > (stack - 2)->staticEval;
        }
        else if ((stack - 4)->staticEval != EVAL_NONE) {
            improving = stack->staticEval > (stack - 4)->staticEval;
        }
    }

    // Adjust quiet history based on how much the previous move changed static eval
    if (!excluded && !board->checkers && (stack - 1)->movedPiece != Piece::NONE && !(stack - 1)->capture && !(stack - 1)->inCheck && stack->ply > 1) {
        int bonus = std::clamp(staticHistoryFactor * int(stack->staticEval + (stack - 1)->staticEval) / 10, staticHistoryMin, staticHistoryMax) + staticHistoryTempo;
        history.updateQuietHistory((stack - 1)->move, flip(board->stm), board - 1, bonus);
    }

    // IIR
    if (depth >= iirMinDepth + iirCheckDepth * !!board->checkers && (!ttHit || ttDepth + iirLowTtDepthOffset < depth))
        depth -= iirReduction;

    // Post-LMR depth adjustments
    if (!board->checkers && (stack - 1)->inLMR) {
        int additionalReduction = 0;
        if ((stack - 1)->reduction >= postlmrOppWorseningThreshold && stack->staticEval <= -(stack - 1)->staticEval)
            additionalReduction -= postlmrOppWorseningReduction;

        depth -= additionalReduction;
        (stack - 1)->reduction += additionalReduction;
    }

    // Reverse futility pruning
    if (!rootNode && depth <= rfpDepth && std::abs(eval) < EVAL_TBWIN_IN_MAX_PLY) {
        int rfpMargin, rfpDepth;
        if (board->checkers) {
            rfpDepth = depth - rfpImprovingOffsetCheck * (improving && !board->opponentHasGoodCapture());
            rfpMargin = rfpBaseCheck + rfpFactorLinearCheck * rfpDepth / 100 + rfpFactorQuadraticCheck * rfpDepth * rfpDepth / 1000000;
        } else {
            rfpDepth = depth - rfpImprovingOffset * (improving && !board->opponentHasGoodCapture());
            rfpMargin = rfpBase + rfpFactorLinear * rfpDepth / 100 + rfpFactorQuadratic * rfpDepth * rfpDepth / 1000000;
        }
        if (eval - rfpMargin >= beta) {
            return std::min((eval + beta) / 2, EVAL_TBWIN_IN_MAX_PLY - 1);
        }
    }

    // Razoring
    if (!rootNode && !board->checkers && depth <= razoringDepth && eval + (razoringFactor * depth) / 100 < alpha && alpha < EVAL_TBWIN_IN_MAX_PLY) {
        Eval razorValue = qsearch<NON_PV_NODE>(board, stack, alpha, beta);
        if (razorValue <= alpha && std::abs(razorValue) < EVAL_TBWIN_IN_MAX_PLY)
            return razorValue;
    }

    // Null move pruning
    if (!pvNode
        && !board->checkers
        && eval >= beta
        && eval >= stack->staticEval
        && stack->staticEval + nmpEvalDepth * depth / 100 - nmpEvalBase >= beta
        && std::abs(beta) < EVAL_TBWIN_IN_MAX_PLY
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
        int R = nmpRedBase + 100 * depth / nmpDepthDiv + std::min(100 * (eval - beta) / nmpDivisor, nmpMin);

        Board* boardCopy = doNullMove(board);
        Eval nullValue = -search<NON_PV_NODE>(boardCopy, stack + 1, depth - R, -beta, -beta + 1, !cutNode);
        undoNullMove();

        if (stopped || exiting)
            return 0;

        if (nullValue >= beta) {
            if (nullValue >= EVAL_TBWIN_IN_MAX_PLY)
                nullValue = beta;

            if (searchData.nmpPlies || depth < 1500)
                return nullValue;

            searchData.nmpPlies = stack->ply + (depth - R) * 2 / 300;
            Eval verificationValue = search<NON_PV_NODE>(board, stack, depth - R, beta - 1, beta, false);
            searchData.nmpPlies = 0;

            if (verificationValue >= beta)
                return nullValue;
        }
    }

    // ProbCut
    probCutBeta = std::min(beta + probCutBetaOffset, EVAL_TBWIN_IN_MAX_PLY - 1);
    if (!pvNode
        && !board->checkers
        && !excluded
        && depth > probCutDepth
        && std::abs(beta) < EVAL_TBWIN_IN_MAX_PLY - 1
        && !(ttDepth >= depth - probcutReduction && ttValue != EVAL_NONE && ttValue < probCutBeta)) {

        assert(probCutBeta > beta);
        assert(probCutBeta < EVAL_TBWIN_IN_MAX_PLY);

        Move probcutTtMove = ttMove && board->isPseudoLegal(ttMove) && SEE(board, ttMove, probCutBeta - stack->staticEval) ? ttMove : Move::none();
        MoveGen movegen(board, &history, stack, probcutTtMove, probCutBeta - stack->staticEval, depth / 100);
        Move move;
        while ((move = movegen.nextMove())) {
            if (move == excludedMove || !board->isLegal(move))
                continue;

            Hash newHash = board->hashAfter(move);
            TT.prefetch(newHash);

            Square origin = move.origin();
            Square target = move.target();
            stack->capture = board->isCapture(move);
            stack->move = move;
            stack->movedPiece = board->pieces[origin];
            stack->contHist = history.continuationHistory[board->stm][stack->movedPiece][target];
            stack->contCorrHist = &history.continuationCorrectionHistory[board->stm][stack->movedPiece][target][board->isSquareThreatened(origin)][board->isSquareThreatened(target)];;

            Board* boardCopy = doMove(board, newHash, move);

            Eval value = -qsearch<NON_PV_NODE>(boardCopy, stack + 1, -probCutBeta, -probCutBeta + 1);

            if (value >= probCutBeta)
                value = -search<NON_PV_NODE>(boardCopy, stack + 1, depth - probcutReduction - 100, -probCutBeta, -probCutBeta + 1, !cutNode);

            undoMove();

            if (stopped || exiting)
                return 0;

            if (value >= probCutBeta) {
                value = std::min(value, EVAL_TBWIN_IN_MAX_PLY - 1);
                ttEntry->update(board->hashes.hash, move, depth - probcutReduction, unadjustedEval, valueToTT(value, stack->ply), board->rule50_ply, stack->ttPv, TT_LOWERBOUND);
                return value;
            }
        }

    }

    // IIR 2: Electric boolagoo
    if (!board->checkers && !ttHit && depth >= iir2MinDepth && pvNode)
        depth -= iir2Reduction;

    if (stopped || exiting)
        return 0;

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

        uint64_t nodesBeforeMove = searchData.nodesSearched;

        bool capture = board->isCapture(move);
        bool importantCapture = stack->ttPv && capture && !cutNode;

        if (!rootNode
            && bestValue > -EVAL_TBWIN_IN_MAX_PLY
            && board->hasNonPawns()
            ) {

            Depth reduction = REDUCTIONS[int(capture) + int(importantCapture)][depth / 100][moveCount];
            reduction += earlyLmrImproving * !improving;
            if (capture)
                reduction -= 100 * *history.getCaptureHistory(board, move) / earlyLmrHistoryFactorCapture;
            else
                reduction -= 100 * history.getWeightedQuietHistory(board, stack, move, std::make_tuple(earlyLmrQhWeight, earlyLmrPhWeight, earlyLmrCh1Weight, earlyLmrCh2Weight, earlyLmrCh3Weight, earlyLmrCh4Weight, earlyLmrCh6Weight)) / earlyLmrHistoryFactorQuiet;
            Depth lmrDepth = depth - reduction;

            if (!movegen.skipQuiets) {

                // Movecount pruning (LMP)
                if (!pvNode && moveCount >= LMP_MARGIN[depth / 100][improving]) {
                    movegen.skipQuietMoves();
                }

                // Futility pruning
                if (!capture) {
                    int fpValue = eval + fpBase + fpFactor * lmrDepth / 100 + pvNode * (fpPvNode + fpPvNodeNoBestMove * !bestMove);
                    fpValue += history.getWeightedQuietHistory(board, stack, move, std::make_tuple(fpQhWeight, fpPhWeight, fpCh1Weight, fpCh2Weight, fpCh3Weight, fpCh4Weight, fpCh6Weight)) / fpHistoryWeight;
                    if (lmrDepth < fpDepth && fpValue <= alpha) {
                        movegen.skipQuietMoves();
                    }
                }
            }

            lmrDepth = std::max<Depth>(0, lmrDepth);

            // Futility pruning for captures
            if (!pvNode && capture && !move.isPromotion()) {
                Piece capturedPiece = move.isEnpassant() ? Piece::PAWN : board->pieces[move.target()];
                if (lmrDepth < fpCaptDepth && eval + fpCaptBase + PIECE_VALUES[capturedPiece] + fpCaptFactor * lmrDepth / 100 <= alpha)
                    continue;
            }

            lmrDepth = std::min(std::min<Depth>(depth + 100, MAX_DEPTH), lmrDepth);

            // History pruning
            if (!pvNode && lmrDepth < historyPruningDepth) {
                if (capture) {
                    if (*history.getCaptureHistory(board, move) < historyPruningFactorCapture * depth / 100)
                        continue;
                } else {
                    if (history.getWeightedQuietHistory(board, stack, move, std::make_tuple(hpQhWeight, hpPhWeight, hpCh1Weight, hpCh2Weight, hpCh3Weight, hpCh4Weight, hpCh6Weight)) < historyPruningFactorQuiet * depth / 100)
                        continue;
                }
            }

            // SEE Pruning
            int seeMargin = capture ? -seeFactorCapture * depth * depth / 10000 : -seeFactorQuiet * lmrDepth / 100;
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
            && std::abs(ttValue) < EVAL_TBWIN_IN_MAX_PLY
            && ttDepth >= depth - extensionTtDepthOffset
            ) {
            Eval singularBeta = ttValue - (1 + (stack->ttPv && !pvNode)) * depth / 100;
            int singularDepth = (depth - 100) / 2;

            bool currTtPv = stack->ttPv;
            stack->excludedMove = move;
            Eval singularValue = search<NON_PV_NODE>(board, stack, singularDepth, singularBeta - 1, singularBeta, cutNode);
            stack->excludedMove = Move::none();
            stack->ttPv = currTtPv;

            if (stopped || exiting)
                return 0;

            if (singularValue < singularBeta) {
                // This move is singular and we should investigate it further
                extension = singleExtension;
                if (!pvNode && singularValue + doubleExtensionMargin < singularBeta) {
                    extension = doubleExtension;
                    depth += doubleExtensionDepthIncreaseFactor * (depth < doubleExtensionDepthIncrease);
                    if (!board->isCapture(move) && singularValue + tripleExtensionMargin < singularBeta)
                        extension = tripleExtension;
                }
            }
            // Multicut: If we beat beta, that means there's likely more moves that beat beta and we can skip this node
            else if (singularBeta >= beta) {
                Eval value = std::min(singularBeta, EVAL_TBWIN_IN_MAX_PLY - 1);
                ttEntry->update(board->hashes.hash, ttMove, singularDepth, unadjustedEval, value, board->rule50_ply, stack->ttPv, TT_LOWERBOUND);

                // Adjust correction history
                if (!board->checkers && singularValue > stack->staticEval) {
                    int bonus = std::clamp((int(singularValue - stack->staticEval) * singularDepth / 100) * correctionHistoryFactorMulticut / 1024, -CORRECTION_HISTORY_LIMIT / 4, CORRECTION_HISTORY_LIMIT / 4);
                    history.updateCorrectionHistory(board, stack, bonus);
                }

                return value;
            }
            // We didn't prove singularity and an excluded search couldn't beat beta, but if the ttValue can we still reduce the depth
            else if (ttValue >= beta)
                extension = ttValueNegExt;
            // We didn't prove singularity and an excluded search couldn't beat beta, but we are expected to fail low, so reduce
            else if (cutNode)
                extension = cutNodeNegExt;
        }

        Hash newHash = board->hashAfter(move);
        TT.prefetch(newHash);

        // Some setup stuff
        Square origin = move.origin();
        Square target = move.target();
        stack->capture = capture;
        stack->move = move;
        stack->movedPiece = board->pieces[origin];
        stack->contHist = history.continuationHistory[board->stm][stack->movedPiece][target];
        stack->contCorrHist = &history.continuationCorrectionHistory[board->stm][stack->movedPiece][target][board->isSquareThreatened(origin)][board->isSquareThreatened(target)];;

        moveCount++;
        searchData.nodesSearched++;

        Board* boardCopy = doMove(board, newHash, move);

        Eval value = 0;
        int newDepth = depth - 100 + extension;
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
                reduction += lmrTtpvFaillow(importantCapture) * (ttHit && ttValue <= alpha);
            }

            if (capture) {
                int captureHistory = *history.getCaptureHistory(board, move);
                reduction -= captureHistory * std::abs(captureHistory) / lmrCaptureHistoryDivisor(importantCapture);

                if (importantCapture) {
                    reduction += lmrImportantBadCaptureOffset * (movegen.stage == STAGE_PLAY_BAD_CAPTURES);
                    reduction = lmrImportantCaptureFactor * reduction / 100;
                }
            }
            else {
                int quietHistory = history.getWeightedQuietHistory(board, stack, move, std::make_tuple(lmrQhWeight, lmrPhWeight, lmrCh1Weight, lmrCh2Weight, lmrCh3Weight, lmrCh4Weight, lmrCh6Weight));
                reduction -= 100 * quietHistory / lmrQuietHistoryDivisor;
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

            bool doShallowerSearch = !rootNode && value < bestValue + newDepth / 100;
            bool doDeeperSearch = value > (bestValue + lmrDeeperBase + lmrDeeperFactor * newDepth / 100);
            newDepth += lmrDeeperWeight * doDeeperSearch - lmrShallowerWeight * doShallowerSearch;

            if (value > alpha && reducedDepth < newDepth && !(ttValue < alpha && ttDepth - lmrResearchSkipDepthOffset >= newDepth && (ttFlag & TT_UPPERBOUND))) {
                value = -search<NON_PV_NODE>(boardCopy, stack + 1, newDepth, -(alpha + 1), -alpha, !cutNode);
                moveSearchCount++;

                if (!capture) {
                    int bonus = std::min(lmrPassBonusBase + lmrPassBonusFactor * (value > alpha ? depth / 100 : reducedDepth / 100), lmrPassBonusMax);
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
        if (pvNode && (moveCount == 1 || value > alpha)) {
            if (move == ttMove && searchData.rootDepth > 8 && ttDepth > 1)
                newDepth = std::max(100, newDepth);

            value = -search<PV_NODE>(boardCopy, stack + 1, newDepth, -beta, -alpha, false);
            moveSearchCount++;
        }

        undoMove();
        assert(value > -EVAL_INFINITE && value < EVAL_INFINITE);

        SearchedMoveList& list = capture ? captureMoves : quietMoves;
        if (list.size() < list.capacity())
            list.add({move, moveSearchCount});

        if (stopped || exiting)
            return 0;

        if (rootNode) {
            if (rootMoveNodes.count(move) == 0)
                rootMoveNodes[move] = searchData.nodesSearched - nodesBeforeMove;
            else
                rootMoveNodes[move] = searchData.nodesSearched - nodesBeforeMove + rootMoveNodes[move];

            RootMove* rootMove = &rootMoves[0];
            for (RootMove& rm : rootMoves) {
                if (rm.move == move) {
                    rootMove = &rm;
                    break;
                }
            }

            rootMove->meanScore = rootMove->meanScore == EVAL_NONE ? value : (rootMove->meanScore + value) / 2;

            if (moveCount == 1 || value > alpha) {
                rootMove->value = value;
                rootMove->depth = searchData.rootDepth;
                rootMove->selDepth = searchData.selDepth;

                rootMove->pv.resize(0);
                rootMove->pv.push_back(move);
                for (int i = 1; i < (stack + 1)->pvLength; i++)
                    rootMove->pv.push_back((stack + 1)->pv[i]);
            }
            else {
                rootMove->value = -EVAL_INFINITE;
            }
        }

        if (value > bestValue) {
            bestValue = value;

            if (value > alpha) {
                bestMove = move;
                alpha = value;

                if (pvNode)
                    updatePv(stack, move);

                if (bestValue >= beta) {

                    int historyUpdateDepth = depth / 100 + (eval <= alpha) + (value - historyDepthBetaOffset > beta);

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

                if (depth > lowDepthPvDepthReductionMin && depth < lowDepthPvDepthReductionMax && beta < EVAL_TBWIN_IN_MAX_PLY && value > -EVAL_TBWIN_IN_MAX_PLY)
                    depth -= lowDepthPvDepthReductionWeight;
            }
        }

    }

    if (stopped || exiting)
        return 0;

    if (!pvNode && bestValue >= beta && std::abs(bestValue) < EVAL_TBWIN_IN_MAX_PLY && std::abs(beta) < EVAL_TBWIN_IN_MAX_PLY && std::abs(alpha) < EVAL_TBWIN_IN_MAX_PLY)
        bestValue = (bestValue * depth + 100 * beta) / (depth + 100);

    if (moveCount == 0) {
        if (board->checkers && excluded)
            return -EVAL_INFINITE;
        // Mate / Stalemate
        bestValue = board->checkers ? matedIn(stack->ply) : 0;
    }

    if (pvNode)
        bestValue = std::min(bestValue, maxValue);

    // Insert into TT
    bool failLow = alpha == oldAlpha;
    bool failHigh = bestValue >= beta;
    int flags = failHigh ? TT_LOWERBOUND : !failLow ? TT_EXACTBOUND : TT_UPPERBOUND;
    if (!excluded)
        ttEntry->update(board->hashes.hash, bestMove, depth, unadjustedEval, valueToTT(bestValue, stack->ply), board->rule50_ply, stack->ttPv, flags);

    // Adjust correction history
    if (!board->checkers && (!bestMove || !board->isCapture(bestMove)) && (!failHigh || bestValue > stack->staticEval) && (!failLow || bestValue <= stack->staticEval)) {
        int bonus = std::clamp((int(bestValue - stack->staticEval) * depth / 100) * correctionHistoryFactor / 1024, -CORRECTION_HISTORY_LIMIT / 4, CORRECTION_HISTORY_LIMIT / 4);
        history.updateCorrectionHistory(board, stack, bonus);
    }

    assert(bestValue > -EVAL_INFINITE && bestValue < EVAL_INFINITE);

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

    searchData.nodesSearched = 0;
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
            Move bestMove = bestTbMove && std::abs(bestThread->rootMoves[0].value) < EVAL_MATE_IN_MAX_PLY ? bestTbMove : bestThread->rootMoves[0].move;
            std::cout << "bestmove " << bestMove.toString(UCI::Options.chess960.value) << std::endl;
        }
        else {
            std::cout << "bestmove " << bestThread->rootMoves[0].move.toString(UCI::Options.chess960.value) << " ponder " << bestThread->rootMoves[0].pv[1].toString(UCI::Options.chess960.value) << std::endl;
        }
    }
}

void Worker::iterativeDeepening() {
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

    Eval baseValue = EVAL_NONE;
    Eval previousValue = EVAL_NONE;
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
                stackList[i].ply = i - STACK_OVERHEAD;
                stackList[i].staticEval = EVAL_NONE;
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
            Eval delta = 2 * EVAL_INFINITE;
            Eval alpha = -EVAL_INFINITE;
            Eval beta = EVAL_INFINITE;
            Eval value;

            if (depth >= aspirationWindowMinDepth) {
                // Set up interval for the start of this aspiration window
                if (rootMoves[0].meanScore == EVAL_NONE)
                    delta = aspirationWindowDelta;
                else
                    delta = aspirationWindowDeltaBase + rootMoves[0].meanScore * rootMoves[0].meanScore / aspirationWindowDeltaDivisor;
                alpha = std::max(previousValue - delta, -EVAL_INFINITE);
                beta = std::min(previousValue + delta, (int)EVAL_INFINITE);
            }

            int failHighs = 0;
            while (true) {
                int searchDepth = std::max(1, depth - failHighs);
                value = search<ROOT_NODE>(board, stack, searchDepth * 100, alpha, beta, false);

                sortRootMoves();

                // Stop if we need to
                if (stopped || exiting)
                    break;

                // Our window was too high, lower alpha for next iteration
                if (value <= alpha) {
                    beta = (alpha + beta) / 2;
                    alpha = std::max(value - delta, -EVAL_INFINITE);
                    failHighs = 0;
                }
                // Our window was too low, increase beta for next iteration
                else if (value >= beta) {
                    beta = std::min(value + delta, (int)EVAL_INFINITE);
                    failHighs = std::min(failHighs + 1, aspirationWindowMaxFailHighs);
                }
                // Our window was good, increase depth for next iteration
                else
                    break;

                if (value >= EVAL_TBWIN_IN_MAX_PLY) {
                    beta = EVAL_INFINITE;
                    failHighs = 0;
                }

                delta *= aspirationWindowDeltaFactor;
            }

            if (stopped || exiting)
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
            tmAdjustment *= tmEvalDiffBase + std::clamp(previousValue - rootMoves[0].value, tmEvalDiffMin, tmEvalDiffMax) * tmEvalDiffFactor;

            // Based on fraction of nodes that went into the best move
            tmAdjustment *= tmNodesBase - tmNodesFactor * ((double)rootMoveNodes[rootMoves[0].move] / (double)searchData.nodesSearched);

            // Based on search score complexity
            if (baseValue != EVAL_NONE) {
                double complexity = 0.6 * std::abs(baseValue - rootMoves[0].value) * std::log(depth);
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
        std::cout << "info depth " << rootMove.depth << " seldepth " << rootMove.selDepth << " score " << formatEval(rootMove.value) << " multipv " << (rootMoveIdx + 1) << " nodes " << nodes << " tbhits " << threadPool->tbhits() << " time " << ms << " nps " << nps << " hashfull " << TT.hashfull() << " pv ";

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
        return rm1.value > rm2.value;
        });
}

Worker* Worker::chooseBestThread() {
    Worker* bestThread = this;

    if (threadPool->workers.size() > 1 && UCI::Options.multiPV.value == 1) {
        std::map<Move, int64_t> votes;
        Eval minValue = EVAL_INFINITE;

        for (auto& worker : threadPool->workers) {
            if (worker->rootMoves[0].value == -EVAL_INFINITE)
                break;
            minValue = std::min(minValue, worker->rootMoves[0].value);
        }

        for (auto& worker : threadPool->workers) {
            auto& rm = worker->rootMoves[0];
            if (rm.value == -EVAL_INFINITE)
                break;
            votes[rm.move] += (rm.value - minValue + 11) * rm.depth;
        }

        for (auto& worker : threadPool->workers) {
            Worker* thread = worker.get();
            RootMove& rm = thread->rootMoves[0];
            if (rm.value == -EVAL_INFINITE)
                break;

            Eval thValue = rm.value;
            Eval bestValue = bestThread->rootMoves[0].value;
            Move thMove = rm.move;
            Move bestMove = bestThread->rootMoves[0].move;

            // In case of checkmate, take the shorter mate / longer getting mated
            if (std::abs(bestValue) >= EVAL_TBWIN_IN_MAX_PLY) {
                if (thValue > bestValue) {
                    bestThread = thread;
                }
            }
            // We have found a mate, take it without voting
            else if (thValue >= EVAL_TBWIN_IN_MAX_PLY) {
                bestThread = thread;
            }
            // No mate found by any thread so far, take the thread with more votes
            else if (votes[thMove] > votes[bestMove]) {
                bestThread = thread;
            }
            // In case of same move, choose the thread with the highest score
            else if (thMove == bestMove && std::abs(thValue) > std::abs(bestValue) && rm.pv.size() > 2) {
                bestThread = thread;
            }
        }
    }

    return bestThread;
}

void Worker::tdatagen() {
    nnue.reset(&rootBoard);

    searchData.nodesSearched = 0;
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

    Eval previousValue = EVAL_NONE;

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
            stackList[i].ply = i - STACK_OVERHEAD;
            stackList[i].staticEval = EVAL_NONE;
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
        Eval delta = 2 * EVAL_INFINITE;
        Eval alpha = -EVAL_INFINITE;
        Eval beta = EVAL_INFINITE;
        Eval value;

        if (depth >= aspirationWindowMinDepth) {
            // Set up interval for the start of this aspiration window
            delta = aspirationWindowDelta;
            alpha = std::max(previousValue - delta, -EVAL_INFINITE);
            beta = std::min(previousValue + delta, (int)EVAL_INFINITE);
        }

        int failHighs = 0;
        while (true) {
            int searchDepth = std::max(1, depth - failHighs);
            value = search<ROOT_NODE>(board, stack, searchDepth * 100, alpha, beta, false);

            sortRootMoves();

            // Stop if we need to
            if (stopped || exiting || searchData.nodesSearched >= searchParameters.nodes)
                break;

            // Our window was too high, lower alpha for next iteration
            if (value <= alpha) {
                beta = (alpha + beta) / 2;
                alpha = std::max(value - delta, -EVAL_INFINITE);
                failHighs = 0;
            }
            // Our window was too low, increase beta for next iteration
            else if (value >= beta) {
                beta = std::min(value + delta, (int)EVAL_INFINITE);
                failHighs = std::min(failHighs + 1, aspirationWindowMaxFailHighs);
            }
            // Our window was good, increase depth for next iteration
            else
                break;

            if (value >= EVAL_TBWIN_IN_MAX_PLY) {
                beta = EVAL_INFINITE;
                failHighs = 0;
            }

            delta *= aspirationWindowDeltaFactor;
        }

        if (stopped || exiting || searchData.nodesSearched >= searchParameters.nodes)
            break;

        previousValue = rootMoves[0].value;
    }

    sortRootMoves();
    printUCI(this);

    std::cout << "bestmove " << rootMoves[0].move.toString(UCI::Options.chess960.value) << std::endl;
}
