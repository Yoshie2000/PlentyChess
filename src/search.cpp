#include <cstdint>
#include <algorithm>
#include <inttypes.h>
#include <cassert>
#include <chrono>
#include <cmath>

#include "search.h"
#include "board.h"
#include "move.h"
#include "evaluation.h"
#include "thread.h"
#include "tt.h"
#include "history.h"
#include "time.h"
#include "nnue.h"
#include "spsa.h"

// Reduction / Margin tables
float lmrReductionNoisyBase = -0.4096330074311659;
TUNE(lmrReductionNoisyBase, -5.00f, 5.00f);
float lmrReductionNoisyFactor = 2.914479938734182f;
TUNE(lmrReductionNoisyFactor, 1.00f, 10.00f);
float lmrReductionQuietBase = 0.5797128756040184f;
TUNE(lmrReductionQuietBase, -5.00f, 5.00f);
float lmrReductionQuietFactor = 2.7676234985659187f;
TUNE(lmrReductionQuietFactor, 1.00f, 10.00f);

float seeMarginNoisy =  -27.39960199367735f;
TUNE(seeMarginNoisy, -100.0f, -1.0f);
float seeMarginQuiet = -76.35829934140916f;
TUNE(seeMarginQuiet, -200.0f, -1.0f);
float lmpMarginWorseningBase = 1.0825945418931069f;
TUNE(lmpMarginWorseningBase, -2.5f, 10.0f);
float lmpMarginWorseningFactor = 0.5334710035758581f;
TUNE(lmpMarginWorseningFactor, 0.05f, 2.5f);
float lmpMarginWorseningPower = 1.995993533227606f;
TUNE(lmpMarginWorseningPower, 0.5f, 5.0f);
float lmpMarginImprovingBase = 2.7970589698247568f;
TUNE(lmpMarginImprovingBase, -2.5f, 10.0f);
float lmpMarginImprovingFactor = 1.0496859839425705f;
TUNE(lmpMarginImprovingFactor, 0.05f, 2.5f);
float lmpMarginImprovingPower = 1.717757682160649f;
TUNE(lmpMarginImprovingPower, 0.5f, 5.0f);

// Search values
int qsFutilityOffset = 63;
TUNE(qsFutilityOffset, 0, 250);

// Pre-search pruning
int rfpDepth = 5;
TUNE(rfpDepth, 2, 20);
int rfpFactor = 74;
TUNE(rfpFactor, 1, 250);

int razoringDepth = 4;
TUNE(razoringDepth, 2, 20);
int razoringFactor = 321;
TUNE(razoringFactor, 1, 1000);

int nmpRedBase = 3;
TUNE(nmpRedBase, 1, 5);
int nmpDepthDiv = 3;
TUNE(nmpDepthDiv, 1, 6);
int nmpMin = 3;
TUNE(nmpMin, 1, 10);
int nmpDivisor = 199;
TUNE(nmpDivisor, 10, 1000);

int seeDepth = 9;
TUNE(seeDepth, 2, 20);

int lmrMcBase = 4;
TUNE(lmrMcBase, 1, 10);
int lmrMcPv = 4;
TUNE(lmrMcPv, 1, 10);
int lmrMinDepth = 3;
TUNE(lmrMinDepth, 1, 10);

int lmrPassBonusFactor = 11;
TUNE(lmrPassBonusFactor, 1, 32);
int lmrPassBonusMax = 1403;
TUNE(lmrPassBonusMax, 32, 8192);

int quietBonusFactor = 16;
TUNE(quietBonusFactor, 1, 32);
int quietBonusMax = 1663;
TUNE(quietBonusMax, 32, 8192);

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

    Move moves[MAX_MOVES] = { MOVE_NONE };
    int moveCount = 0;
    generateMoves(board, moves, &moveCount);

    uint64_t nodes = 0;
    for (int i = 0; i < moveCount; i++) {
        Move move = moves[i];

        if (!isLegal(board, move))
            continue;

        doMove(board, &stack, move);
        uint64_t subNodes = perftInternal(board, depth - 1);
        undoMove(board, move);

        nodes += subNodes;
    }
    return nodes;
}

