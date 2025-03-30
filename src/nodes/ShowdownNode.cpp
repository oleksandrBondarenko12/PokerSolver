#include "ShowdownNode.h"
#include <sstream>
#include <stdexcept>

namespace PokerSolver {

// -----------------------------------------------------------------------------
// Constructor

ShowdownNode::ShowdownNode(const std::vector<double>& tiePayoffs,
                           const std::vector<std::vector<double>>& playerPayoffs,
                           GameRound round,
                           double pot,
                           std::shared_ptr<GameTreeNode> parent)
    : GameTreeNode(round, pot, parent),
      tiePayoffs_(tiePayoffs),
      playerPayoffs_(playerPayoffs)
{
    // Optionally, add validation here:
    // For example, ensure that the number of tie payoffs matches the number of players
    // or that playerPayoffs_ is well–formed.
}

// -----------------------------------------------------------------------------
// Implementation of pure virtual functions

NodeType ShowdownNode::type() const {
    return NodeType::Showdown;
}

std::string ShowdownNode::nodeTypeToString() const {
    return "Showdown";
}

// -----------------------------------------------------------------------------
// Payoff Accessors

std::vector<double> ShowdownNode::getPayoffs(ShowdownResult result, int winner) const {
    if (result == ShowdownResult::Tie) {
        return tiePayoffs_;
    }
    else { // NotTie
        if (winner < 0 || static_cast<size_t>(winner) >= playerPayoffs_.size()) {
            throw std::out_of_range("ShowdownNode::getPayoffs: winner index out of range");
        }
        return playerPayoffs_[winner];
    }
}

double ShowdownNode::getPayoff(ShowdownResult result, int winner, int player) const {
    if (result == ShowdownResult::Tie) {
        if (player < 0 || static_cast<size_t>(player) >= tiePayoffs_.size()) {
            throw std::out_of_range("ShowdownNode::getPayoff: player index out of range (tie)");
        }
        return tiePayoffs_[player];
    }
    else { // NotTie
        if (winner < 0 || static_cast<size_t>(winner) >= playerPayoffs_.size()) {
            throw std::out_of_range("ShowdownNode::getPayoff: winner index out of range (not tie)");
        }
        const std::vector<double>& payoffs = playerPayoffs_[winner];
        if (player < 0 || static_cast<size_t>(player) >= payoffs.size()) {
            throw std::out_of_range("ShowdownNode::getPayoff: player index out of range in winner's payoffs");
        }
        return payoffs[player];
    }
}

} // namespace PokerSolver
