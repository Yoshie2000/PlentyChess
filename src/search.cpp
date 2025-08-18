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
#include "time.h"
#include "spsa.h"
#include "nnue.h"
#include "uci.h"
#include "fathom/src/tbprobe.h"
#include "zobrist.h"

uint64_t perftInternal(Board& board, NNUE* nnue, int16_t depth) {
    if (depth == 0) return 1;

    Move moves[MAX_MOVES] = { MOVE_NONE };
    int moveCount = 0;
    generateMoves(&board, moves, &moveCount);

    uint64_t nodes = 0;
    for (int i = 0; i < moveCount; i++) {
        Move move = moves[i];

        if (!board.isLegal(move))
            continue;

        Board boardCopy = board;
        boardCopy.doMove(move, boardCopy.hashAfter(move), nnue);
        uint64_t subNodes = perftInternal(boardCopy, nnue, depth - 1);
        UCI::nnue.decrementAccumulator();

        nodes += subNodes;
    }
    return nodes;
}

uint64_t perft(Board& board, int16_t depth) {
    clock_t begin = clock();
    UCI::nnue.reset(&board);

    Move moves[MAX_MOVES] = { MOVE_NONE };
    int moveCount = 0;
    generateMoves(&board, moves, &moveCount);

    uint64_t nodes = 0;
    for (int i = 0; i < moveCount; i++) {
        Move move = moves[i];

        if (!board.isLegal(move))
            continue;

        Board boardCopy = board;
        boardCopy.doMove(move, boardCopy.hashAfter(move), &UCI::nnue);
        uint64_t subNodes = perftInternal(boardCopy, &UCI::nnue, depth - 1);
        UCI::nnue.decrementAccumulator();

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

Board* Worker::doMove(Board* board, uint64_t newHash, Move move) {
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

    uint64_t* compareHash = &boardHistory[boardHistory.size() - 2];

    int j = 0;
    for (int i = 3; i <= maxPlyOffset; i += 2) {
        compareHash -= 2;

        uint64_t moveHash = board->hashes.hash ^ *compareHash;
        if ((j = H1(moveHash), CUCKOO_HASHES[j] == moveHash) || (j = H2(moveHash), CUCKOO_HASHES[j] == moveHash)) {
            Move move = CUCKOO_MOVES[j];
            Square origin = moveOrigin(move);
            Square target = moveTarget(move);

            if (BB::BETWEEN[origin][target] & (board->byColor[Color::WHITE] | board->byColor[Color::BLACK]))
                continue;

            if (ply > i)
                return true;

            Square pieceSquare = board->pieces[origin] == Piece::NONE ? target : origin;
            Color pieceColor = (board->byColor[Color::WHITE] & bitboard(pieceSquare)) ? Color::WHITE : Color::BLACK;
            if (pieceColor != board->stm)
                continue;

            // Check for 2-fold repetition
            uint64_t* compareHash2 = compareHash;
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
    uint64_t* compareHash = &boardHistory[boardHistory.size() - 3];

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
        Move moves[MAX_MOVES] = { MOVE_NONE };
        int moveCount = 0;
        int legalMoveCount = 0;
        generateMoves(board, moves, &moveCount);
        for (int i = 0; i < moveCount; i++) {
            Move move = moves[i];
            if (!board->isLegal(move))
                continue;
            legalMoveCount++;
        }

        return legalMoveCount > 0;
    }

    // Otherwise, no draw
    return false;
}

Move tbProbeMoveRoot(unsigned result) {
    Square origin = TB_GET_FROM(result);
    Square target = TB_GET_TO(result);
    int promotion = TB_GET_PROMOTES(result);

    if (promotion)
        return createMove(origin, target) | MOVE_PROMOTION | ((promotion - 1) << 14);

    if (TB_GET_EP(result))
        return createMove(origin, target) | MOVE_ENPASSANT;

    return createMove(origin, target);
}

void Worker::startSearching(bool datagen) {
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

    int maxDepth = searchParameters.depth == 0 ? MAX_PLY - 1 : std::min<int16_t>(MAX_PLY - 1, searchParameters.depth);
    // TODO
}

void Worker::printUCI(Worker* thread, int multiPvCount) {
    int64_t ms = 1; // TODO
    int64_t nodes = threadPool->nodesSearched();
    int64_t nps = ms == 0 ? 0 : nodes / ((double)ms / 1000);

    for (int rootMoveIdx = 0; rootMoveIdx < multiPvCount; rootMoveIdx++) {
        RootMove rootMove = thread->rootMoves[rootMoveIdx];
        std::cout << "info depth " << rootMove.depth << " seldepth " << rootMove.selDepth << " score " << formatEval(rootMove.value) << " multipv " << (rootMoveIdx + 1) << " nodes " << nodes << " tbhits " << threadPool->tbhits() << " time " << ms << " nps " << nps << " hashfull " << 0 << " pv ";

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