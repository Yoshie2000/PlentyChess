#include <cstdint>
#include <algorithm>
#include <inttypes.h>
#include <cassert>
#include <chrono>
#include <cmath>
#include <thread>
#include <map>

#include "search.h"
#include "board.h"
#include "move.h"
#include "evaluation.h"
#include "thread.h"
#include "tt.h"
#include "time.h"
#include "spsa.h"
#include "nnue.h"
#include "spsa.h"
#include "uci.h"

// Time management
TUNE_FLOAT(tmInitialAdjustment, 1.0151265117716062f, 0.5f, 1.5f);
TUNE_INT(tmBestMoveStabilityMax, 18, 1, 100);
TUNE_FLOAT(tmBestMoveStabilityBase, 1.411706462717875f, 0.75f, 2.5f);
TUNE_FLOAT(tmBestMoveStabilityFactor, 0.04707638950749792f, 0.001f, 0.1f);
TUNE_FLOAT(tmEvalDiffBase, 0.9010618659177686f, 0.5f, 1.5f);
TUNE_FLOAT(tmEvalDiffFactor, 0.008242014020091608f, 0.001f, 0.1f);
TUNE_INT(tmEvalDiffMin, -16, -250, 50);
TUNE_INT(tmEvalDiffMax, 58, -50, 250);
TUNE_FLOAT(tmNodesBase, 1.7139889483273534f, 0.5f, 5.0f);
TUNE_FLOAT(tmNodesFactor, 0.8519440543202387f, 0.1f, 10.0f);

// Aspiration windows
TUNE_INT_DISABLED(aspirationWindowMinDepth, 4, 2, 20);
TUNE_INT_DISABLED(aspirationWindowDelta, 17, 1, 200); // 15
TUNE_INT_DISABLED(aspirationWindowMaxFailHighs, 3, 0, 20);
TUNE_FLOAT_DISABLED(aspirationWindowDeltaFactor, 1.789268600882079, 1.0f, 10.0f); // 1.8452

// Reduction / Margin tables
TUNE_FLOAT_DISABLED(lmrReductionNoisyBase, -0.5507368054141233, -5.00f, 5.00f); // -0.1782
TUNE_FLOAT_DISABLED(lmrReductionNoisyFactor, 2.9945957027805936, 1.00f, 10.00f); // 3.2148
TUNE_FLOAT_DISABLED(lmrReductionQuietBase, 0.7842110713340991, -5.00f, 5.00f); // 1.0577
TUNE_FLOAT_DISABLED(lmrReductionQuietFactor, 2.8063316175892044, 1.00f, 10.00f); // 2.6368

TUNE_FLOAT_DISABLED(seeMarginNoisy, -26.14630196595127, -100.0f, -1.0f); // -24.2315
TUNE_FLOAT_DISABLED(seeMarginQuiet, -75.36717662656213, -200.0f, -1.0f); // -71.7102
TUNE_FLOAT_DISABLED(lmpMarginWorseningBase, 1.4194200452372927, -2.5f, 10.0f); // 2.1576
TUNE_FLOAT_DISABLED(lmpMarginWorseningFactor, 0.4609450270167632, 0.05f, 2.5f); // 0.5731
TUNE_FLOAT_DISABLED(lmpMarginWorseningPower, 1.785778252619638, 0.5f, 5.0f); // 1.7130
TUNE_FLOAT_DISABLED(lmpMarginImprovingBase, 3.096315467954453, -2.5f, 10.0f); // 2.9163
TUNE_FLOAT_DISABLED(lmpMarginImprovingFactor, 1.0584246020918444, 0.05f, 2.5f); // 1.1387
TUNE_FLOAT_DISABLED(lmpMarginImprovingPower, 1.8914578112950748, 0.5f, 5.0f); // 1.8248

// Search values
TUNE_INT_DISABLED(qsFutilityOffset, 49, 0, 250); // 49

// Pre-search pruning
TUNE_INT_DISABLED(iirMinDepth, 4, 1, 20);

TUNE_INT_DISABLED(rfpDepth, 8, 2, 20);
TUNE_INT_DISABLED(rfpFactor, 84, 1, 250); // 90

TUNE_INT_DISABLED(razoringDepth, 5, 2, 20);
TUNE_INT_DISABLED(razoringFactor, 310, 1, 1000); // 265

