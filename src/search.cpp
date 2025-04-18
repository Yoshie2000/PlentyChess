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

// Time management
TUNE_FLOAT_DISABLED(tmInitialAdjustment, 1.0796307320901835f, 0.5f, 1.5f);
TUNE_INT_DISABLED(tmBestMoveStabilityMax, 18, 10, 30);
TUNE_FLOAT_DISABLED(tmBestMoveStabilityBase, 1.5114270553993898f, 0.75f, 2.5f);
TUNE_FLOAT_DISABLED(tmBestMoveStabilityFactor, 0.05192744990530549f, 0.001f, 0.1f);
TUNE_FLOAT_DISABLED(tmEvalDiffBase, 0.9640137856251421f, 0.5f, 1.5f);
TUNE_FLOAT_DISABLED(tmEvalDiffFactor, 0.0038870466506728298f, 0.001f, 0.1f);
TUNE_INT_DISABLED(tmEvalDiffMin, -9, -250, 50);
TUNE_INT_DISABLED(tmEvalDiffMax, 63, -50, 250);
TUNE_FLOAT_DISABLED(tmNodesBase, 1.6874618043176948f, 0.5f, 5.0f);
TUNE_FLOAT_DISABLED(tmNodesFactor, 0.921048521228935f, 0.1f, 2.5f);

// Aspiration windows
TUNE_INT_DISABLED(aspirationWindowMinDepth, 4, 2, 6);
TUNE_INT_DISABLED(aspirationWindowDelta, 14, 1, 30);
TUNE_INT_DISABLED(aspirationWindowDeltaBase, 10, 1, 30);
TUNE_INT(aspirationWindowDeltaDivisor, 13002, 7500, 17500);
TUNE_INT_DISABLED(aspirationWindowMaxFailHighs, 3, 1, 10);
TUNE_FLOAT_DISABLED(aspirationWindowDeltaFactor, 1.5804938062670641f, 1.0f, 3.0f);

// Reduction / Margin tables
TUNE_FLOAT(lmrReductionNoisyBase, -0.13908573295700846f, -2.0f, -0.1f);
TUNE_FLOAT(lmrReductionNoisyFactor, 3.2138426839474468f, 2.0f, 4.0f);
TUNE_FLOAT(lmrReductionQuietBase, 1.203634500924053f, 0.50f, 1.5f);
TUNE_FLOAT(lmrReductionQuietFactor, 2.8792614798870306f, 2.0f, 4.0f);

TUNE_FLOAT(seeMarginNoisy, -24.47291348333615f, -50.0f, -10.0f);
TUNE_FLOAT(seeMarginQuiet, -76.22223052179581f, -100.0f, -50.0f);
TUNE_FLOAT(lmpMarginWorseningBase, 2.0264034228074035f, -1.0f, 2.5f);
TUNE_FLOAT(lmpMarginWorseningFactor, 0.36773070268987235f, 0.1f, 1.5f);
TUNE_FLOAT(lmpMarginWorseningPower, 1.6488172531400145f, 1.0f, 3.0f);
TUNE_FLOAT(lmpMarginImprovingBase, 2.9529829700559365f, 2.0f, 5.0f);
TUNE_FLOAT(lmpMarginImprovingFactor, 0.8372651393568566f, 0.5f, 2.0f);
TUNE_FLOAT(lmpMarginImprovingPower, 1.974333065229244f, 1.0f, 3.0f);

// Search values
TUNE_INT(qsFutilityOffset, 68, 1, 125);
TUNE_INT(qsSeeMargin, -103, -200, 50);

// Pre-search pruning
TUNE_INT_DISABLED(iirMinDepth, 4, 1, 10);

TUNE_INT(staticHistoryFactor, -38, -500, -1);
TUNE_INT(staticHistoryMin, -100, -1000, -1);
TUNE_INT(staticHistoryMax, 160, 1, 1000);

TUNE_INT_DISABLED(rfpDepth, 8, 2, 20);
TUNE_INT(rfpFactor, 97, 1, 250);

TUNE_INT_DISABLED(razoringDepth, 5, 2, 20);
TUNE_INT(razoringFactor, 238, 1, 1000);

TUNE_INT_DISABLED(nmpRedBase, 4, 1, 5);
TUNE_INT_DISABLED(nmpDepthDiv, 3, 1, 6);
TUNE_INT_DISABLED(nmpMin, 4, 1, 10);
TUNE_INT(nmpDivisor, 186, 10, 1000);
TUNE_INT(nmpEvalDepth, 21, 1, 100);
TUNE_INT(nmpEvalBase, 161, 50, 300);

TUNE_INT(probCutBetaOffset, 204, 1, 500);
TUNE_INT_DISABLED(probCutDepth, 5, 1, 15);

// In-search pruning
TUNE_INT(earlyLmrHistoryFactorQuiet, 16277, 10000, 20000);
TUNE_INT(earlyLmrHistoryFactorCapture, 14971, 10000, 20000);

TUNE_INT_DISABLED(fpDepth, 11, 1, 20);
TUNE_INT(fpBase, 216, 1, 1000);
TUNE_INT(fpFactor, 107, 1, 500);

TUNE_INT_DISABLED(fpCaptDepth, 9, 1, 20);
TUNE_INT(fpCaptBase, 411, 150, 750);
TUNE_INT(fpCaptFactor, 376, 100, 600);

TUNE_INT_DISABLED(historyPruningDepth, 4, 1, 15);
TUNE_INT(historyPruningFactorCapture, -1727, -8192, -128);
TUNE_INT(historyPruningFactorQuiet, -6019, -8192, -128);

TUNE_INT_DISABLED(doubleExtensionMargin, 6, 1, 30);
TUNE_INT_DISABLED(doubleExtensionDepthIncrease, 11, 2, 20);
TUNE_INT_DISABLED(tripleExtensionMargin, 41, 25, 100);

TUNE_INT_DISABLED(lmrMcBase, 2, 1, 10);
TUNE_INT_DISABLED(lmrMcPv, 2, 1, 10);
TUNE_INT_DISABLED(lmrMinDepth, 3, 1, 10);

