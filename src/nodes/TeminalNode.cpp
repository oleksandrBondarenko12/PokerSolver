#include "TerminalNode.h"
#include <sstream>

namespace PokerSolver {

TerminalNode::TerminalNode(GameRound round, double pot, const std::vector<double>& payoffs, int winner,
                           std::shared_ptr<GameTreeNode> parent)
    : GameTreeNode(round, pot, parent),
      payoffs_(payoffs),
      winner_(winner)
{
}

NodeType TerminalNode::type() const {
    return NodeType::Terminal;
}

std::string TerminalNode::nodeTypeToString() const {
    return "Terminal";
}

const std::vector<double>& TerminalNode::payoffs() const {
    return payoffs_;
}

int TerminalNode::winner() const {
    return winner_;
}

std::string TerminalNode::toString() const {
    std::ostringstream oss;
    oss << "TerminalNode[Round: " << gameRoundToString(round())
        << ", Pot: " << pot()
        << ", Winner: " << (winner_ >= 0 ? std::to_string(winner_) : "N/A")
        << ", Payoffs: [";
    for (std::size_t i = 0; i < payoffs_.size(); ++i) {
        oss << payoffs_[i];
        if (i < payoffs_.size() - 1)
            oss << ", ";
    }
    oss << "], Depth: " << depth() << "]";
    return oss.str();
}

} // namespace PokerSolver
