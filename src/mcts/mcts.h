#include <vector>
#include <map>

#include "../board.h"

class MCTSNode {

public:
    Board board;
    History* history;
    Eval staticEval;

    double parentProbability;
    Move parentMove;
    Piece parentMovedPiece;
    MCTSNode* parent;
    std::vector<MCTSNode> children;

    int64_t numVisits;
    double totalEval;
    bool terminal;

    std::vector<std::pair<Move, double>> remainingMoves;

    MCTSNode(Board* prevBoard, History* history, Move move, double probability, MCTSNode* parent);
    MCTSNode(Board* rootBoard, History* history);

    std::vector<std::pair<Move, double>> generateMoves();
    double q();
    MCTSNode* expand();
    double rollout();
    void backpropagate(double result);
    bool fullyExpanded();
    MCTSNode* bestChild(double c);
    MCTSNode* treePolicy();
    MCTSNode* bestMove(int numRollouts);

};