TUNE_INT(lmrCheck, 931, 0, 5000);
TUNE_INT(lmrTtPv, 1597, 0, 5000);
TUNE_INT(lmrCutnode, 1997, 0, 5000);
TUNE_INT(lmrCorrection, 16074, 1000, 20000);
TUNE_INT(lmrHistoryFactorQuiet, 24297, 10000, 30000);
TUNE_INT(lmrHistoryFactorCapture, 310166, 250000, 400000);

TUNE_INT_DISABLED(lmrDeeperBase, 40, 1, 100);
TUNE_INT_DISABLED(lmrDeeperFactor, 2, 0, 10);

TUNE_INT(lmrPassBonusBase, -313, -500, 500);
TUNE_INT(lmrPassBonusFactor, 200, 1, 500);
TUNE_INT(lmrPassBonusMax, 1107, 32, 4096);

TUNE_INT(historyBonusQuietBase, 42, -500, 500);
TUNE_INT(historyBonusQuietFactor, 208, 1, 500);
TUNE_INT(historyBonusQuietMax, 2219, 32, 4096);
TUNE_INT(historyBonusCaptureBase, 25, -500, 500);
TUNE_INT(historyBonusCaptureFactor, 166, 1, 500);
TUNE_INT(historyBonusCaptureMax, 1836, 32, 4096);
TUNE_INT(historyMalusQuietBase, 32, -500, 500);
TUNE_INT(historyMalusQuietFactor, 226, 1, 500);
TUNE_INT(historyMalusQuietMax, 1820, 32, 4096);
TUNE_INT(historyMalusCaptureBase, 118, -500, 500);
TUNE_INT(historyMalusCaptureFactor, 228, 1, 500);
TUNE_INT(historyMalusCaptureMax, 1950, 32, 4096);

TUNE_INT(historyDepthBetaOffset, 239, 1, 500);

TUNE_INT(correctionHistoryFactor, 159, 32, 512);

int REDUCTIONS[2][MAX_PLY][MAX_MOVES];
int SEE_MARGIN[MAX_PLY][2];
int LMP_MARGIN[MAX_PLY][2];

void initReductions() {
    REDUCTIONS[0][0][0] = 0;
    REDUCTIONS[1][0][0] = 0;

    for (int i = 1; i < MAX_PLY; i++) {
        for (int j = 1; j < MAX_MOVES; j++) {
            REDUCTIONS[0][i][j] = 1000 * (lmrReductionNoisyBase + log(i) * log(j) / lmrReductionNoisyFactor); // non-quiet
            REDUCTIONS[1][i][j] = 1000 * (lmrReductionQuietBase + log(i) * log(j) / lmrReductionQuietFactor); // quiet
        }
    }

    for (int depth = 0; depth < MAX_PLY; depth++) {
        SEE_MARGIN[depth][0] = seeMarginNoisy * depth * depth; // non-quiet
        SEE_MARGIN[depth][1] = seeMarginQuiet * depth; // quiet

        LMP_MARGIN[depth][0] = lmpMarginWorseningBase + lmpMarginWorseningFactor * std::pow(depth, lmpMarginWorseningPower); // non-improving
        LMP_MARGIN[depth][1] = lmpMarginImprovingBase + lmpMarginImprovingFactor * std::pow(depth, lmpMarginImprovingPower); // improving
    }
}

uint64_t perftInternal(Board* board, NNUE* nnue, int depth) {
    if (depth == 0) return 1;

    BoardStack stack;

    Move moves[MAX_MOVES] = { MOVE_NONE };
    int moveCount = 0;
    generateMoves(board, moves, &moveCount);

    uint64_t nodes = 0;
    for (int i = 0; i < moveCount; i++) {
        Move move = moves[i];

        if (!board->isLegal(move))
            continue;

        board->doMove(&stack, move, board->hashAfter(move), nnue);
        uint64_t subNodes = perftInternal(board, nnue, depth - 1);
        board->undoMove(move, nnue);

        nodes += subNodes;
    }
    return nodes;
}