uint64_t perft(Board* board, int depth) {
    clock_t begin = clock();
    BoardStack stack;

    Move moves[MAX_MOVES] = { MOVE_NONE };
    int moveCount = 0;
    generateMoves(board, moves, &moveCount);

    uint64_t nodes = 0;
    for (int i = 0; i < moveCount; i++) {
        Move move = moves[i];

        if (!isLegal(board, move))
            continue;

        doMove(board, &stack, move);
        uint64_t subNodes = perftInternal(board, depth - 1);
        undoMove(board, move);

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

    assert(alpha >= -EVAL_INFINITE && alpha < beta && beta <= EVAL_INFINITE);

    // Check for stop
    if (stack->ply >= MAX_PLY || isDraw(board))
        return (stack->ply >= MAX_PLY && !board->stack->checkers) ? evaluate(board) : drawEval(thread);

    BoardStack boardStack;
    Move pv[MAX_PLY + 1] = { MOVE_NONE };
    Eval bestValue, futilityValue;

    (stack + 1)->ply = stack->ply + 1;

    stack->staticEval = bestValue = evaluate(board);
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
    MoveGen movegen(board, stack, true);
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
        doMove(board, &boardStack, move);

        Eval value = -qsearch<nodeType>(board, thread, stack + 1, -beta, -alpha);
        undoMove(board, move);
        assert(value > -EVAL_INFINITE && value < EVAL_INFINITE);

        if (value > bestValue) {
            bestValue = value;

            if (value > alpha) {
                alpha = value;

                if (pvNode)
                    updatePv(stack->pv, move, (stack + 1)->pv);

                if (bestValue >= beta)
                    break;
            }
        }

    }

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

    if (!rootNode && alpha < 0 && hasUpcomingRepetition(board, stack->ply)) {
        alpha = drawEval(thread);
        if (alpha >= beta)
            return alpha;
    }

    if (depth <= 0) return qsearch<nodeType>(board, thread, stack, alpha, beta);

    BoardStack boardStack;
    Move pv[MAX_PLY + 1] = { MOVE_NONE };
    Move bestMove = MOVE_NONE;
    Eval bestValue = -EVAL_INFINITE;
    Eval oldAlpha = alpha;
    bool improving = false, skipQuiets = false;

    Move quietMoves[MAX_MOVES] = { MOVE_NONE };
    int quietMoveCount = 0;

    (stack + 1)->ply = stack->ply + 1;
    (stack + 1)->killers[0] = (stack + 1)->killers[1] = MOVE_NONE;

    if (!rootNode) {

        if (timeOver(&thread->searchParameters, &thread->searchData))
            thread->searchData.stopSearching = true;

        // Check for stop or max depth
        if (thread->searchData.stopSearching || stack->ply >= MAX_PLY || isDraw(board))
            return (stack->ply >= MAX_PLY && !board->stack->checkers) ? evaluate(board) : drawEval(thread);

        // Mate distance pruning
        alpha = std::max((int)alpha, (int)matedIn(stack->ply));
        beta = std::min((int)beta, (int)mateIn(stack->ply + 1));
        if (alpha >= beta)
            return alpha;
    }

    // TT Lookup
    bool ttHit;
    TTEntry* ttEntry = TT.probe(board->stack->hash, &ttHit);
    Move ttMove = ttHit ? ttEntry->bestMove : MOVE_NONE;
    Eval ttValue = ttHit ? valueFromTt(ttEntry->value, stack->ply) : EVAL_NONE;
    int ttDepth = ttHit ? ttEntry->depth : 0;
    uint8_t ttFlag = ttHit ? ttEntry->flags & 0x3 : TT_NOBOUND;
    bool ttPv = pvNode || (ttEntry->flags >> 2);

    // TT cutoff
    if (!pvNode && ttDepth >= depth && ttValue != EVAL_NONE && ((ttFlag == TT_UPPERBOUND && ttValue <= alpha) || (ttFlag == TT_LOWERBOUND && ttValue >= beta) || (ttFlag == TT_EXACTBOUND)))
        return ttValue;

    Eval eval = EVAL_NONE;
    if (board->stack->checkers) {
        stack->staticEval = EVAL_NONE;
        goto movesLoop;
    }
    if (ttHit) {
        eval = stack->staticEval = ttEntry->eval != EVAL_NONE ? ttEntry->eval : evaluate(board);

        if (ttValue != EVAL_NONE && ((ttFlag == TT_UPPERBOUND && ttValue < eval) || (ttFlag == TT_LOWERBOUND && ttValue > eval) || (ttFlag == TT_EXACTBOUND)))
            eval = ttValue;
    }
    else {
        eval = stack->staticEval = evaluate(board);
        ttEntry->update(board->stack->hash, MOVE_NONE, 0, eval, EVAL_NONE, ttPv, TT_NOBOUND);
    }

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
    if (depth < rfpDepth && eval - (rfpFactor * depth) >= beta) return eval;

    // Razoring
    if (depth < razoringDepth && eval + (razoringFactor * depth) < alpha) {
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
    Move counterMove = stack->ply > 0 ? counterMoves[moveOrigin((stack - 1)->move)][moveTarget((stack - 1)->move)] : MOVE_NONE;
    MoveGen movegen(board, stack, ttMove, counterMove, stack->killers);
    Move move;
    int moveCount = 0;
    while ((move = movegen.nextMove()) != MOVE_NONE) {

        if (!isLegal(board, move))
            continue;

        bool capture = isCapture(board, move);
        if (!capture && skipQuiets)
            continue;

        if (!rootNode
            && bestValue > -EVAL_MATE_IN_MAX_PLY
            && hasNonPawns(board)
            ) {

            if (!skipQuiets) {

                // Movecount pruning (LMP)
                if (!pvNode
                    && !board->stack->checkers
                    && moveCount >= LMP_MARGIN[depth][improving]) {
                    skipQuiets = true;
                }
            }

            // SEE Pruning
            if (depth < seeDepth && !SEE(board, move, SEE_MARGIN[depth][!capture]))
                continue;

        }

        if (pvNode)
            (stack + 1)->pv = nullptr;

        if (!capture)
            quietMoves[quietMoveCount++] = move;

        moveCount++;
        thread->searchData.nodesSearched++;
        stack->move = move;
        stack->movedPiece = board->pieces[moveOrigin(move)];
        doMove(board, &boardStack, move);

        Eval value;
        int newDepth = depth - 1;
        if (board->stack->checkers)
            newDepth++;

        // Very basic LMR: Late moves are being searched with less depth
        // Check if the move can exceed alpha
        if (moveCount > lmrMcBase + lmrMcPv * pvNode && depth >= lmrMinDepth && (!capture || !ttPv)) {
            int reducedDepth = newDepth - REDUCTIONS[!capture][depth][moveCount];

            if (!ttPv)
                reducedDepth--;

            if (cutNode)
                reducedDepth--;

            reducedDepth = std::clamp(reducedDepth, 1, newDepth);
            value = -search<NON_PV_NODE>(board, stack + 1, thread, reducedDepth, -(alpha + 1), -alpha, true);

            if (value > alpha && reducedDepth < newDepth) {
                value = -search<NON_PV_NODE>(board, stack + 1, thread, newDepth, -(alpha + 1), -alpha, !cutNode);

                if (!capture) {
                    int bonus = std::min(lmrPassBonusFactor * (depth + 1) * (depth + 1), lmrPassBonusMax);
                    updateContinuationHistory(board, stack, move, bonus);
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

        undoMove(board, move);
        assert(value > -EVAL_INFINITE && value < EVAL_INFINITE);

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
                            counterMoves[moveOrigin((stack - 1)->move)][moveTarget((stack - 1)->move)] = move;

                        int bonus = std::min(quietBonusFactor * (depth + 1) * (depth + 1), quietBonusMax);
                        updateHistories(board, stack, move, bonus, quietMoves, quietMoveCount);
                    }
                    break;
                }
            }
        }

    }

    if (moveCount == 0) {
        if (board->stack->checkers) {
            return matedIn(stack->ply); // Checkmate
        }
        return 0; // Stalemate
    }

    // Insert into TT
    int flags = bestValue >= beta ? TT_LOWERBOUND : alpha != oldAlpha ? TT_EXACTBOUND : TT_UPPERBOUND;
    ttEntry->update(board->stack->hash, bestMove, depth, eval, valueToTT(bestValue, stack->ply), ttPv, flags);

    assert(bestValue > -EVAL_INFINITE && bestValue < EVAL_INFINITE);

    return bestValue;
}

