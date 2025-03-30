#include "ActionNode.h"
#include <sstream>
#include <stdexcept>

namespace PokerSolver {

// -----------------------------------------------------------------------------
// Constructor
ActionNode::ActionNode(GameRound round,
                       double pot,
                       std::shared_ptr<GameTreeNode> parent,
                       int player,
                       const std::vector<GameActions>& actions)
    : GameTreeNode(round, pot, parent),
      player_(player),
      actions_(actions)
{
    // Resize the trainable vector to have the same size as the actions.
    // Initially, no trainable is assigned.
    trainables_.resize(actions_.size(), nullptr);
}

// -----------------------------------------------------------------------------
// Destructor
ActionNode::~ActionNode() = default;

// -----------------------------------------------------------------------------
// Override: return the node type as Action.
NodeType ActionNode::type() const {
    return NodeType::Action;
}

// -----------------------------------------------------------------------------
// Override: provide a human-readable string for this node type.
std::string ActionNode::nodeTypeToString() const {
    return "Action";
}

// -----------------------------------------------------------------------------
// Accessor for the acting player's index.
int ActionNode::player() const {
    return player_;
}

// -----------------------------------------------------------------------------
// Accessor for the available game actions.
const std::vector<GameActions>& ActionNode::actions() const {
    return actions_;
}

// -----------------------------------------------------------------------------
// Accessor for child nodes.
const std::vector<std::shared_ptr<GameTreeNode>>& ActionNode::children() const {
    return children_;
}

// -----------------------------------------------------------------------------
// Returns the Trainable instance associated with a given action index.
// Throws an exception if the index is out-of-range.
std::shared_ptr<Trainable> ActionNode::getTrainable(int actionIndex) const {
    if (actionIndex < 0 || actionIndex >= static_cast<int>(trainables_.size())) {
        throw std::out_of_range("ActionNode::getTrainable: action index out of range");
    }
    return trainables_[actionIndex];
}

// -----------------------------------------------------------------------------
// Adds a child node to the children vector.
void ActionNode::addChild(const std::shared_ptr<GameTreeNode>& child) {
    children_.push_back(child);
}

// -----------------------------------------------------------------------------
// Associates a Trainable instance with an action index.
// Throws an exception if the index is invalid.
void ActionNode::setTrainable(int actionIndex, std::shared_ptr<Trainable> trainable) {
    if (actionIndex < 0 || actionIndex >= static_cast<int>(trainables_.size())) {
        throw std::out_of_range("ActionNode::setTrainable: action index out of range");
    }
    trainables_[actionIndex] = trainable;
}

} // namespace PokerSolver
