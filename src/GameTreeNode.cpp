#include "GameTreeNode.h"
#include <sstream>
#include <cassert>

namespace PokerSolver {

// -----------------------------------------------------------------------------
// Constructors and Destructor

GameTreeNode::GameTreeNode()
    : round_(GameRound::Preflop), pot_(0.0), depth_(0)
{
    // No parent given → depth is 0
}

GameTreeNode::GameTreeNode(GameRound round, double pot, std::shared_ptr<GameTreeNode> parent)
    : round_(round), pot_(pot), parent_(parent), depth_(parent ? parent->depth() + 1 : 0)
{
}

GameTreeNode::~GameTreeNode() = default;

// -----------------------------------------------------------------------------
// Accessor Implementations

GameRound GameTreeNode::round() const {
    return round_;
}

double GameTreeNode::pot() const {
    return pot_;
}

std::shared_ptr<GameTreeNode> GameTreeNode::parent() const {
    return parent_.lock();
}

int GameTreeNode::depth() const {
    return depth_;
}

// -----------------------------------------------------------------------------
// Mutator Implementations

void GameTreeNode::setParent(std::shared_ptr<GameTreeNode> parent) {
    parent_ = parent;
    depth_ = parent ? parent->depth() + 1 : 0;
}

void GameTreeNode::setPot(double pot) {
    pot_ = pot;
}

void GameTreeNode::setRound(GameRound round) {
    round_ = round;
}

// -----------------------------------------------------------------------------
// Utility Method Implementations

std::string GameTreeNode::toString() const {
    std::ostringstream oss;
    oss << "Node[Type: " << nodeTypeToString()
        << ", Round: " << gameRoundToString(round_)
        << ", Pot: " << pot_
        << ", Depth: " << depth_ << "]";
    return oss.str();
}

std::string GameTreeNode::printHistory() const {
    if (auto p = parent())
        return p->printHistory() + " -> " + toString();
    else
        return toString();
}

// -----------------------------------------------------------------------------
// Static Helper Functions

std::string GameTreeNode::gameRoundToString(GameRound round) {
    switch (round) {
        case GameRound::Preflop: return "Preflop";
        case GameRound::Flop:    return "Flop";
        case GameRound::Turn:    return "Turn";
        case GameRound::River:   return "River";
        default:                 return "Unknown";
    }
}

} // namespace PokerSolver