TUNE_INT_DISABLED(nmpRedBase, 3, 1, 5);
TUNE_INT_DISABLED(nmpDepthDiv, 3, 1, 6);
TUNE_INT_DISABLED(nmpMin, 3, 1, 10);
TUNE_INT_DISABLED(nmpDivisor, 183, 10, 1000); // 201

// In-search pruning
TUNE_INT_DISABLED(fpDepth, 11, 1, 20);
TUNE_INT_DISABLED(fpBase, 240, 0, 1000); // 217
TUNE_INT_DISABLED(fpFactor, 132, 1, 500); // 148

TUNE_INT_DISABLED(historyPruningDepth, 4, 1, 15);
TUNE_INT_DISABLED(historyPruningFactor, -2096, -8192, -128); // -2353

TUNE_INT_DISABLED(doubleExtensionMargin, 15, 1, 100); // 16
TUNE_INT_DISABLED(doubleExtensionLimit, 12, 1, 100); // 11

TUNE_INT_DISABLED(seeDepth, 9, 2, 20);

TUNE_INT_DISABLED(lmrMcBase, 2, 1, 10);
TUNE_INT_DISABLED(lmrMcPv, 2, 1, 10);
TUNE_INT_DISABLED(lmrMinDepth, 3, 1, 10);

TUNE_INT_DISABLED(lmrHistoryFactorQuiet, 13199, 128, 32768); // 14654
TUNE_INT_DISABLED(lmrHistoryFactorCapture, 13199, 128, 32768); // 12679
TUNE_INT_DISABLED(lmrDeeperBase, 43, 1, 200);
TUNE_INT_DISABLED(lmrDeeperFactor, 2, 0, 10);

TUNE_INT_DISABLED(lmrPassBonusBase, 0, -1000, 1000); // 25
TUNE_INT_DISABLED(lmrPassBonusFactor, 110, 1, 1000); // 134
TUNE_INT_DISABLED(lmrPassBonusMax, 1140, 32, 8192); // 1033

TUNE_INT_DISABLED(historyBonusBase, 0, -1000, 1000); // -53
TUNE_INT_DISABLED(historyBonusFactor, 160, 1, 32); // 187
TUNE_INT_DISABLED(historyBonusMax, 1757, 32, 8192); // 1721

int REDUCTIONS[2][MAX_PLY][MAX_MOVES];
int SEE_MARGIN[MAX_PLY][2];
int LMP_MARGIN[MAX_PLY][2];

