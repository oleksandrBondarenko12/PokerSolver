#include "ChanceNode.h"
#include <sstream>

namespace PokerSolver {

// Constructor: initializes the round, pot, parent, cards, and the donk flag.
// The depth is automatically computed in the base class constructor.
ChanceNode::ChanceNode(GameRound round, double pot,
                       std::shared_ptr<GameTreeNode> parent,
                       const std::vector<Card>& cards,
                       bool donk)
    : GameTreeNode(round, pot, parent),
      cards_(cards),
      donk_(donk)
{
    // Initially, no child node is set.
    child_ = nullptr;
}

// Returns a constant reference to the cards vector.
const std::vector<Card>& ChanceNode::cards() const {
    return cards_;
}

// Returns the child node (if any) as a shared_ptr.
std::shared_ptr<GameTreeNode> ChanceNode::child() const {
    return child_;
}

// Sets the child node pointer.
void ChanceNode::setChild(std::shared_ptr<GameTreeNode> child) {
    child_ = child;
}

// Returns whether this chance node represents a donk scenario.
bool ChanceNode::isDonk() const noexcept {
    return donk_;
}

// The type() method returns the specific node type for a chance node.
NodeType ChanceNode::type() const {
    return NodeType::Chance;
}

// Returns a human-readable string representing the node type.
std::string ChanceNode::nodeTypeToString() const {
    return "Chance";
}

} // namespace PokerSolver
