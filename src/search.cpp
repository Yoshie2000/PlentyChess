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

int REDUCTIONS[2][MAX_PLY][MAX_MOVES];
int SEE_MARGIN[MAX_PLY][2];
int LMP_MARGIN[MAX_PLY][2];

void initReductions() {
    REDUCTIONS[0][0][0] = 0;
    REDUCTIONS[1][0][0] = 0;

    for (int i = 1; i < MAX_PLY; i++) {
        for (int j = 1; j < MAX_MOVES; j++) {
            REDUCTIONS[0][i][j] = -0.50 + log(i) * log(j) / 3.00; // non-quiet
            REDUCTIONS[1][i][j] = +0.00 + log(i) * log(j) / 2.50; // quiet
        }
    }

    for (int depth = 0; depth < MAX_PLY; depth++) {
        SEE_MARGIN[depth][0] = -30 * depth * depth; // non-quiet
        SEE_MARGIN[depth][1] = -80 * depth; // quiet

        LMP_MARGIN[depth][0] = 1.5 + 0.5 * std::pow(depth, 2.0); // non-improving
        LMP_MARGIN[depth][1] = 3.0 + 1.0 * std::pow(depth, 2.0); // improving
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

template <NodeType nodeType>
Eval qsearch(Board* board, SearchStack* stack, Eval alpha, Eval beta) {
    constexpr bool pvNode = nodeType == PV_NODE;

    assert(alpha >= -EVAL_INFINITE && alpha < beta && beta <= EVAL_INFINITE);

    // Check for stop
    if (board->stopSearching || stack->ply >= MAX_PLY)
        return (stack->ply >= MAX_PLY) ? evaluate(board) : 0;

    BoardStack boardStack;
    Move pv[MAX_PLY + 1] = { MOVE_NONE };
    Eval bestValue, futilityValue;

    stack->nodes = 0;
    (stack + 1)->ply = stack->ply + 1;
    (stack + 1)->nodes = 0;

    stack->staticEval = bestValue = evaluate(board);
    futilityValue = stack->staticEval + 75;

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
        stack->nodes++;
        doMove(board, &boardStack, move);

        Eval value = -qsearch<nodeType>(board, stack + 1, -beta, -alpha);
        undoMove(board, move);
        assert(value > -EVAL_INFINITE && value < EVAL_INFINITE);

        stack->nodes += (stack + 1)->nodes;

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

    if (depth <= 0) return qsearch<nodeType>(board, stack, alpha, beta);

    if (!rootNode && alpha < 0 && hasRepeated(board)) {
        alpha = 0;
        if (alpha >= beta)
            return alpha;
    }

    BoardStack boardStack;
    Move pv[MAX_PLY + 1] = { MOVE_NONE };
    Move bestMove = MOVE_NONE;
    Eval bestValue = -EVAL_INFINITE;
    Eval oldAlpha = alpha;
    bool improving = false, skipQuiets = false;

    Move quietMoves[MAX_MOVES] = { MOVE_NONE };
    int quietMoveCount = 0;

    stack->nodes = 0;
    (stack + 1)->ply = stack->ply + 1;
    (stack + 1)->nodes = 0;
    (stack + 1)->killers[0] = (stack + 1)->killers[1] = MOVE_NONE;

    if (!rootNode) {

        // Check for stop or max depth
        if (board->stopSearching || stack->ply >= MAX_PLY || isDraw(board, stack->ply))
            return (stack->ply >= MAX_PLY) ? evaluate(board) : 0;

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
    }

    // Improving
    if ((stack - 2)->staticEval != EVAL_NONE) {
        improving = stack->staticEval > (stack - 2)->staticEval;
    }
    else if ((stack - 4)->staticEval != EVAL_NONE) {
        improving = stack->staticEval > (stack - 4)->staticEval;
    }
    else {
        improving = true;
    }

    // Reverse futility pruning
    if (depth < 7 && eval - (70 * depth) >= beta) return eval;

    // Razoring
    if (depth <= 4 && eval + 250 * depth < alpha) {
        Eval razorValue = qsearch<NON_PV_NODE>(board, stack, alpha, beta);
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
        && stack->ply >= thread->nmpPlies
        && hasNonPawns(board)
        ) {
        stack->move = MOVE_NULL;
        int R = 3 + depth / 3 + std::min((eval - beta) / 200, 3);

        doNullMove(board, &boardStack);
        Eval nullValue = -search<NON_PV_NODE>(board, stack + 1, thread, depth - R, -beta, -beta + 1, !cutNode);
        undoNullMove(board);

        if (nullValue >= beta) {
            if (nullValue > EVAL_MATE_IN_MAX_PLY)
                nullValue = beta;

            if (thread->nmpPlies || depth < 15)
                return nullValue;

            thread->nmpPlies = stack->ply + (depth - R) * 2 / 3;
            Eval verificationValue = search<NON_PV_NODE>(board, stack, thread, depth - R, beta - 1, beta, false);
            thread->nmpPlies = 0;

            if (verificationValue >= beta)
                return nullValue;
        }
    }

movesLoop:
    // Moves loop
    MoveGen movegen(board, stack, ttMove, counterMoves[moveOrigin((stack - 1)->move)][moveTarget((stack - 1)->move)], stack->killers);
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
            if (depth <= 8 && !SEE(board, move, SEE_MARGIN[depth][!capture]))
                continue;

        }

        moveCount++;

        if (pvNode)
            (stack + 1)->pv = nullptr;

        if (!capture)
            quietMoves[quietMoveCount++] = move;

        stack->nodes++;
        stack->move = move;
        stack->movedPiece = board->pieces[moveOrigin(move)];
        doMove(board, &boardStack, move);

        Eval value;
        int newDepth = depth - 1;
        if (board->stack->checkers)
            newDepth++;

        // Very basic LMR: Late moves are being searched with less depth
        // Check if the move can exceed alpha
        if (moveCount > 5 + 5 * pvNode && depth >= 3) {
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
                    int bonus = std::min(8 * (depth + 1) * (depth + 1), 1024);
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

        stack->nodes += (stack + 1)->nodes;

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

                        int bonus = std::min(8 * (depth + 1) * (depth + 1), 1024);
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
    initHistory();
    nodesSearched = 0;

    int maxDepth = searchParameters.depth == 0 ? MAX_PLY - 1 : searchParameters.depth;

    Move bestMove = MOVE_NONE;

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
        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
        Eval value = search<ROOT_NODE>(&rootBoard, stack, this, depth, -EVAL_INFINITE, EVAL_INFINITE, false);
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

        nodesSearched += stack->nodes;

        if (rootBoard.stopSearching) {
            if (bestMove == MOVE_NONE)
                bestMove = stack->pv[0];
            break;
        }

        // Send UCI info
        int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
        int64_t nps = ms == 0 ? 0 : (int64_t)((stack->nodes) / ((double)ms / 1000));
        std::cout << "info depth " << depth << " score " << formatEval(value) << " nodes " << stack->nodes << " time " << ms << " nps " << nps << " pv ";

        // Send PV
        bestMove = stack->pv[0];
        Move move;
        while ((move = *stack->pv++) != MOVE_NONE) {
            std::cout << moveToString(move) << " ";
        }
        std::cout << std::endl;
    }

    std::cout << "bestmove " << moveToString(bestMove) << std::endl;
}