void initReductions() {
    REDUCTIONS[0][0][0] = 0;
    REDUCTIONS[1][0][0] = 0;

    for (int i = 1; i < MAX_PLY; i++) {
        for (int j = 1; j < MAX_MOVES; j++) {
            REDUCTIONS[0][i][j] = lmrReductionNoisyBase + log(i) * log(j) / lmrReductionNoisyFactor; // non-quiet
            REDUCTIONS[1][i][j] = lmrReductionQuietBase + log(i) * log(j) / lmrReductionQuietFactor; // quiet
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
    if (depth == 0) return C64(1);

    BoardStack stack;

    Move moves[MAX_MOVES] = { MOVE_NONE };
    int moveCount = 0;
    generateMoves(board, moves, &moveCount);

    uint64_t nodes = 0;
    for (int i = 0; i < moveCount; i++) {
        Move move = moves[i];

        if (!isLegal(board, move))
            continue;

        doMove(board, &stack, move, hashAfter(board, move), nnue);
        uint64_t subNodes = perftInternal(board, nnue, depth - 1);
        undoMove(board, move, nnue);

        nodes += subNodes;
    }
    return nodes;
}

uint64_t perft(Board* board, int depth) {
    clock_t begin = clock();
    BoardStack stack;
    NNUE nnue;
    resetAccumulators(board, &nnue);

    Move moves[MAX_MOVES] = { MOVE_NONE };
    int moveCount = 0;
    generateMoves(board, moves, &moveCount);

    uint64_t nodes = 0;
    for (int i = 0; i < moveCount; i++) {
        Move move = moves[i];

        if (!isLegal(board, move))
            continue;

        doMove(board, &stack, move, hashAfter(board, move), &nnue);
        uint64_t subNodes = perftInternal(board, &nnue, depth - 1);
        undoMove(board, move, &nnue);

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
    if (value > EVAL_MATE_IN_MAX_PLY) value += ply;
    else if (value < -EVAL_MATE_IN_MAX_PLY) value -= ply;
    return value;
}

int valueFromTt(int value, int ply) {
    if (value == EVAL_NONE) return EVAL_NONE;
    if (value > EVAL_MATE_IN_MAX_PLY) value -= ply;
    else if (value < -EVAL_MATE_IN_MAX_PLY) value += ply;
    return value;
}

Eval drawEval(Thread* thread) {
    return 4 - (thread->searchData.nodesSearched & 3);  // Small overhead to avoid 3-fold blindness
}

template <NodeType nodeType>
Eval qsearch(Board* board, Thread* thread, SearchStack* stack, Eval alpha, Eval beta) {
    constexpr bool pvNode = nodeType == PV_NODE;

    if (pvNode)
        stack->pvLength = stack->ply;
    thread->searchData.selDepth = std::max(stack->ply, thread->searchData.selDepth);

    assert(alpha >= -EVAL_INFINITE && alpha < beta && beta <= EVAL_INFINITE);

    if (thread->mainThread && timeOver(thread->searchParameters, &thread->searchData))
        thread->threadPool->stopSearching();

    // Check for stop
    if (!thread->searching || thread->exiting || stack->ply >= MAX_PLY || isDraw(board))
        return (stack->ply >= MAX_PLY && !board->stack->checkers) ? evaluate(board, &thread->nnue) : drawEval(thread);

    BoardStack boardStack;
    Move bestMove = MOVE_NONE;
    Eval bestValue, futilityValue, unadjustedEval;
    Eval oldAlpha = alpha;

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
        ttMove = ttEntry->bestMove;
        ttValue = valueFromTt(ttEntry->value, stack->ply);
        ttEval = ttEntry->eval;
        ttFlag = ttEntry->flags & 0x3;
        ttPv = ttPv || ttEntry->flags & 0x4;
    }

    // TT cutoff
    if (!pvNode && ttValue != EVAL_NONE && ((ttFlag == TT_UPPERBOUND && ttValue <= alpha) || (ttFlag == TT_LOWERBOUND && ttValue >= beta) || (ttFlag == TT_EXACTBOUND)))
        return ttValue;

    if (board->stack->checkers) {
        stack->staticEval = bestValue = unadjustedEval = futilityValue = -EVAL_INFINITE;
        goto movesLoopQsearch;
    }
    else if (ttHit && ttEval != EVAL_NONE) {
        unadjustedEval = ttEval;
        stack->staticEval = bestValue = thread->history.correctStaticEval(unadjustedEval, board);
    }
    else {
        unadjustedEval = evaluate(board, &thread->nnue);
        stack->staticEval = bestValue = thread->history.correctStaticEval(unadjustedEval, board);
        ttEntry->update(board->stack->hash, MOVE_NONE, 0, unadjustedEval, EVAL_NONE, ttPv, TT_NOBOUND);
    }
    futilityValue = stack->staticEval + qsFutilityOffset;

    // Stand pat
    if (bestValue >= beta)
        return beta;
    if (alpha < bestValue)
        alpha = bestValue;

movesLoopQsearch:
    // Mate distance pruning
    alpha = std::max((int)alpha, (int)matedIn(stack->ply));
    beta = std::min((int)beta, (int)mateIn(stack->ply + 1));
    if (alpha >= beta)
        return alpha;

    // Moves loop
    MoveGen movegen(board, &thread->history, stack, ttMove, !board->stack->checkers, 1);
    Move move;
    int moveCount = 0;
    while ((move = movegen.nextMove()) != MOVE_NONE) {

        if (futilityValue > -EVAL_INFINITE) { // Only prune when not in check
            if (bestValue >= -EVAL_MATE_IN_MAX_PLY
                && futilityValue <= alpha
                && !SEE(board, move, 1)
                ) {
                bestValue = std::max(bestValue, futilityValue);
                continue;
            }

            if (!SEE(board, move, -107))
                break;
        }

        if (!isLegal(board, move))
            continue;

        uint64_t newHash = hashAfter(board, move);
        TT.prefetch(newHash);
        moveCount++;
        thread->searchData.nodesSearched++;
        doMove(board, &boardStack, move, newHash, &thread->nnue);

        Eval value = -qsearch<nodeType>(board, thread, stack + 1, -beta, -alpha);
        undoMove(board, move, &thread->nnue);
        assert(value > -EVAL_INFINITE && value < EVAL_INFINITE);

        if (!thread->searching || thread->exiting)
            return 0;

        if (value > bestValue) {
            bestValue = value;
            bestMove = move;

            if (value > alpha) {
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
    int flags = bestValue >= beta ? TT_LOWERBOUND : alpha != oldAlpha ? TT_EXACTBOUND : TT_UPPERBOUND;
    ttEntry->update(board->stack->hash, bestMove, 0, unadjustedEval, valueToTT(bestValue, stack->ply), ttPv, flags);

    return bestValue;
}

template <NodeType nt>
Eval search(Board* board, SearchStack* stack, Thread* thread, int depth, Eval alpha, Eval beta, bool cutNode) {
    constexpr bool rootNode = nt == ROOT_NODE;
    constexpr bool pvNode = nt == PV_NODE || nt == ROOT_NODE;
    constexpr NodeType nodeType = nt == ROOT_NODE ? PV_NODE : NON_PV_NODE;

    assert(-EVAL_INFINITE <= alpha && alpha < beta && beta <= EVAL_INFINITE);
    assert(!(pvNode && cutNode));
    assert(pvNode || alpha == beta - 1);

    if (pvNode)
        stack->pvLength = stack->ply;
    thread->searchData.selDepth = std::max(stack->ply, thread->searchData.selDepth);

    if (!rootNode && alpha < 0 && hasUpcomingRepetition(board, stack->ply)) {
        alpha = drawEval(thread);
        if (alpha >= beta)
            return alpha;
    }

    if (depth <= 0) return qsearch<nodeType>(board, thread, stack, alpha, beta);

    BoardStack boardStack;
    Move bestMove = MOVE_NONE;
    Move excludedMove = stack->excludedMove;
    Eval bestValue = -EVAL_INFINITE;
    Eval oldAlpha = alpha;
    bool improving = false, skipQuiets = false, excluded = excludedMove != MOVE_NONE;

    (stack + 1)->killers[0] = (stack + 1)->killers[1] = MOVE_NONE;
    (stack + 1)->excludedMove = MOVE_NONE;
    (stack + 1)->doubleExtensions = stack->doubleExtensions;

    if (!rootNode) {

        if (thread->mainThread && timeOver(thread->searchParameters, &thread->searchData))
            thread->threadPool->stopSearching();

        // Check for stop or max depth
        if (!thread->searching || thread->exiting || stack->ply >= MAX_PLY || isDraw(board))
            return (stack->ply >= MAX_PLY && !board->stack->checkers) ? evaluate(board, &thread->nnue) : drawEval(thread);

        // Mate distance pruning
        alpha = std::max((int)alpha, (int)matedIn(stack->ply));
        beta = std::min((int)beta, (int)mateIn(stack->ply + 1));
        if (alpha >= beta)
            return alpha;
    }

    // TT Lookup
    bool ttHit = false;
    TTEntry* ttEntry = nullptr;
    Move ttMove = MOVE_NONE;
    Eval ttValue = EVAL_NONE, ttEval = EVAL_NONE;
    int ttDepth = 0;
    uint8_t ttFlag = TT_NOBOUND;
    bool ttPv = pvNode;

    if (!excluded) {
        ttEntry = TT.probe(board->stack->hash, &ttHit);
        if (ttHit) {
            ttMove = ttEntry->bestMove;
            ttValue = valueFromTt(ttEntry->value, stack->ply);
            ttEval = ttEntry->eval;
            ttDepth = ttEntry->depth + TT_DEPTH_OFFSET;
            ttFlag = ttEntry->flags & 0x3;
            ttPv = ttPv || ttEntry->flags & 0x4;
        }
    }

    // TT cutoff
    if (!pvNode && ttDepth >= depth && ttValue != EVAL_NONE && ((ttFlag == TT_UPPERBOUND && ttValue <= alpha) || (ttFlag == TT_LOWERBOUND && ttValue >= beta) || (ttFlag == TT_EXACTBOUND)))
        return ttValue;

    Eval eval = EVAL_NONE, unadjustedEval = EVAL_NONE;
    if (board->stack->checkers) {
        stack->staticEval = EVAL_NONE;
        goto movesLoop;
    }
    else if (excluded) {
        unadjustedEval = eval = stack->staticEval;
    }
    else if (ttHit) {
        unadjustedEval = ttEval != EVAL_NONE ? ttEval : evaluate(board, &thread->nnue);
        eval = stack->staticEval = thread->history.correctStaticEval(unadjustedEval, board);

        if (ttValue != EVAL_NONE && ((ttFlag == TT_UPPERBOUND && ttValue < eval) || (ttFlag == TT_LOWERBOUND && ttValue > eval) || (ttFlag == TT_EXACTBOUND)))
            eval = ttValue;
    }
    else {
        unadjustedEval = evaluate(board, &thread->nnue);
        eval = stack->staticEval = thread->history.correctStaticEval(unadjustedEval, board);

        ttEntry->update(board->stack->hash, MOVE_NONE, 0, unadjustedEval, EVAL_NONE, ttPv, TT_NOBOUND);
    }

    // IIR
    if (ttMove == MOVE_NONE && depth >= iirMinDepth)
        depth--;

    // Improving
    if ((stack - 2)->staticEval != EVAL_NONE) {
        improving = stack->staticEval > (stack - 2)->staticEval;
    }
    else if ((stack - 4)->staticEval != EVAL_NONE) {
        improving = stack->staticEval > (stack - 4)->staticEval;
    }

    // Reverse futility pruning
    if (!rootNode && depth < rfpDepth && std::abs(eval) < EVAL_MATE_IN_MAX_PLY && eval - rfpFactor * (depth - improving) >= beta)
        return eval;

    // Razoring
    if (!rootNode && depth < razoringDepth && eval + (razoringFactor * depth) < alpha) {
        Eval razorValue = qsearch<NON_PV_NODE>(board, thread, stack, alpha, beta);
        if (razorValue <= alpha)
            return razorValue;
    }

    // Null move pruning
    if (!pvNode
        && eval >= stack->staticEval
        && eval >= beta
        && beta > -EVAL_MATE_IN_MAX_PLY
        && !excluded
        && (stack - 1)->movedPiece != NO_PIECE
        && depth >= 3
        && stack->ply >= thread->searchData.nmpPlies
        && hasNonPawns(board)
        ) {
        stack->move = MOVE_NULL;
        stack->movedPiece = NO_PIECE;
        int R = nmpRedBase + depth / nmpDepthDiv + std::min((eval - beta) / nmpDivisor, nmpMin);

        doNullMove(board, &boardStack);
        Eval nullValue = -search<NON_PV_NODE>(board, stack + 1, thread, depth - R, -beta, -beta + 1, !cutNode);
        undoNullMove(board);

        if (!thread->searching || thread->exiting)
            return 0;

        if (nullValue >= beta) {
            if (nullValue > EVAL_MATE_IN_MAX_PLY)
                nullValue = beta;

            if (thread->searchData.nmpPlies || depth < 15)
                return nullValue;

            thread->searchData.nmpPlies = stack->ply + (depth - R) * 2 / 3;
            Eval verificationValue = search<NON_PV_NODE>(board, stack, thread, depth - R, beta - 1, beta, false);
            thread->searchData.nmpPlies = 0;

            if (verificationValue >= beta)
                return nullValue;
        }
    }

    assert(board->stack);

movesLoop:

    Move quietMoves[64] = { MOVE_NONE };
    Move captureMoves[64] = { MOVE_NONE };
    int quietMoveCount = 0;
    int captureMoveCount = 0;

    // Moves loop
    MoveGen movegen(board, &thread->history, stack, ttMove, stack->killers, depth);
    Move move;
    int moveCount = 0;
    while ((move = movegen.nextMove()) != MOVE_NONE) {

        if (move == excludedMove)
            continue;
        if (rootNode && std::find(thread->excludedRootMoves.begin(), thread->excludedRootMoves.end(), move) != thread->excludedRootMoves.end())
            continue;

        bool capture = isCapture(board, move);
        if (!capture && skipQuiets)
            continue;

        if (!isLegal(board, move))
            continue;

        uint64_t nodesBeforeMove = thread->searchData.nodesSearched;
        int moveHistory = thread->history.getHistory(board, stack, move, capture);

        if (!rootNode
            && bestValue > -EVAL_MATE_IN_MAX_PLY
            && hasNonPawns(board)
            ) {

            int lmrDepth = std::max(0, depth - REDUCTIONS[!capture][depth][moveCount]);

            if (!pvNode && !skipQuiets && !board->stack->checkers) {

                // Movecount pruning (LMP)
                if (moveCount >= LMP_MARGIN[depth][improving]) {
                    skipQuiets = true;
                }
                // Futility pruning
                else if (lmrDepth < fpDepth && eval + fpBase + fpFactor * lmrDepth <= alpha)
                    skipQuiets = true;
            }

            // History pruning
            if (!pvNode && lmrDepth < historyPruningDepth && moveHistory < historyPruningFactor * depth)
                continue;

            // SEE Pruning
            if (!pvNode && depth < seeDepth && !SEE(board, move, SEE_MARGIN[depth][!capture]))
                continue;

        }

        // Extensions
        bool doExtensions = !rootNode && stack->ply < thread->searchData.rootDepth * 2;
        int extension = 0;
        if (doExtensions
            && depth >= 7
            && move == ttMove
            && !excluded
            && (ttFlag & TT_LOWERBOUND)
            && std::abs(ttValue) < EVAL_MATE_IN_MAX_PLY
            && ttDepth >= depth - 3
            ) {
            Eval singularBeta = ttValue - depth;
            int singularDepth = (depth - 1) / 2;

            stack->excludedMove = move;
            Eval singularValue = search<NON_PV_NODE>(board, stack, thread, singularDepth, singularBeta - 1, singularBeta, cutNode);
            stack->excludedMove = MOVE_NONE;

            if (singularValue < singularBeta) {
                // This move is singular and we should investigate it further
                extension = 1;
                if (!pvNode && singularValue + doubleExtensionMargin < singularBeta && stack->doubleExtensions <= doubleExtensionLimit) {
                    extension = 2;
                    stack->doubleExtensions = (stack - 1)->doubleExtensions + 1;
                    depth += depth < 10;
                }
            }
            // Multicut: If we beat beta, that means there's likely more moves that beat beta and we can skip this node
            else if (singularBeta >= beta)
                return singularBeta;
            // We didn't prove singularity and an excluded search couldn't beat beta, but if the ttValue can we still reduce the depth
            else if (ttValue >= beta)
                extension = -2;
            // We didn't prove singularity and an excluded search couldn't beat beta, but we are expected to fail low 2 different ways, so reduce
            else if (cutNode && ttValue <= alpha)
                extension = -1;
        }

        uint64_t newHash = hashAfter(board, move);
        TT.prefetch(newHash);

        if (!capture) {
            if (quietMoveCount < 64)
                quietMoves[quietMoveCount++] = move;
        }
        else {
            if (captureMoveCount < 64)
                captureMoves[captureMoveCount++] = move;
        }

        // Some setup stuff
        moveCount++;
        thread->searchData.nodesSearched++;
        stack->move = move;
        stack->movedPiece = board->pieces[moveOrigin(move)];
        doMove(board, &boardStack, move, newHash, &thread->nnue);

        if (doExtensions && extension == 0 && board->stack->checkers)
            extension = 1;

        Eval value;
        int newDepth = depth - 1 + extension;

        // Very basic LMR: Late moves are being searched with less depth
        // Check if the move can exceed alpha
        if (moveCount > lmrMcBase + lmrMcPv * pvNode && depth >= lmrMinDepth && (!capture || !ttPv)) {
            int reducedDepth = newDepth - REDUCTIONS[!capture][depth][moveCount];

            if (!ttPv)
                reducedDepth--;

            if (cutNode)
                reducedDepth -= 2;

            if (capture)
                reducedDepth += moveHistory / lmrHistoryFactorCapture;
            else
                reducedDepth += moveHistory / lmrHistoryFactorQuiet;

            reducedDepth = std::clamp(reducedDepth, 1, newDepth);
            value = -search<NON_PV_NODE>(board, stack + 1, thread, reducedDepth, -(alpha + 1), -alpha, true);

            bool doShallowerSearch = !rootNode && value < bestValue + newDepth;
            bool doDeeperSearch = value > (bestValue + lmrDeeperBase + lmrDeeperFactor * newDepth);
            newDepth += doDeeperSearch - doShallowerSearch;

            if (value > alpha && reducedDepth < newDepth) {
                value = -search<NON_PV_NODE>(board, stack + 1, thread, newDepth, -(alpha + 1), -alpha, !cutNode);

                if (!capture) {
                    int bonus = std::min(lmrPassBonusBase + lmrPassBonusFactor * depth, lmrPassBonusMax);
                    thread->history.updateContinuationHistory(board, stack, move, bonus);
                }
            }
        }
        else {
            value = -search<NON_PV_NODE>(board, stack + 1, thread, newDepth, -(alpha + 1), -alpha, !cutNode);
        }

        // PV moves will be researched at full depth if good enough
        if (pvNode && (moveCount == 1 || value > alpha)) {
            value = -search<PV_NODE>(board, stack + 1, thread, newDepth, -beta, -alpha, false);
        }

        undoMove(board, move, &thread->nnue);
        assert(value > -EVAL_INFINITE && value < EVAL_INFINITE);

        if (!thread->searching || thread->exiting)
            return 0;

        if (value > bestValue) {
            bestValue = value;
            bestMove = move;

            if (value > alpha) {
                alpha = value;

                if (pvNode)
                    updatePv(stack, move);

                if (bestValue >= beta) {

                    int bonus = std::min(historyBonusBase + historyBonusFactor * (depth + (eval <= alpha) + (value - 250 > beta)), historyBonusMax);
                    if (!capture) {
                        // Update quiet killers
                        if (move != stack->killers[0]) {
                            stack->killers[1] = stack->killers[0];
                            stack->killers[0] = move;
                        }

                        // Update counter move
                        if (stack->ply > 0)
                            thread->history.setCounterMove((stack - 1)->move, move);

                        thread->history.updateQuietHistories(board, stack, move, bonus, quietMoves, quietMoveCount);
                    }
                    thread->history.updateCaptureHistory(board, move, bonus, captureMoves, captureMoveCount);
                    break;
                }
            }
        }

        if (rootNode) {
            if (thread->rootMoveNodes.count(move) == 0)
                thread->rootMoveNodes[move] = thread->searchData.nodesSearched - nodesBeforeMove;
            else
                thread->rootMoveNodes[move] = thread->searchData.nodesSearched - nodesBeforeMove + thread->rootMoveNodes[move];
        }

    }

    if (moveCount == 0) {
        if (board->stack->checkers) {
            return excluded ? -EVAL_INFINITE : matedIn(stack->ply); // Checkmate
        }
        return 0; // Stalemate
    }

    // Insert into TT
    int flags = bestValue >= beta ? TT_LOWERBOUND : alpha != oldAlpha ? TT_EXACTBOUND : TT_UPPERBOUND;
    if (!excluded)
        ttEntry->update(board->stack->hash, bestMove, depth, thread->threadPool->threads.size() == 1 ? eval : unadjustedEval, valueToTT(bestValue, stack->ply), ttPv, flags);

    // Adjust correction history
    if (!board->stack->checkers
        && (bestMove == MOVE_NONE || !isCapture(board, bestMove))
        && !(bestValue >= beta && bestValue <= stack->staticEval)
        && !(bestMove == MOVE_NONE && bestValue >= stack->staticEval)
        ) {
        int bonus = std::clamp((int)(bestValue - stack->staticEval) * depth / 8, -CORRECTION_HISTORY_LIMIT / 4, CORRECTION_HISTORY_LIMIT / 4);
        thread->history.updateCorrectionHistory(board, bonus);
    }

    assert(bestValue > -EVAL_INFINITE && bestValue < EVAL_INFINITE);

    return bestValue;
}

void Thread::tsearch() {
    if (TUNE_ENABLED)
        initReductions();

    resetAccumulators(&rootBoard, &nnue);

    searchData.nodesSearched = 0;
    if (mainThread)
        initTimeManagement(&rootBoard, searchParameters, &searchData);

    int multiPvCount = 0;
    {
        Move moves[MAX_MOVES] = { MOVE_NONE };
        int m = 0;
        generateMoves(&rootBoard, moves, &m);
        for (int i = 0; i < m; i++) {
            if (isLegal(&rootBoard, moves[i]))
                multiPvCount++;
        }
    }
    multiPvCount = std::min(multiPvCount, UCI::Options.multiPV.value);

    int maxDepth = searchParameters->depth == 0 ? MAX_PLY - 1 : searchParameters->depth;

    Eval previousValue = EVAL_NONE;
    Move previousMove = MOVE_NONE;

    int bestMoveStability = 0;

    constexpr int STACK_OVERHEAD = 4;
    SearchStack stackList[MAX_PLY + STACK_OVERHEAD];
    SearchStack* stack = &stackList[STACK_OVERHEAD];

    rootMoveNodes.clear();

    for (int depth = 1; depth <= maxDepth; depth++) {

        excludedRootMoves.clear();
        for (int rootMoveIdx = 0; rootMoveIdx < multiPvCount; rootMoveIdx++) {

            for (int i = 0; i < MAX_PLY + STACK_OVERHEAD; i++) {
                stackList[i].pvLength = 0;
                stackList[i].ply = i - STACK_OVERHEAD;
                stackList[i].staticEval = EVAL_NONE;
                stackList[i].excludedMove = MOVE_NONE;
                stackList[i].killers[0] = MOVE_NONE;
                stackList[i].killers[1] = MOVE_NONE;
                stackList[i].doubleExtensions = 0;
                if (i <= STACK_OVERHEAD) {
                    stackList[i].movedPiece = NO_PIECE;
                    stackList[i].move = MOVE_NONE;
                }
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
                value = search<ROOT_NODE>(&rootBoard, stack, this, searchDepth, alpha, beta, false);

                // Stop if we need to
                if (!searching || exiting)
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

                if (value >= EVAL_MATE_IN_MAX_PLY) {
                    beta = EVAL_INFINITE;
                    failHighs = 0;
                }

                delta *= aspirationWindowDeltaFactor;
            }

            if (!searching || exiting)
                goto bestMoveOutput;
            else if (rootMoveIdx == 0)
                result.rootMoves.clear();

            excludedRootMoves.push_back(stack->pv[0]);

            std::vector<Move> pv;
            for (int i = 0; i < stack->pvLength; i++)
                pv.push_back(stack->pv[i]);

            assert(pv.size() > 0);

            RootMove rootMove = {
                value,
                depth,
                searchData.selDepth,
                pv
            };
            result.rootMoves.push_back(rootMove);
        }

        if (mainThread) {
            // Send UCI info
            int64_t ms = getTime() - searchData.startTime;
            int64_t nodes = threadPool->nodesSearched();
            int64_t nps = ms == 0 ? 0 : nodes / ((double)ms / 1000);

            std::sort(result.rootMoves.begin(), result.rootMoves.end(), [](RootMove rm1, RootMove rm2) { return rm1.value > rm2.value; });

            for (int rootMoveIdx = 0; rootMoveIdx < multiPvCount; rootMoveIdx++) {
                RootMove rootMove = result.rootMoves[rootMoveIdx];
                std::cout << "info depth " << rootMove.depth << " seldepth " << rootMove.selDepth << " score " << formatEval(rootMove.value) << " multipv " << (rootMoveIdx + 1) << " nodes " << nodes << " time " << ms << " nps " << nps << " hashfull " << TT.hashfull() << " pv ";

                // Send PV
                for (Move move : rootMove.pv)
                    std::cout << moveToString(move, UCI::Options.chess960.value) << " ";
                std::cout << std::endl;
            }

            // Adjust time management
            double tmAdjustment = tmInitialAdjustment;

            // Based on best move stability
            if (result.rootMoves[0].pv[0] == previousMove)
                bestMoveStability = std::min(bestMoveStability + 1, tmBestMoveStabilityMax);
            else
                bestMoveStability = 0;
            tmAdjustment *= tmBestMoveStabilityBase - bestMoveStability * tmBestMoveStabilityFactor;

            // Based on score difference to last iteration
            tmAdjustment *= tmEvalDiffBase + std::clamp(previousValue - result.rootMoves[0].value, tmEvalDiffMin, tmEvalDiffMax) * tmEvalDiffFactor;

            // Based on fraction of nodes that went into the best move
            tmAdjustment *= tmNodesBase - tmNodesFactor * ((double)rootMoveNodes[result.rootMoves[0].pv[0]] / (double)searchData.nodesSearched);

            if (timeOverDepthCleared(searchParameters, &searchData, tmAdjustment)) {
                threadPool->stopSearching();
                goto bestMoveOutput;
            }
        }

        previousMove = result.rootMoves[0].pv[0];
        previousValue = result.rootMoves[0].value;
    }

bestMoveOutput:
    result.finished = true;

    if (mainThread) {
        std::cout << "bestmove " << moveToString(result.rootMoves[0].pv[0], UCI::Options.chess960.value) << std::endl;
    }
}