uint64_t perft(Board* board, int depth) {
    clock_t begin = clock();
    BoardStack stack;
    UCI::nnue.reset(board);

    Move moves[MAX_MOVES] = { MOVE_NONE };
    int moveCount = 0;
    generateMoves(board, moves, &moveCount);

    uint64_t nodes = 0;
    for (int i = 0; i < moveCount; i++) {
        Move move = moves[i];

        if (!board->isLegal(move))
            continue;

        board->doMove(&stack, move, board->hashAfter(move), &UCI::nnue);
        uint64_t subNodes = perftInternal(board, &UCI::nnue, depth - 1);
        board->undoMove(move, &UCI::nnue);

        std::cout << moveToString(move, UCI::Options.chess960.value) << ": " << subNodes << std::endl;

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
    if (value > EVAL_TBWIN_IN_MAX_PLY) value += ply;
    else if (value < -EVAL_TBWIN_IN_MAX_PLY) value -= ply;
    return value;
}

int valueFromTt(int value, int ply) {
    if (value == EVAL_NONE) return EVAL_NONE;
    if (value > EVAL_TBWIN_IN_MAX_PLY) value -= ply;
    else if (value < -EVAL_TBWIN_IN_MAX_PLY) value += ply;
    return value;
}

Eval drawEval(Worker* thread) {
    return 4 - (thread->searchData.nodesSearched & 3);  // Small overhead to avoid 3-fold blindness
}

template <NodeType nodeType>
Eval Worker::qsearch(Board* board, SearchStack* stack, Eval alpha, Eval beta) {
    constexpr bool pvNode = nodeType == PV_NODE;

    if (pvNode)
        stack->pvLength = stack->ply;
    searchData.selDepth = std::max(stack->ply, searchData.selDepth);

    assert(alpha >= -EVAL_INFINITE && alpha < beta && beta <= EVAL_INFINITE);

    if (mainThread && timeOver(searchParameters, &searchData))
        threadPool->stopSearching();

    // Check for stop
    if (stopped || exiting || stack->ply >= MAX_PLY || board->isDraw(stack->ply))
        return (stack->ply >= MAX_PLY && !board->stack->checkers) ? evaluate(board, &nnue) : drawEval(this);

    stack->inCheck = board->stack->checkerCount > 0;

    // TT Lookup
    bool ttHit = false;
    TTEntry* ttEntry = nullptr;
    Move ttMove = MOVE_NONE;
    Eval ttValue = EVAL_NONE;
    Eval ttEval = EVAL_NONE;
    uint8_t ttFlag = TT_NOBOUND;
    bool ttPv = pvNode;

    ttEntry = TT.probe(board->stack->hash, &ttHit);
    if (ttHit) {
        ttMove = ttEntry->getMove();
        ttValue = valueFromTt(ttEntry->getValue(), stack->ply);
        ttEval = ttEntry->getEval();
        ttFlag = ttEntry->getFlag();
        ttPv = ttPv || ttEntry->getTtPv();
    }

    // TT cutoff
    if (!pvNode && ttValue != EVAL_NONE && ((ttFlag == TT_UPPERBOUND && ttValue <= alpha) || (ttFlag == TT_LOWERBOUND && ttValue >= beta) || (ttFlag == TT_EXACTBOUND)))
        return ttValue;

    Move bestMove = MOVE_NONE;
    Eval bestValue, futilityValue, unadjustedEval;

    Eval correctionValue = history.getCorrectionValue(board, stack);
    stack->correctionValue = correctionValue;
    if (board->stack->checkers) {
        stack->staticEval = bestValue = unadjustedEval = futilityValue = -EVAL_INFINITE;
        goto movesLoopQsearch;
    }
    else if (ttHit && ttEval != EVAL_NONE) {
        unadjustedEval = ttEval;
        stack->staticEval = bestValue = history.correctStaticEval(unadjustedEval, correctionValue);

        if (ttValue != EVAL_NONE && ((ttFlag == TT_UPPERBOUND && ttValue < bestValue) || (ttFlag == TT_LOWERBOUND && ttValue > bestValue) || (ttFlag == TT_EXACTBOUND)))
            bestValue = ttValue;
    }
    else {
        unadjustedEval = evaluate(board, &nnue);
        stack->staticEval = bestValue = history.correctStaticEval(unadjustedEval, correctionValue);
        ttEntry->update(board->stack->hash, MOVE_NONE, 0, unadjustedEval, EVAL_NONE, ttPv, TT_NOBOUND);
    }
    futilityValue = stack->staticEval + qsFutilityOffset;

    // Stand pat
    if (bestValue >= beta) {
        if (std::abs(bestValue) < EVAL_TBWIN_IN_MAX_PLY && std::abs(beta) < EVAL_TBWIN_IN_MAX_PLY)
            bestValue = (bestValue + beta) / 2;
        ttEntry->update(board->stack->hash, MOVE_NONE, ttEntry->depth, unadjustedEval, EVAL_NONE, ttPv, TT_NOBOUND);
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

    BoardStack boardStack;

    // Moves loop
    MoveGen movegen(board, &history, stack, ttMove, !board->stack->checkers, 1);
    Move move;
    int moveCount = 0;
    bool playedQuiet = false;
    while ((move = movegen.nextMove()) != MOVE_NONE) {

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

            if (moveTarget(move) != moveTarget((stack - 1)->move) && (moveType(move) != MOVE_PROMOTION) && !board->givesCheck(move) && moveCount > 2)
                continue;
        }

        if (!board->isLegal(move))
            continue;

        uint64_t newHash = board->hashAfter(move);
        TT.prefetch(newHash);
        moveCount++;
        searchData.nodesSearched++;

        Square origin = moveOrigin(move);
        Square target = moveTarget(move);
        stack->capture = capture;
        stack->move = move;
        stack->movedPiece = board->pieces[origin];
        stack->contHist = history.continuationHistory[board->stm][stack->movedPiece][target];
        stack->contCorrHist = &history.continuationCorrectionHistory[board->stm][stack->movedPiece][target];

        playedQuiet |= move != ttMove && !capture;

        board->doMove(&boardStack, move, newHash, &nnue);

        Eval value = -qsearch<nodeType>(board, stack + 1, -beta, -alpha);
        board->undoMove(move, &nnue);
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
        assert(board->stack->checkers && moveCount == 0);
        bestValue = matedIn(stack->ply); // Checkmate
    }

    // Insert into TT
    int flags = bestValue >= beta ? TT_LOWERBOUND : TT_UPPERBOUND;
    ttEntry->update(board->stack->hash, bestMove, 0, unadjustedEval, valueToTT(bestValue, stack->ply), ttPv, flags);

    return bestValue;
}

template <NodeType nt>
Eval Worker::search(Board* board, SearchStack* stack, int depth, Eval alpha, Eval beta, bool cutNode) {
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
    if (!rootNode && alpha < 0 && board->hasUpcomingRepetition(stack->ply)) {
        alpha = drawEval(this);
        if (alpha >= beta)
            return alpha;
    }

    if (depth <= 0) return qsearch<nodeType>(board, stack, alpha, beta);

    if (!rootNode) {

        // Check for time / node limits on main thread
        if (mainThread && timeOver(searchParameters, &searchData))
            threadPool->stopSearching();

        // Check for stop or max depth
        if (stopped || exiting || stack->ply >= MAX_PLY || board->isDraw(stack->ply))
            return (stack->ply >= MAX_PLY && !board->stack->checkers) ? evaluate(board, &nnue) : drawEval(this);

        // Mate distance pruning
        alpha = std::max((int)alpha, (int)matedIn(stack->ply));
        beta = std::min((int)beta, (int)mateIn(stack->ply + 1));
        if (alpha >= beta)
            return alpha;
    }

    // Initialize some stuff
    Move bestMove = MOVE_NONE;
    Move excludedMove = stack->excludedMove;
    Eval bestValue = -EVAL_INFINITE, maxValue = EVAL_INFINITE;
    Eval oldAlpha = alpha;
    bool improving = false, skipQuiets = false, excluded = excludedMove != MOVE_NONE;

    (stack + 1)->killer = MOVE_NONE;
    (stack + 1)->excludedMove = MOVE_NONE;
    stack->inCheck = board->stack->checkerCount > 0;

    // TT Lookup
    bool ttHit = false;
    TTEntry* ttEntry = nullptr;
    Move ttMove = MOVE_NONE;
    Eval ttValue = EVAL_NONE, ttEval = EVAL_NONE;
    int ttDepth = 0;
    uint8_t ttFlag = TT_NOBOUND;
    stack->ttPv = excluded ? stack->ttPv : pvNode;

    if (!excluded) {
        ttEntry = TT.probe(board->stack->hash, &ttHit);
        if (ttHit) {
            ttMove = rootNode && rootMoves[0].value > -EVAL_INFINITE ? rootMoves[0].move : ttEntry->getMove();
            ttValue = valueFromTt(ttEntry->getValue(), stack->ply);
            ttEval = ttEntry->getEval();
            ttDepth = ttEntry->getDepth();
            ttFlag = ttEntry->getFlag();
            stack->ttPv = stack->ttPv || ttEntry->getTtPv();
        }
    }

    // TT cutoff
    if (!pvNode && ttDepth >= depth && ttValue != EVAL_NONE && ((ttFlag == TT_UPPERBOUND && ttValue <= alpha) || (ttFlag == TT_LOWERBOUND && ttValue >= beta) || (ttFlag == TT_EXACTBOUND)))
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
            board->stack->rule50_ply,
            board->stack->castling,
            board->stack->enpassantTarget ? lsb(board->stack->enpassantTarget) : 0,
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
                ttEntry->update(board->stack->hash, MOVE_NONE, MAX_PLY, EVAL_NONE, tbValue, stack->ttPv, tbBound);
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
    if (board->stack->checkers) {
        stack->staticEval = EVAL_NONE;
        goto movesLoop;
    }
    else if (excluded) {
        unadjustedEval = eval = stack->staticEval;
    }
    else if (ttHit) {
        unadjustedEval = ttEval != EVAL_NONE ? ttEval : evaluate(board, &nnue);
        eval = stack->staticEval = history.correctStaticEval(unadjustedEval, correctionValue);

        if (ttValue != EVAL_NONE && ((ttFlag == TT_UPPERBOUND && ttValue < eval) || (ttFlag == TT_LOWERBOUND && ttValue > eval) || (ttFlag == TT_EXACTBOUND)))
            eval = ttValue;
    }
    else {
        unadjustedEval = evaluate(board, &nnue);
        eval = stack->staticEval = history.correctStaticEval(unadjustedEval, correctionValue);

        ttEntry->update(board->stack->hash, MOVE_NONE, 0, unadjustedEval, EVAL_NONE, stack->ttPv, TT_NOBOUND);
    }

    // Improving
    if ((stack - 2)->staticEval != EVAL_NONE) {
        improving = stack->staticEval > (stack - 2)->staticEval;
    }
    else if ((stack - 4)->staticEval != EVAL_NONE) {
        improving = stack->staticEval > (stack - 4)->staticEval;
    }

    // Adjust quiet history based on how much the previous move changed static eval
    if (!excluded && (stack - 1)->movedPiece != Piece::NONE && !(stack - 1)->capture && !(stack - 1)->inCheck && stack->ply > 1) {
        int bonus = std::clamp(staticHistoryFactor * int(stack->staticEval + (stack - 1)->staticEval) / 10, staticHistoryMin, staticHistoryMax) + 50;
        history.updateQuietHistory((stack - 1)->move, flip(board->stm), board, board->stack->previous, bonus);
    }

    // IIR
    if ((!ttHit || ttDepth + 4 < depth) && depth >= iirMinDepth)
        depth--;

    // Post-LMR depth adjustments
    if ((stack - 1)->inLMR) {
        if ((stack - 1)->reduction >= 3000 && stack->staticEval <= -(stack - 1)->staticEval) {
            depth++;
            (stack - 1)->reduction -= 1000;
        }
    }

    // Reverse futility pruning
    if (!rootNode && depth < rfpDepth && std::abs(eval) < EVAL_TBWIN_IN_MAX_PLY && eval - rfpFactor * (depth - (improving && !board->opponentHasGoodCapture())) >= beta)
        return std::min((eval + beta) / 2, EVAL_TBWIN_IN_MAX_PLY - 1);

    // Razoring
    if (!rootNode && depth < razoringDepth && eval + (razoringFactor * depth) < alpha && alpha < EVAL_TBWIN_IN_MAX_PLY) {
        Eval razorValue = qsearch<NON_PV_NODE>(board, stack, alpha, beta);
        if (razorValue <= alpha && razorValue > -EVAL_TBWIN_IN_MAX_PLY)
            return razorValue;
    }

    BoardStack boardStack;

    // Null move pruning
    if (!pvNode
        && eval >= beta
        && eval >= stack->staticEval
        && stack->staticEval + nmpEvalDepth * depth - nmpEvalBase >= beta
        && std::abs(beta) < EVAL_TBWIN_IN_MAX_PLY
        && !excluded
        && (stack - 1)->movedPiece != Piece::NONE
        && depth >= 3
        && stack->ply >= searchData.nmpPlies
        && board->hasNonPawns()
        ) {
        stack->capture = false;
        stack->move = MOVE_NULL;
        stack->movedPiece = Piece::NONE;
        stack->contHist = history.continuationHistory[board->stm][0][0];
        stack->contCorrHist = &history.continuationCorrectionHistory[board->stm][0][0];
        int R = nmpRedBase + depth / nmpDepthDiv + std::min((eval - beta) / nmpDivisor, nmpMin);

        board->doNullMove(&boardStack);
        Eval nullValue = -search<NON_PV_NODE>(board, stack + 1, depth - R, -beta, -beta + 1, !cutNode);
        board->undoNullMove();

        if (stopped || exiting)
            return 0;

        if (nullValue >= beta) {
            if (nullValue >= EVAL_TBWIN_IN_MAX_PLY)
                nullValue = beta;

            if (searchData.nmpPlies || depth < 15)
                return nullValue;

            searchData.nmpPlies = stack->ply + (depth - R) * 2 / 3;
            Eval verificationValue = search<NON_PV_NODE>(board, stack, depth - R, beta - 1, beta, false);
            searchData.nmpPlies = 0;

            if (verificationValue >= beta)
                return nullValue;
        }
    }

    // ProbCut
    probCutBeta = std::min(beta + probCutBetaOffset, EVAL_TBWIN_IN_MAX_PLY - 1);
    if (!pvNode
        && !excluded
        && depth > probCutDepth
        && std::abs(beta) < EVAL_TBWIN_IN_MAX_PLY - 1
        && !(ttDepth >= depth - 3 && ttValue != EVAL_NONE && ttValue < probCutBeta)) {

        assert(probCutBeta > beta);
        assert(probCutBeta < EVAL_TBWIN_IN_MAX_PLY);

        Move probcutTtMove = ttMove != MOVE_NONE && board->isPseudoLegal(ttMove) && SEE(board, ttMove, probCutBeta - stack->staticEval) ? ttMove : MOVE_NONE;
        MoveGen movegen(board, &history, stack, probcutTtMove, probCutBeta - stack->staticEval, depth);
        Move move;
        while ((move = movegen.nextMove()) != MOVE_NONE) {
            if (move == excludedMove || !board->isLegal(move))
                continue;

            uint64_t newHash = board->hashAfter(move);
            TT.prefetch(newHash);

            Square origin = moveOrigin(move);
            Square target = moveTarget(move);
            stack->capture = true;
            stack->move = move;
            stack->movedPiece = board->pieces[origin];
            stack->contHist = history.continuationHistory[board->stm][stack->movedPiece][target];
            stack->contCorrHist = &history.continuationCorrectionHistory[board->stm][stack->movedPiece][target];
            board->doMove(&boardStack, move, newHash, &nnue);

            Eval value = -qsearch<NON_PV_NODE>(board, stack + 1, -probCutBeta, -probCutBeta + 1);

            if (value >= probCutBeta)
                value = -search<NON_PV_NODE>(board, stack + 1, depth - 4, -probCutBeta, -probCutBeta + 1, !cutNode);

            board->undoMove(move, &nnue);

            if (stopped || exiting)
                return 0;

            if (value >= probCutBeta) {
                value = std::min(value, EVAL_TBWIN_IN_MAX_PLY - 1);
                ttEntry->update(board->stack->hash, move, depth - 3, unadjustedEval, valueToTT(value, stack->ply), stack->ttPv, TT_LOWERBOUND);
                return value;
            }
        }

    }

    assert(board->stack);

    // IIR 2: Electric boolagoo
    if (!ttHit && depth >= iirMinDepth && pvNode)
        depth--;

movesLoop:

    if (stopped || exiting)
        return 0;

    Move quietMoves[32];
    Move captureMoves[32];
    int quietSearchCount[32];
    int captureSearchCount[32];
    int quietMoveCount = 0;
    int captureMoveCount = 0;

    // Moves loop
    MoveGen movegen(board, &history, stack, ttMove, depth);
    Move move;
    int moveCount = 0;
    while ((move = movegen.nextMove()) != MOVE_NONE) {

        if (move == excludedMove)
            continue;
        if (rootNode && std::find(excludedRootMoves.begin(), excludedRootMoves.end(), move) != excludedRootMoves.end())
            continue;

        bool capture = board->isCapture(move);
        if (!capture && skipQuiets)
            continue;

        if (!board->isLegal(move))
            continue;

        uint64_t nodesBeforeMove = searchData.nodesSearched;
        int moveHistory = history.getHistory(board, board->stack, stack, move, capture);

        if (!rootNode
            && bestValue > -EVAL_TBWIN_IN_MAX_PLY
            && board->hasNonPawns()
            ) {

            int lmrDepth = std::max(0, depth - REDUCTIONS[!capture][depth][moveCount] / 1000 - !improving + moveHistory / (capture ? earlyLmrHistoryFactorCapture : earlyLmrHistoryFactorQuiet));

            if (!pvNode && !skipQuiets) {

                // Movecount pruning (LMP)
                if (moveCount >= LMP_MARGIN[depth][improving]) {
                    skipQuiets = true;
                }

                // Futility pruning
                if (!capture && lmrDepth < fpDepth && eval + fpBase + fpFactor * lmrDepth <= alpha)
                    skipQuiets = true;
            }

            // Futility pruning for captures
            if (!pvNode && capture && moveType(move) != MOVE_PROMOTION) {
                Piece capturedPiece = moveType(move) == MOVE_ENPASSANT ? Piece::PAWN : board->pieces[moveTarget(move)];
                if (lmrDepth < fpCaptDepth && eval + fpCaptBase + PIECE_VALUES[capturedPiece] + fpCaptFactor * lmrDepth <= alpha)
                    continue;
            }

            // History pruning
            int hpFactor = capture ? historyPruningFactorCapture : historyPruningFactorQuiet;
            if (!pvNode && lmrDepth < historyPruningDepth && moveHistory < hpFactor * depth)
                continue;

            // SEE Pruning
            if (!SEE(board, move, (2 + pvNode) * SEE_MARGIN[!capture ? lmrDepth : depth][!capture] / 2))
                continue;

        }

        // Extensions
        bool doExtensions = !rootNode && stack->ply < searchData.rootDepth * 2;
        int extension = 0;
        if (doExtensions
            && depth >= 6
            && move == ttMove
            && !excluded
            && (ttFlag & TT_LOWERBOUND)
            && std::abs(ttValue) < EVAL_TBWIN_IN_MAX_PLY
            && ttDepth >= depth - 3
            ) {
            Eval singularBeta = ttValue - (1 + (stack->ttPv && !pvNode)) * depth;
            int singularDepth = (depth - 1) / 2;

            bool currTtPv = stack->ttPv;
            stack->excludedMove = move;
            Eval singularValue = search<NON_PV_NODE>(board, stack, singularDepth, singularBeta - 1, singularBeta, cutNode);
            stack->excludedMove = MOVE_NONE;
            stack->ttPv = currTtPv;

            if (stopped || exiting)
                return 0;

            if (singularValue < singularBeta) {
                // This move is singular and we should investigate it further
                extension = 1;
                if (!pvNode && singularValue + doubleExtensionMargin < singularBeta) {
                    extension = 2;
                    depth += depth < doubleExtensionDepthIncrease;
                    if (!board->isCapture(move) && singularValue + tripleExtensionMargin < singularBeta)
                        extension = 3;
                }
            }
            // Multicut: If we beat beta, that means there's likely more moves that beat beta and we can skip this node
            else if (singularBeta >= beta) {
                Eval value = std::min(singularBeta, EVAL_TBWIN_IN_MAX_PLY - 1);
                ttEntry->update(board->stack->hash, ttMove, singularDepth, unadjustedEval, value, stack->ttPv, TT_LOWERBOUND);
                return value;
            }
            // We didn't prove singularity and an excluded search couldn't beat beta, but if the ttValue can we still reduce the depth
            else if (ttValue >= beta)
                extension = -3;
            // We didn't prove singularity and an excluded search couldn't beat beta, but we are expected to fail low, so reduce
            else if (cutNode)
                extension = -2;
            else if (ttValue <= alpha)
                extension = -1;
        }

        uint64_t newHash = board->hashAfter(move);
        TT.prefetch(newHash);

        if (!capture) {
            if (quietMoveCount < 32) {
                quietMoves[quietMoveCount] = move;
                quietSearchCount[quietMoveCount] = 0;
            }
        }
        else {
            if (captureMoveCount < 32) {
                captureMoves[captureMoveCount] = move;
                captureSearchCount[captureMoveCount] = 0;
            }
        }

        // Some setup stuff
        Square origin = moveOrigin(move);
        Square target = moveTarget(move);
        stack->capture = capture;
        stack->move = move;
        stack->movedPiece = board->pieces[origin];
        stack->contHist = history.continuationHistory[board->stm][stack->movedPiece][target];
        stack->contCorrHist = &history.continuationCorrectionHistory[board->stm][stack->movedPiece][target];

        moveCount++;
        searchData.nodesSearched++;
        board->doMove(&boardStack, move, newHash, &nnue);

        Eval value = 0;
        int newDepth = depth - 1 + extension;

        // Very basic LMR: Late moves are being searched with less depth
        // Check if the move can exceed alpha
        if (moveCount > lmrMcBase + lmrMcPv * rootNode - (ttMove != MOVE_NONE) && depth >= lmrMinDepth && (!capture || !stack->ttPv || cutNode)) {
            int reduction = REDUCTIONS[!capture][depth][moveCount];

            if (board->stack->checkers)
                reduction -= lmrCheck;

            if (!stack->ttPv)
                reduction += lmrTtPv;

            if (cutNode)
                reduction += lmrCutnode;
            
            if (stack->ttPv && ttHit && ttValue <= alpha)
                reduction += 1000;

            reduction -= std::abs(correctionValue / lmrCorrection);

            if (capture)
                reduction -= moveHistory * std::abs(moveHistory) / lmrHistoryFactorCapture;
            else
                reduction -= 1000 * moveHistory / lmrHistoryFactorQuiet;

            int reducedDepth = std::clamp(newDepth - reduction / 1000, 1, newDepth + pvNode);
            stack->reduction = reduction;
            stack->inLMR = true;
            value = -search<NON_PV_NODE>(board, stack + 1, reducedDepth, -(alpha + 1), -alpha, true);
            stack->inLMR = false;
            reducedDepth = std::clamp(newDepth - stack->reduction / 1000, 1, newDepth + pvNode);
            stack->reduction = 0;

            if (capture && captureMoveCount < 32)
                captureSearchCount[captureMoveCount]++;
            else if (!capture && quietMoveCount < 32)
                quietSearchCount[quietMoveCount]++;

            bool doShallowerSearch = !rootNode && value < bestValue + newDepth;
            bool doDeeperSearch = value > (bestValue + lmrDeeperBase + lmrDeeperFactor * newDepth);
            newDepth += doDeeperSearch - doShallowerSearch;

            if (value > alpha && reducedDepth < newDepth && !(ttValue < alpha && ttDepth - 4 >= newDepth && (ttFlag & TT_UPPERBOUND))) {
                value = -search<NON_PV_NODE>(board, stack + 1, newDepth, -(alpha + 1), -alpha, !cutNode);

                if (capture && captureMoveCount < 32)
                    captureSearchCount[captureMoveCount]++;
                else if (!capture && quietMoveCount < 32)
                    quietSearchCount[quietMoveCount]++;

                if (!capture) {
                    int bonus = std::min(lmrPassBonusBase + lmrPassBonusFactor * (value > alpha ? depth : reducedDepth), lmrPassBonusMax);
                    history.updateContinuationHistory(stack, flip(board->stm), stack->movedPiece, move, bonus);
                }
            }
        }
        else if (!pvNode || moveCount > 1) {
            value = -search<NON_PV_NODE>(board, stack + 1, newDepth, -(alpha + 1), -alpha, !cutNode);

            if (capture && captureMoveCount < 32)
                captureSearchCount[captureMoveCount]++;
            else if (!capture && quietMoveCount < 32)
                quietSearchCount[quietMoveCount]++;
        }

        // PV moves will be researched at full depth if good enough
        if (pvNode && (moveCount == 1 || value > alpha)) {
            value = -search<PV_NODE>(board, stack + 1, newDepth, -beta, -alpha, false);

            if (capture && captureMoveCount < 32)
                captureSearchCount[captureMoveCount]++;
            else if (!capture && quietMoveCount < 32)
                quietSearchCount[quietMoveCount]++;
        }

        board->undoMove(move, &nnue);
        assert(value > -EVAL_INFINITE && value < EVAL_INFINITE);

        if (capture && captureMoveCount < 32)
            captureMoveCount++;
        else if (!capture && quietMoveCount < 32)
            quietMoveCount++;

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

                    int historyUpdateDepth = depth + (eval <= alpha) + (value - historyDepthBetaOffset > beta);

                    int quietBonus = std::min(historyBonusQuietBase + historyBonusQuietFactor * historyUpdateDepth, historyBonusQuietMax);
                    int quietMalus = std::min(historyMalusQuietBase + historyMalusQuietFactor * historyUpdateDepth, historyMalusQuietMax);
                    int captureBonus = std::min(historyBonusCaptureBase + historyBonusCaptureFactor * historyUpdateDepth, historyBonusCaptureMax);
                    int captureMalus = std::min(historyMalusCaptureBase + historyMalusCaptureFactor * historyUpdateDepth, historyMalusCaptureMax);

                    if (!capture) {
                        // Update quiet killer
                        stack->killer = move;

                        // Update counter move
                        if (stack->ply > 0)
                            history.setCounterMove((stack - 1)->move, move);

                        history.updateQuietHistories(board, board->stack, stack, move, quietSearchCount[quietMoveCount - 1], quietBonus, quietMalus, quietMoves, quietSearchCount, quietMoveCount);
                    }
                    if (captureMoveCount > 0)
                        history.updateCaptureHistory(board, move, captureSearchCount[captureMoveCount - 1], captureBonus, captureMalus, captureMoves, captureSearchCount, captureMoveCount);
                    break;
                }

                if (depth > 4 && depth < 10 && beta < EVAL_TBWIN_IN_MAX_PLY && value > -EVAL_TBWIN_IN_MAX_PLY)
                    depth--;
            }
        }

    }

    if (stopped || exiting)
        return 0;

    if (!pvNode && bestValue >= beta && std::abs(bestValue) < EVAL_TBWIN_IN_MAX_PLY && std::abs(beta) < EVAL_TBWIN_IN_MAX_PLY && std::abs(alpha) < EVAL_TBWIN_IN_MAX_PLY)
        bestValue = (bestValue * depth + beta) / (depth + 1);

    if (moveCount == 0) {
        if (board->stack->checkers && excluded)
            return -EVAL_INFINITE;
        // Mate / Stalemate
        bestValue = board->stack->checkers ? matedIn(stack->ply) : 0;
    }

    if (pvNode)
        bestValue = std::min(bestValue, maxValue);

    // Insert into TT
    bool failLow = alpha == oldAlpha;
    bool failHigh = bestValue >= beta;
    int flags = failHigh ? TT_LOWERBOUND : !failLow ? TT_EXACTBOUND : TT_UPPERBOUND;
    if (!excluded)
        ttEntry->update(board->stack->hash, bestMove, depth, unadjustedEval, valueToTT(bestValue, stack->ply), stack->ttPv, flags);

    // Adjust correction history
    if (!board->stack->checkers && (bestMove == MOVE_NONE || !board->isCapture(bestMove)) && (!failHigh || bestValue > stack->staticEval) && (!failLow || bestValue <= stack->staticEval)) {
        int bonus = std::clamp((int)(bestValue - stack->staticEval) * depth * correctionHistoryFactor / 1024, -CORRECTION_HISTORY_LIMIT / 4, CORRECTION_HISTORY_LIMIT / 4);
        history.updateCorrectionHistory(board, stack, bonus);
    }

    assert(bestValue > -EVAL_INFINITE && bestValue < EVAL_INFINITE);

    return bestValue;
}

