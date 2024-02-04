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

// Aspiration windows
TUNE_INT(aspirationWindowMinDepth, 4, 2, 20);
TUNE_INT(aspirationWindowDelta, 17, 1, 200);
TUNE_INT(aspirationWindowMaxFailHighs, 3, 0, 20);
TUNE_FLOAT(aspirationWindowDeltaFactor, 1.789268600882079, 1.0f, 10.0f);

// Reduction / Margin tables
TUNE_FLOAT(lmrReductionNoisyBase, -0.5507368054141233, -5.00f, 5.00f);
TUNE_FLOAT(lmrReductionNoisyFactor, 2.9945957027805936, 1.00f, 10.00f);
TUNE_FLOAT(lmrReductionQuietBase, 0.7842110713340991, -5.00f, 5.00f);
TUNE_FLOAT(lmrReductionQuietFactor, 2.8063316175892044, 1.00f, 10.00f);

TUNE_FLOAT(seeMarginNoisy, -26.14630196595127, -100.0f, -1.0f);
TUNE_FLOAT(seeMarginQuiet, -75.36717662656213, -200.0f, -1.0f);
TUNE_FLOAT(lmpMarginWorseningBase, 1.4194200452372927, -2.5f, 10.0f);
TUNE_FLOAT(lmpMarginWorseningFactor, 0.4609450270167632, 0.05f, 2.5f);
TUNE_FLOAT(lmpMarginWorseningPower, 1.785778252619638, 0.5f, 5.0f);
TUNE_FLOAT(lmpMarginImprovingBase, 3.096315467954453, -2.5f, 10.0f);
TUNE_FLOAT(lmpMarginImprovingFactor, 1.0584246020918444, 0.05f, 2.5f);
TUNE_FLOAT(lmpMarginImprovingPower, 1.8914578112950748, 0.5f, 5.0f);

// Search values
TUNE_INT(qsFutilityOffset, 49, 0, 250);

// Pre-search pruning
TUNE_INT(iirMinDepth, 4, 1, 20);

TUNE_INT(rfpDepth, 8, 2, 20);
TUNE_INT(rfpFactor, 84, 1, 250);

TUNE_INT(razoringDepth, 5, 2, 20);
TUNE_INT(razoringFactor, 310, 1, 1000);

TUNE_INT(nmpRedBase, 3, 1, 5);
TUNE_INT(nmpDepthDiv, 3, 1, 6);
TUNE_INT(nmpMin, 3, 1, 10);
TUNE_INT(nmpDivisor, 183, 10, 1000);

// In-search pruning
TUNE_INT(fpDepth, 11, 1, 20);
TUNE_INT(fpBase, 240, 0, 1000);
TUNE_INT(fpFactor, 132, 1, 500);

TUNE_INT(historyPruningDepth, 4, 1, 15);
TUNE_INT(historyPruningFactor, -2096, -8192, -128);

TUNE_INT(seeDepth, 9, 2, 20);

TUNE_INT(lmrMcBase, 2, 1, 10);
TUNE_INT(lmrMcPv, 2, 1, 10);
TUNE_INT(lmrMinDepth, 3, 1, 10);

TUNE_INT(lmrHistoryFactor, 13199, 128, 32768);
TUNE_INT(lmrDeeperBase, 43, 1, 200);
TUNE_INT(lmrDeeperFactor, 2, 0, 10);

TUNE_INT(lmrPassBonusFactor, 11, 1, 32);
TUNE_INT(lmrPassBonusMax, 1140, 32, 8192);

TUNE_INT(historyBonusFactor, 16, 1, 32);
TUNE_INT(historyBonusMax, 1757, 32, 8192);

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

uint64_t perftInternal(Board* board, int depth) {
    if (depth == 0) return C64(1);

    BoardStack stack;
    NNUE nnue;

    Move moves[MAX_MOVES] = { MOVE_NONE };
    int moveCount = 0;
    generateMoves(board, moves, &moveCount);

    uint64_t nodes = 0;
    for (int i = 0; i < moveCount; i++) {
        Move move = moves[i];

        if (!isLegal(board, move))
            continue;

        doMove(board, &stack, move, &nnue);
        uint64_t subNodes = perftInternal(board, depth - 1);
        undoMove(board, move, &nnue);

        nodes += subNodes;
    }
    return nodes;
}