void Thread::tsearch() {
    nnue.resetAccumulators(&rootBoard);

    int maxDepth = searchParameters.depth == 0 ? MAX_PLY - 1 : searchParameters.depth;
    Move bestMove = MOVE_NONE;

    searchData.nodesSearched = 0;
    initTimeManagement(&rootBoard, &searchParameters, &searchData);

    for (int depth = 1; depth <= maxDepth; depth++) {
        SearchStack stack[MAX_PLY];
        Move pv[MAX_PLY + 1];
        pv[0] = MOVE_NONE;
        stack->pv = pv;
        stack->ply = 0;
        stack->move = MOVE_NONE;
        stack->movedPiece = NO_PIECE;
        stack->killers[0] = stack->killers[1] = MOVE_NONE;

        // Search
        Eval value = search<ROOT_NODE>(&rootBoard, stack, this, depth, -EVAL_INFINITE, EVAL_INFINITE, false);

        if (searchData.stopSearching) {
            if (bestMove == MOVE_NONE)
                bestMove = stack->pv[0];
            break;
        }

        // Send UCI info
        int64_t ms = getTime() - searchData.startTime;
        int64_t nps = ms == 0 ? 0 : (int64_t)((searchData.nodesSearched) / ((double)ms / 1000));
        std::cout << "info depth " << depth << " score " << formatEval(value) << " nodes " << searchData.nodesSearched << " time " << ms << " nps " << nps << " pv ";

        // Send PV
        bestMove = stack->pv[0];
        Move move;
        while ((move = *stack->pv++) != MOVE_NONE) {
            std::cout << moveToString(move) << " ";
        }
        std::cout << std::endl;

        if (timeOverDepthCleared(&searchParameters, &searchData))
            break;
    }

    std::cout << "bestmove " << moveToString(bestMove) << std::endl;
}