Move tbProbeMoveRoot(unsigned result) {
    Square origin = TB_GET_FROM(result);
    Square target = TB_GET_TO(result);
    int promotion = TB_GET_PROMOTES(result);

    if (promotion)
        return createMove(origin, target) | MOVE_PROMOTION | (PROMOTION_PIECE[promotion - 1] << 14);

    if (TB_GET_EP(result))
        return createMove(origin, target) | MOVE_ENPASSANT;

    return createMove(origin, target);
}

void Worker::tsearch() {
    if (TUNE_ENABLED)
        initReductions();

    nnue.reset(&rootBoard);

    Move bestTbMove = MOVE_NONE;
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
            rootBoard.stack->rule50_ply,
            rootBoard.stack->castling,
            rootBoard.stack->enpassantTarget ? lsb(rootBoard.stack->enpassantTarget) : 0,
            rootBoard.stm == Color::WHITE,
            nullptr
        );

        if (result != TB_RESULT_FAILED)
            bestTbMove = tbProbeMoveRoot(result);
    }

    searchData.nodesSearched = 0;
    searchData.tbHits = 0;
    if (mainThread)
        initTimeManagement(&rootBoard, searchParameters, &searchData);

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
            std::cout << "bestmove " << moveToString(bestTbMove != MOVE_NONE && std::abs(bestThread->rootMoves[0].value) < EVAL_MATE_IN_MAX_PLY ? bestTbMove : bestThread->rootMoves[0].move, UCI::Options.chess960.value) << std::endl;
        }
        else {
            std::cout << "bestmove " << moveToString(bestThread->rootMoves[0].move, UCI::Options.chess960.value) << " ponder " << moveToString(bestThread->rootMoves[0].pv[1], UCI::Options.chess960.value) << std::endl;
        }
    }
}

