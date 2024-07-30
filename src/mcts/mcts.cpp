#include <cmath>

#include "mcts.h"
#include "../move.h"
#include "../tt.h"
#include "../thread.h"
#include "../uci.h"

void Thread::mctsSearch() {
    MCTSNode root(&rootBoard, &history);
    MCTSNode* bestChild = root.bestMove(searchParameters->rollouts);
    Move move = bestChild->parentMove;

    // for (MCTSNode child : root.children) {
    //     std::cout << moveToString(child.parentMove, rootBoard.chess960) << ": Q(" << child.q() << "), N(" << child.numVisits << ")" << std::endl;
    // }

    std::cout << "info string mcts bestmove " << moveToString(move, rootBoard.chess960) << " score " << int(-400.0 * std::log(1.0 / std::clamp(bestChild->q(), 0.0, 1.0) - 1.0)) << std::endl;
}

MCTSNode::MCTSNode(Board* prevBoard, History* history, Move move, double probability, MCTSNode* parent) :
    boardStack(*prevBoard->stack),
    board(*prevBoard),
    history(history),
    staticEval(EVAL_NONE),
    parentProbability(probability),
    parentMove(move),
    parentMovedPiece(prevBoard->pieces[moveOrigin(move)]),
    parent(parent),
    children(),
    numVisits(0),
    totalEval(0),
    terminal(false),
    remainingMoves() {
    board.doMove(&boardStack, move, board.hashAfter(move), nullptr);

    if (board.isDraw()) {
        terminal = true;
    }
    else {
        remainingMoves = generateMoves();
        terminal = remainingMoves.empty();
    }
}

MCTSNode::MCTSNode(Board* rootBoard, History* history) :
    boardStack(*rootBoard->stack),
    board(*rootBoard),
    history(history),
    staticEval(EVAL_NONE),
    parentProbability(0),
    parentMove(MOVE_NONE),
    parentMovedPiece(Piece::NONE),
    parent(nullptr),
    children(),
    numVisits(0),
    totalEval(0),
    terminal(false),
    remainingMoves() {
    if (board.isDraw()) {
        terminal = true;
    }
    else {
        remainingMoves = generateMoves();
        terminal = remainingMoves.empty();

        // for (auto scoredMove : remainingMoves) {
        //     std::cout << moveToString(scoredMove.first, false) << ": " << scoredMove.second << std::endl;
        // }
    }
}

std::vector<std::pair<Move, double>> MCTSNode::generateMoves() {
    assert(history != nullptr);

    // Probe the TT
    bool ttHit;
    TTEntry* ttEntry = TT.probe(board.stack->hash, &ttHit);
    if (ttHit) {
        staticEval = ttEntry->getEval();
    }
    else {
        UCI::nnue.reset(&board);
        staticEval = UCI::nnue.evaluate(&board);
    }

    // Collect all legal moves, sorted by strength
    MoveGen movegen(&board, history, this, ttHit ? ttEntry->getMove() : MOVE_NONE);
    Move move;
    std::vector<Move> moves;
    while ((move = movegen.nextMove()) != MOVE_NONE) {
        if (!board.isLegal(move))
            continue;
        moves.push_back(move);
    }

    // Rescore with softmax
    std::vector<std::pair<Move, double>> result;
    double sum = 0;
    for (size_t i = 0; i < moves.size(); i++) {
        double score = moves.size() - i;
        double softmax = std::exp(score);
        sum += softmax;
        result.push_back(std::make_pair(moves[i], softmax));
    }
    for (size_t i = 0; i < moves.size(); i++) {
        result[i].second /= sum;
    }

    return result;
}

double MCTSNode::q() {
    assert(numVisits > 0);
    return totalEval / static_cast<double>(numVisits);
}

MCTSNode* MCTSNode::expand() {
    // Choose move to play randomly, weighted by the softmax scores of the moves
    double r = static_cast<double>(std::rand()) / RAND_MAX;
    double sum = 0;
    std::pair<Move, double> scoredMove = remainingMoves.back();
    int idx = remainingMoves.size() - 1;
    for (size_t i = 0; i < remainingMoves.size(); i++) {
        sum += remainingMoves[i].second;
        if (r <= sum) {
            idx = i;
            scoredMove = remainingMoves[i];
            break;
        }
    }

    MCTSNode child(&board, history, scoredMove.first, scoredMove.second, this);
    children.push_back(child);

    remainingMoves.erase(remainingMoves.begin() + idx);

    return &children.back();
}

double MCTSNode::rollout() {
    // assert(children.size() == 0);

    if (terminal) {
        if (board.isDraw())
            return 0;
        return board.stack->checkers ? (board.stm == Color::WHITE ? -2 : 2) : 0;
    }

    assert(!terminal);

    double x = static_cast<double>(-staticEval) / 400.0;
    return 1.0 / (1.0 + std::exp(-x));
}

void MCTSNode::backpropagate(double result) {
    numVisits++;
    totalEval += result;
    if (parent)
        parent->backpropagate(-result + 1.0); // 0.6 = 0.5 + 0.1 => 0.1 = result - 2 * (result - 0.5) = result - 2 * result + 1.0 = -result + 1.0
}

bool MCTSNode::fullyExpanded() {
    return remainingMoves.empty();
}

MCTSNode* MCTSNode::bestChild(double c) {
    assert(!children.empty());
    assert(numVisits > 0);

    MCTSNode* best = nullptr;
    double bestChildWeight = 0;

    for (MCTSNode& child : children) {
        double weight = child.q() + c * child.parentProbability * std::sqrt(static_cast<double>(numVisits)) / (1.0 + static_cast<double>(child.numVisits));
        if (!best || weight > bestChildWeight) {
            best = &child;
            bestChildWeight = weight;
        }
    }

    return best;
}

MCTSNode* MCTSNode::treePolicy() {
    MCTSNode* node = this;

    while (!node->terminal) {
        if (!node->fullyExpanded())
            return node->expand();
        else
            node = node->bestChild(1.4);
    }

    return node;
}

MCTSNode* MCTSNode::bestMove(int numRollouts) {
    for (int i = 0; i < numRollouts; i++) {
        MCTSNode* leafNode = treePolicy(); // Selection (history-PUCT-based) and Expansion (history-softmax-probability-based)
        double result = leafNode->rollout(); // Simulation (staticEval-based)
        leafNode->backpropagate(result); // Backpropagation
    }

    return bestChild(0);
}