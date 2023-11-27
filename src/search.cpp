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

uint64_t perftInternal(Board* board, int depth) {
    if (depth == 0) return C64(1);

    BoardStack stack;

    Move moves[MAX_MOVES] = { MOVE_NONE };
    int moveCount = 0;
    generateMoves(board, moves, &moveCount);

    uint64_t nodes = 0;
    for (int i = 0; i < moveCount; i++) {
        Move move = moves[i];

        doMove(board, &stack, move);

        // This move was illegal, we remain in check after
        if (isInCheck(board, 1 - board->stm)) {
            undoMove(board, move);
            continue;
        }

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

        doMove(board, &stack, move);

        // This move was illegal, we remain in check after
        if (isInCheck(board, 1 - board->stm)) {
            undoMove(board, move);
            continue;
        }

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

    assert(alpha >= -EVAL_INFINITE && alpha < beta && beta <= EVAL_INFINITE);

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

    // Generate moves
    Move moves[MAX_MOVES] = { MOVE_NONE };
    int moveCount = 0, skippedMoves = 0;
    generateMoves(board, moves, &moveCount, true);

    alpha = std::max((int)alpha, (int)matedIn(stack->ply));
    beta = std::min((int)beta, (int)mateIn(stack->ply + 1));
    if (alpha >= beta)
        return alpha;

    // Moves loop
    for (int i = 0; i < moveCount; i++) {
        Move move = moves[i];

        doMove(board, &boardStack, move);
        stack->nodes++;

        // This move was illegal, we remain in check after
        if (isInCheck(board, 1 - board->stm)) {
            undoMove(board, move);
            skippedMoves++;
            continue;
        }

        // Set up pv for the next search
        (stack + 1)->pv = pv;
        (stack + 1)->pv[0] = MOVE_NONE;

        Eval value = -qsearch<PV_NODE>(board, stack + 1, -beta, -alpha);
        undoMove(board, move);
        assert(value > -EVAL_INFINITE && value < EVAL_INFINITE);

        stack->nodes += (stack + 1)->nodes;

        if (value > bestValue) {
            bestValue = value;

            if (value > alpha) {
                alpha = value;

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

template <NodeType nodeType>
Eval search(Board* board, SearchStack* stack, int depth, Eval alpha, Eval beta) {

    assert(-EVAL_INFINITE <= alpha && alpha < beta && beta <= EVAL_INFINITE);

    if (depth <= 0) return qsearch<PV_NODE>(board, stack, alpha, beta);

    BoardStack boardStack;
    Move pv[MAX_PLY + 1] = { MOVE_NONE };
    Eval bestValue;
    bool rootNode = nodeType == ROOT_NODE;

    stack->nodes = 0;
    (stack + 1)->ply = stack->ply + 1;
    (stack + 1)->nodes = 0;

    // Generate moves
    Move moves[MAX_MOVES] = { MOVE_NONE };
    int moveCount = 0, skippedMoves = 0;
    generateMoves(board, moves, &moveCount);

    if (!rootNode) {
        // Mate distance pruning
        alpha = std::max((int)alpha, (int)matedIn(stack->ply));
        beta = std::min((int)beta, (int)mateIn(stack->ply + 1));
        if (alpha >= beta)
            return alpha;
    }

    // Moves loop
    bestValue = -EVAL_INFINITE;
    for (int i = 0; i < moveCount; i++) {
        Move move = moves[i];

        doMove(board, &boardStack, move);
        stack->nodes++;

        // This move was illegal, we remain in check after
        if (isInCheck(board, 1 - board->stm)) {
            undoMove(board, move);
            skippedMoves++;
            continue;
        }

        // Set up pv for the next search
        (stack + 1)->pv = pv;
        (stack + 1)->pv[0] = MOVE_NONE;

        Eval value = -search<PV_NODE>(board, stack + 1, depth - 1, -beta, -alpha);
        undoMove(board, move);
        assert(value > -EVAL_INFINITE && value < EVAL_INFINITE);

        stack->nodes += (stack + 1)->nodes;

        // if (rootNode) {
        //     std::cout << "move " << moveToString(move) << " eval " << formatEval(value) << std::endl;
        // }

        if (value > bestValue) {
            bestValue = value;

            if (value > alpha) {
                alpha = value;

                updatePv(stack->pv, move, (stack + 1)->pv);

                if (bestValue >= beta)
                    break;
            }
        }

    }

    if (moveCount == skippedMoves) {
        if (isInCheck(board, board->stm)) {
            return matedIn(stack->ply); // Checkmate
        }
        return 0; // Stalemate
    }

    assert(bestValue > -EVAL_INFINITE && bestValue < EVAL_INFINITE);

    return bestValue;
}

void Thread::tsearch() {
    int maxDepth = searchParameters.depth == 0 ? MAX_PLY : searchParameters.depth;

    Move bestMove = MOVE_NONE;

    for (int depth = 1; depth <= maxDepth; depth++) {
        SearchStack stack[MAX_PLY];
        Move pv[MAX_PLY + 1];
        stack->pv = pv;
        stack->ply = 1;

        // Search
        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
        Eval value = search<ROOT_NODE>(&rootBoard, stack, depth, -EVAL_INFINITE, EVAL_INFINITE);
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

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

        if (ms >= 2000) break; // For now, search until the longest search exceeds 2s
    }

    std::cout << "bestmove " << moveToString(bestMove) << std::endl;
}