void Worker::iterativeDeepening() {
    int multiPvCount = 0;
    {
        Move moves[MAX_MOVES] = { MOVE_NONE };
        int m = 0;
        generateMoves(&rootBoard, moves, &m);
        for (int i = 0; i < m; i++) {
            if (rootBoard.isLegal(moves[i])) {
                multiPvCount++;

                RootMove rootMove = {};
                rootMove.move = moves[i];
                rootMoves.push_back(rootMove);
            }
        }
    }
    multiPvCount = std::min(multiPvCount, UCI::Options.multiPV.value);

    int maxDepth = searchParameters->depth == 0 ? MAX_PLY - 1 : searchParameters->depth;

    Eval previousValue = EVAL_NONE;
    Move previousMove = MOVE_NONE;

    int bestMoveStability = 0;

    constexpr int STACK_OVERHEAD = 6;
    SearchStack stackList[MAX_PLY + STACK_OVERHEAD + 2];
    SearchStack* stack = &stackList[STACK_OVERHEAD];

    rootMoveNodes.clear();

    for (int depth = 1; depth <= maxDepth; depth++) {
        excludedRootMoves.clear();
        for (int rootMoveIdx = 0; rootMoveIdx < multiPvCount; rootMoveIdx++) {

            for (int i = 0; i < MAX_PLY + STACK_OVERHEAD + 2; i++) {
                stackList[i].pvLength = 0;
                stackList[i].ply = i - STACK_OVERHEAD;
                stackList[i].staticEval = EVAL_NONE;
                stackList[i].excludedMove = MOVE_NONE;
                stackList[i].killer = MOVE_NONE;
                stackList[i].movedPiece = Piece::NONE;
                stackList[i].move = MOVE_NONE;
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
                value = search<ROOT_NODE>(&rootBoard, stack, searchDepth, alpha, beta, false);

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

            if (timeOverDepthCleared(searchParameters, &searchData, tmAdjustment)) {
                threadPool->stopSearching();
                return;
            }
        }

        previousMove = rootMoves[0].move;
        previousValue = rootMoves[0].value;
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
            std::cout << moveToString(move, UCI::Options.chess960.value) << " ";
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
    size_t bestMoveIdx = 0;

    if (threadPool->workers.size() > 1 && UCI::Options.multiPV.value == 1) {
        std::map<Move, int64_t> votes;
        Eval minValue = EVAL_INFINITE;

        for (auto& worker : threadPool->workers) {
            for (auto& rm : worker->rootMoves) {
                if (rm.value == -EVAL_INFINITE)
                    break;
                minValue = std::min(minValue, rm.value);
            }
        }
        minValue++;

        for (auto& worker : threadPool->workers) {
            for (auto& rm : worker->rootMoves) {
                if (rm.value == -EVAL_INFINITE)
                    break;
                // Votes weighted by depth and difference to the minimum value
                votes[rm.move] += (rm.value - minValue + 10) * std::max(1, rm.depth - (worker->rootMoves[0].depth - rm.depth));
            }
        }

        for (auto& worker : threadPool->workers) {
            Worker* thread = worker.get();
            for (size_t rmi = 0; rmi < thread->rootMoves.size(); rmi++) {
                RootMove& rm = thread->rootMoves[rmi];
                if (rm.value == -EVAL_INFINITE)
                    break;

                Eval thValue = rm.value;
                Eval bestValue = bestThread->rootMoves[bestMoveIdx].value;
                Move thMove = rm.move;
                Move bestMove = bestThread->rootMoves[bestMoveIdx].move;

                // In case of checkmate, take the shorter mate / longer getting mated
                if (std::abs(bestValue) >= EVAL_TBWIN_IN_MAX_PLY) {
                    if (thValue > bestValue) {
                        bestThread = thread;
                        bestMoveIdx = rmi;
                    }
                }
                // We have found a mate, take it without voting
                else if (thValue >= EVAL_TBWIN_IN_MAX_PLY) {
                    bestThread = thread;
                    bestMoveIdx = rmi;
                }
                // No mate found by any thread so far, take the thread with more votes
                else if (votes[thMove] > votes[bestMove]) {
                    bestThread = thread;
                    bestMoveIdx = rmi;
                }
                // In case of same move, choose the thread with the highest score
                else if (thMove == bestMove && std::abs(thValue) > std::abs(bestValue) && rm.pv.size() > 2) {
                    bestThread = thread;
                    bestMoveIdx = rmi;
                }
            }
        }
    }

    if (bestMoveIdx)
        std::swap(bestThread->rootMoves[0], bestThread->rootMoves[bestMoveIdx]);

    return bestThread;
}

void Worker::tdatagen() {
    nnue.reset(&rootBoard);

    searchData.nodesSearched = 0;
    searchData.tbHits = 0;
    initTimeManagement(&rootBoard, searchParameters, &searchData);
    {
        Move moves[MAX_MOVES] = { MOVE_NONE };
        int m = 0;
        generateMoves(&rootBoard, moves, &m);
        for (int i = 0; i < m; i++) {
            if (rootBoard.isLegal(moves[i])) {
                RootMove rootMove = {};
                rootMove.move = moves[i];
                rootMoves.push_back(rootMove);
            }
        }
    }

    Eval previousValue = EVAL_NONE;

    constexpr int STACK_OVERHEAD = 6;
    SearchStack stackList[MAX_PLY + STACK_OVERHEAD + 2];
    SearchStack* stack = &stackList[STACK_OVERHEAD];

    rootMoveNodes.clear();

    int maxDepth = searchParameters->depth ? searchParameters->depth : MAX_PLY - 1;

    for (int depth = 1; depth <= maxDepth; depth++) {
        for (int i = 0; i < MAX_PLY + STACK_OVERHEAD + 2; i++) {
            stackList[i].pvLength = 0;
            stackList[i].ply = i - STACK_OVERHEAD;
            stackList[i].staticEval = EVAL_NONE;
            stackList[i].excludedMove = MOVE_NONE;
            stackList[i].killer = MOVE_NONE;
            stackList[i].movedPiece = Piece::NONE;
            stackList[i].move = MOVE_NONE;
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
            value = search<ROOT_NODE>(&rootBoard, stack, searchDepth, alpha, beta, false);

            sortRootMoves();

            // Stop if we need to
            if (stopped || exiting || searchData.nodesSearched >= searchParameters->nodes)
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

        if (stopped || exiting || searchData.nodesSearched >= searchParameters->nodes)
            break;

        previousValue = rootMoves[0].value;
    }

    sortRootMoves();
    printUCI(this);

    std::cout << "bestmove " << moveToString(rootMoves[0].move, UCI::Options.chess960.value) << std::endl;
}
