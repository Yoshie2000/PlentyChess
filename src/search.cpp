#include <cstdint>
#include <algorithm>
#include <inttypes.h>
#include <cassert>
#include <chrono>

#include "search.h"
#include "board.h"
#include "move.h"
#include "evaluation.h"
#include "thread.h"
#include "tt.h"

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

template <NodeType nodeType>
Eval qsearch(Board* board, SearchStack* stack, Eval alpha, Eval beta) {
    constexpr bool pvNode = nodeType == PV_NODE;

    assert(alpha >= -EVAL_INFINITE && alpha < beta && beta <= EVAL_INFINITE);

    // Check for stop
    if (board->stopSearching || stack->ply >= MAX_PLY)
        return (stack->ply >= MAX_PLY) ? evaluate(board) : 0;

    BoardStack boardStack;
    Move pv[MAX_PLY + 1] = { MOVE_NONE };
    Eval bestValue;

    stack->nodes = 0;
    (stack + 1)->ply = stack->ply + 1;
    (stack + 1)->nodes = 0;

    bestValue = evaluate(board);
    if (bestValue >= beta)
        return beta;
    if (alpha < bestValue)
        alpha = bestValue;

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
    MoveGen movegen(board, true);
    Move move;
    int moveCount = 0;
    while ((move = movegen.nextMove()) != MOVE_NONE) {

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
    //     if (isInCheck(board, board->stm)) {
    //         return matedIn(stack->ply); // Checkmate
    //     }
    //     return 0; // Stalemate
    // }

    return bestValue;
}

template <NodeType nt>
Eval search(Board* board, SearchStack* stack, int depth, Eval alpha, Eval beta) {
    constexpr bool rootNode = nt == ROOT_NODE;
    constexpr bool pvNode = nt == PV_NODE || nt == ROOT_NODE;
    constexpr NodeType nodeType = nt == ROOT_NODE ? PV_NODE : NON_PV_NODE;

    assert(-EVAL_INFINITE <= alpha && alpha < beta && beta <= EVAL_INFINITE);

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
    Move ttMove = MOVE_NONE;
    TTEntry* ttEntry = TT.probe(board->stack->hash, &ttHit);
    if (ttHit) {
        // bestValue = ttEntry->value;
        ttMove = ttEntry->bestMove;
    }

    // Reverse futility pruning
    Eval eval = evaluate(board);
    if (depth < 7 && eval - (70 * depth) >= beta) return eval;

    // Moves loop
    MoveGen movegen(board, ttMove, stack->killers);
    Move move;
    int moveCount = 0;
    while ((move = movegen.nextMove()) != MOVE_NONE) {

        if (!isLegal(board, move))
            continue;
        
        if (pvNode)
            (stack + 1)->pv = nullptr;

        moveCount++;
        stack->nodes++;
        doMove(board, &boardStack, move);

        Eval value;
        int newDepth = depth - 1;
        
        // Very basic LMR: Late moves are being searched with less depth
        // Check if the move can exceed alpha
        if (moveCount > 10) {
            value = -search<NON_PV_NODE>(board, stack + 1, newDepth - 1, -(alpha + 1), -alpha);

            if (value > alpha)
                value = -search<NON_PV_NODE>(board, stack + 1, newDepth, -(alpha + 1), -alpha);
        }
        else {
            value = -search<NON_PV_NODE>(board, stack + 1, newDepth, -(alpha + 1), -alpha);
        }

        // PV moves will be researched at full depth if good enough
        if (pvNode && (moveCount == 1 || value > alpha)) {
            // Set up pv for the next search
            (stack + 1)->pv = pv;
            (stack + 1)->pv[0] = MOVE_NONE;
            value = -search<PV_NODE>(board, stack + 1, newDepth, -beta, -alpha);
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
                
                if (move != stack->killers[0]) {
                    stack->killers[1] = stack->killers[0];
                    stack->killers[0] = move;
                }

                if (bestValue >= beta)
                    break;
            }
        }

    }

    if (moveCount == 0) {
        if (isInCheck(board, board->stm)) {
            return matedIn(stack->ply); // Checkmate
        }
        return 0; // Stalemate
    }

    // Insert into TT
    ttEntry->update(board->stack->hash, bestMove, depth, bestValue);

    assert(bestValue > -EVAL_INFINITE && bestValue < EVAL_INFINITE);

    return bestValue;
}

void Thread::tsearch() {
    TT.clear();
    nodesSearched = 0;

    int maxDepth = searchParameters.depth == 0 ? MAX_PLY : searchParameters.depth;

    Move bestMove = MOVE_NONE;

    for (int depth = 1; depth <= maxDepth; depth++) {
        SearchStack stack[MAX_PLY];
        Move pv[MAX_PLY + 1];
        pv[0] = MOVE_NONE;
        stack->pv = pv;
        stack->ply = 0;
        stack->killers[0] = stack->killers[1] = MOVE_NONE;

        // Search
        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
        Eval value = search<ROOT_NODE>(&rootBoard, stack, depth, -EVAL_INFINITE, EVAL_INFINITE);
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