uint64_t perft(Board* board, int depth) {
    clock_t begin = clock();
    BoardStack stack;
    NNUE nnue;

    Move moves[MAX_MOVES] = { MOVE_NONE };
    int moveCount = 0;
    generateMoves(board, moves, &moveCount);

    uint64_t nodes = 0;
    for (int i = 0; i < moveCount; i++) {
        Move move = moves[i];

        if (!isLegal(board, move))
            continue;

        doMove(board, &stack, move, &nnue);
        uint64_t subNodes = perftInternal(board, depth - 1);
        undoMove(board, move, &nnue);

        std::cout << moveToString(move) << ": " << subNodes << std::endl;

        nodes += subNodes;
    }

    clock_t end = clock();
    double time = (double)(end - begin) / CLOCKS_PER_SEC;
    uint64_t nps = nodes / time;
    std::cout << "Perft: " << nodes << " nodes in " << time << "s => " << nps << "nps" << std::endl;

    return nodes;
}

void updatePv(Move* pv, Move move, const Move* currentPv) {
    *pv++ = move;
    while (currentPv && *currentPv != MOVE_NONE)
        *pv++ = *currentPv++;
    *pv = MOVE_NONE;
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

    thread->searchData.selDepth = std::max(stack->ply, thread->searchData.selDepth);

    assert(alpha >= -EVAL_INFINITE && alpha < beta && beta <= EVAL_INFINITE);

    if (thread->mainThread && timeOver(thread->searchParameters, &thread->searchData))
        thread->threadPool->stopSearching();

    // Check for stop
    if (!thread->searching || thread->exiting || stack->ply >= MAX_PLY || isDraw(board))
        return (stack->ply >= MAX_PLY && !board->stack->checkers) ? evaluate(board, &thread->nnue) : drawEval(thread);

    BoardStack boardStack;
    Move pv[MAX_PLY + 1] = { MOVE_NONE };
    Move bestMove = MOVE_NONE;
    Eval bestValue, futilityValue;
    Eval oldAlpha = alpha;

    (stack + 1)->ply = stack->ply + 1;

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

    if (ttHit && ttEval != EVAL_NONE) {
        stack->staticEval = bestValue = ttEval;
    }
    else {
        stack->staticEval = bestValue = evaluate(board, &thread->nnue);
        ttEntry->update(board->stack->hash, MOVE_NONE, 0, stack->staticEval, EVAL_NONE, ttPv, TT_NOBOUND);
    }
    futilityValue = stack->staticEval + qsFutilityOffset;

    // Stand pat
    if (bestValue >= beta)
        return beta;
    if (alpha < bestValue)
        alpha = bestValue;

    // Mate distance pruning
    alpha = std::max((int)alpha, (int)matedIn(stack->ply));
    beta = std::min((int)beta, (int)mateIn(stack->ply + 1));
    if (alpha >= beta)
        return alpha;

    // Set up pv for the next search
    if (pvNode) {
        (stack + 1)->pv = pv;
        stack->pv[0] = MOVE_NONE;
    }

    // Moves loop
    MoveGen movegen(board, &thread->history, stack, ttMove, true, 1);
    Move move;
    int moveCount = 0;
    while ((move = movegen.nextMove()) != MOVE_NONE) {

        if (bestValue >= -EVAL_MATE_IN_MAX_PLY
            && futilityValue <= alpha
            && !SEE(board, move, 1)
            ) {
            bestValue = std::max(bestValue, futilityValue);
            continue;
        }

        if (!isLegal(board, move))
            continue;

        moveCount++;
        thread->searchData.nodesSearched++;
        doMove(board, &boardStack, move, &thread->nnue);

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
                    updatePv(stack->pv, move, (stack + 1)->pv);

                if (bestValue >= beta)
                    break;
            }
        }
    }

    // Insert into TT
    int flags = bestValue >= beta ? TT_LOWERBOUND : alpha != oldAlpha ? TT_EXACTBOUND : TT_UPPERBOUND;
    ttEntry->update(board->stack->hash, bestMove, 0, stack->staticEval, valueToTT(bestValue, stack->ply), ttPv, flags);

    // if (moveCount != 0 && moveCount == skippedMoves) {
    //     if (board->stack->checkers) {
    //         return matedIn(stack->ply); // Checkmate
    //     }
    //     return 0; // Stalemate
    // }

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

    thread->searchData.selDepth = std::max(stack->ply, thread->searchData.selDepth);

    if (!rootNode && alpha < 0 && hasUpcomingRepetition(board, stack->ply)) {
        alpha = drawEval(thread);
        if (alpha >= beta)
            return alpha;
    }

    if (depth <= 0) return qsearch<nodeType>(board, thread, stack, alpha, beta);

    BoardStack boardStack;
    Move pv[MAX_PLY + 1] = { MOVE_NONE };
    Move bestMove = MOVE_NONE;
    Move excludedMove = stack->excludedMove;
    Eval bestValue = -EVAL_INFINITE;
    Eval oldAlpha = alpha;
    bool improving = false, skipQuiets = false, excluded = excludedMove != MOVE_NONE;

    Move quietMoves[MAX_MOVES] = { MOVE_NONE };
    Move captureMoves[MAX_MOVES] = { MOVE_NONE };
    int quietMoveCount = 0;
    int captureMoveCount = 0;

    (stack + 1)->ply = stack->ply + 1;
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

    Eval eval = EVAL_NONE;
    if (board->stack->checkers || excluded) {
        stack->staticEval = EVAL_NONE;
        goto movesLoop;
    }
    if (ttHit) {
        eval = stack->staticEval = ttEval != EVAL_NONE ? ttEval : evaluate(board, &thread->nnue);

        if (ttValue != EVAL_NONE && ((ttFlag == TT_UPPERBOUND && ttValue < eval) || (ttFlag == TT_LOWERBOUND && ttValue > eval) || (ttFlag == TT_EXACTBOUND)))
            eval = ttValue;
    }
    else {
        eval = stack->staticEval = evaluate(board, &thread->nnue);
        ttEntry->update(board->stack->hash, MOVE_NONE, 0, stack->staticEval, EVAL_NONE, ttPv, TT_NOBOUND);
    }

    // IIR
    if (ttMove == MOVE_NONE && depth >= iirMinDepth)
        depth--;

    // Improving
    if (stack->ply >= 2 && (stack - 2)->staticEval != EVAL_NONE) {
        improving = stack->staticEval > (stack - 2)->staticEval;
    }
    else if (stack->ply >= 4 && (stack - 4)->staticEval != EVAL_NONE) {
        improving = stack->staticEval > (stack - 4)->staticEval;
    }
    else {
        improving = true;
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
        && stack->ply
        && (stack - 1)->move != MOVE_NULL
        && (stack - 1)->move != MOVE_NONE
        && depth >= 3
        && !board->stack->checkers
        && stack->ply >= thread->searchData.nmpPlies
        && hasNonPawns(board)
        ) {
        stack->move = MOVE_NULL;
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
    // Moves loop
    MoveGen movegen(board, &thread->history, stack, ttMove, stack->killers, depth);
    Move move;
    int moveCount = 0;
    while ((move = movegen.nextMove()) != MOVE_NONE) {

        if (move == excludedMove)
            continue;

        bool capture = isCapture(board, move);
        if (!capture && skipQuiets)
            continue;

        if (!isLegal(board, move))
            continue;

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
            if (lmrDepth < historyPruningDepth && moveHistory < historyPruningFactor * depth)
                continue;

            // SEE Pruning
            if (depth < seeDepth && !SEE(board, move, SEE_MARGIN[depth][!capture]))
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
                if (!pvNode && singularValue + 25 < singularBeta && stack->doubleExtensions <= 12) {
                    extension = 2;
                    stack->doubleExtensions = (stack - 1)->doubleExtensions + 1;
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

        // Some setup stuff
        if (pvNode)
            (stack + 1)->pv = nullptr;

        if (!capture)
            quietMoves[quietMoveCount++] = move;
        else
            captureMoves[captureMoveCount++] = move;

        moveCount++;
        thread->searchData.nodesSearched++;
        stack->move = move;
        stack->movedPiece = board->pieces[moveOrigin(move)];
        doMove(board, &boardStack, move, &thread->nnue);

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
                reducedDepth--;

            reducedDepth += moveHistory / lmrHistoryFactor;

            reducedDepth = std::clamp(reducedDepth, 1, newDepth);
            value = -search<NON_PV_NODE>(board, stack + 1, thread, reducedDepth, -(alpha + 1), -alpha, true);

            bool doDeeperSearch = value > (bestValue + lmrDeeperBase + lmrDeeperFactor * newDepth);
            newDepth += doDeeperSearch;

            if (value > alpha && reducedDepth < newDepth) {
                value = -search<NON_PV_NODE>(board, stack + 1, thread, newDepth, -(alpha + 1), -alpha, !cutNode);

                if (!capture) {
                    int bonus = std::min(lmrPassBonusFactor * (depth + 1) * (depth + 1), lmrPassBonusMax);
                    thread->history.updateContinuationHistory(board, stack, move, bonus);
                }
            }
        }
        else {
            value = -search<NON_PV_NODE>(board, stack + 1, thread, newDepth, -(alpha + 1), -alpha, !cutNode);
        }

        // PV moves will be researched at full depth if good enough
        if (pvNode && (moveCount == 1 || value > alpha)) {
            // Set up pv for the next search
            (stack + 1)->pv = pv;
            (stack + 1)->pv[0] = MOVE_NONE;
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
                    updatePv(stack->pv, move, (stack + 1)->pv);

                if (bestValue >= beta) {
                    if (!capture) {

                        // Update quiet killers
                        if (move != stack->killers[0]) {
                            stack->killers[1] = stack->killers[0];
                            stack->killers[0] = move;
                        }

                        // Update counter move
                        if (stack->ply >= 1)
                            thread->history.setCounterMove((stack - 1)->move, move);

                        int bonus = std::min(historyBonusFactor * (depth + 1) * (depth + 1), historyBonusMax);
                        thread->history.updateQuietHistories(board, stack, move, bonus, quietMoves, quietMoveCount);
                    }
                    int bonus = std::min(historyBonusFactor * (depth + 1) * (depth + 1), historyBonusMax);
                    thread->history.updateCaptureHistory(board, move, bonus, captureMoves, captureMoveCount);
                    break;
                }
            }
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
        ttEntry->update(board->stack->hash, bestMove, depth, thread->threadPool->threads.size() == 1 ? eval : stack->staticEval, valueToTT(bestValue, stack->ply), ttPv, flags);

    assert(bestValue > -EVAL_INFINITE && bestValue < EVAL_INFINITE);

    return bestValue;
}

void Thread::tsearch() {
    if (TUNE_ENABLED)
        initReductions();

    resetAccumulators(&rootBoard, &nnue);

    int maxDepth = searchParameters->depth == 0 ? MAX_PLY - 1 : searchParameters->depth;

    searchData.nodesSearched = 0;
    if (mainThread)
        initTimeManagement(&rootBoard, searchParameters, &searchData);

    // Necessary for aspiration windows
    Eval previousValue = EVAL_NONE;

    std::vector<Move> pvVec;
    bool finishedDepth = false;

    for (int depth = 1; depth <= maxDepth; depth++) {
        finishedDepth = false;

        SearchStack stackList[MAX_PLY + 4];
        SearchStack* stack = &stackList[4];
        Move pv[MAX_PLY + 1];
        pv[0] = MOVE_NONE;
        stack->pv = pv;
        stack->ply = 0;
        stack->move = MOVE_NONE;
        stack->movedPiece = NO_PIECE;
        stack->killers[0] = stack->killers[1] = MOVE_NONE;
        stack->excludedMove = MOVE_NONE;
        stack->doubleExtensions = 0;
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
        previousValue = value;

        if (!searching || exiting) {
            if (result.move == MOVE_NONE) {
                result.move = stack->pv[0];
                result.value = value;
                result.depth = depth;
                result.selDepth = searchData.selDepth;
            }
            break;
        }

        result.move = stack->pv[0];
        result.value = value;
        result.depth = depth;
        result.selDepth = searchData.selDepth;
        finishedDepth = true;

        if (mainThread) {
            // Send UCI info
            int64_t ms = getTime() - searchData.startTime;
            int64_t nodes = threadPool->nodesSearched();
            int64_t nps = ms == 0 ? 0 : nodes / ((double)ms / 1000);
            std::cout << "info depth " << depth << " seldepth " << searchData.selDepth << " score " << formatEval(value) << " nodes " << nodes << " time " << ms << " nps " << nps << " pv ";

            // Send PV
            Move move;
            pvVec.clear();
            while ((move = *stack->pv++) != MOVE_NONE) {
                std::cout << moveToString(move) << " ";
                pvVec.push_back(move);
            }
            std::cout << std::endl;

            // Every thread can request a time stop when a depth is cleared
            if (timeOverDepthCleared(searchParameters, &searchData)) {
                threadPool->stopSearching();
                break;
            }
        }
    }

    result.finished = true;

    if (mainThread) {
        if (!finishedDepth) {
            int64_t ms = getTime() - searchData.startTime;
            int64_t nodes = threadPool->nodesSearched();
            int64_t nps = ms == 0 ? 0 : nodes / ((double)ms / 1000);
            std::cout << "info depth " << result.depth << " seldepth " << result.selDepth << " score " << formatEval(result.value) << " nodes " << nodes << " time " << ms << " nps " << nps << " pv ";

            for (Move move : pvVec)
                std::cout << moveToString(move) << " ";
            std::cout << std::endl;
        }

        std::cout << "bestmove " << moveToString(result.move) << std::endl;
    }
}