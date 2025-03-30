#include "Rule.h"

namespace PokerSolver {

// -----------------------------------------------------------------------------
// Constructor
Rule::Rule(Deck deck,
           float oopCommit,
           float ipCommit,
           GameRound currentRound,
           int raiseLimit,
           float smallBlind,
           float bigBlind,
           float stack,
           const GameTreeBuildingSettings& buildSettings,
           float allinThreshold)
    : deck_(std::move(deck)),
      oopCommit_(oopCommit),
      ipCommit_(ipCommit),
      currentRound_(currentRound),
      raiseLimit_(raiseLimit),
      smallBlind_(smallBlind),
      bigBlind_(bigBlind),
      stack_(stack),
      buildSettings_(buildSettings),
      allinThreshold_(allinThreshold)
{
    // For this implementation, we assume the initial effective stack is the full stack.
    // Alternatively, it might be computed as: stack_ - (oopCommit_ + ipCommit_)
    initialEffectiveStack_ = stack_;
}

// -----------------------------------------------------------------------------
// Accessor Implementations

Deck Rule::deck() const {
    return deck_;
}

float Rule::oopCommit() const {
    return oopCommit_;
}

float Rule::ipCommit() const {
    return ipCommit_;
}

GameRound Rule::currentRound() const {
    return currentRound_;
}

int Rule::raiseLimit() const {
    return raiseLimit_;
}

float Rule::smallBlind() const {
    return smallBlind_;
}

float Rule::bigBlind() const {
    return bigBlind_;
}

float Rule::stack() const {
    return stack_;
}

GameTreeBuildingSettings Rule::buildSettings() const {
    return buildSettings_;
}

float Rule::allinThreshold() const {
    return allinThreshold_;
}

float Rule::initialEffectiveStack() const {
    return initialEffectiveStack_;
}

// -----------------------------------------------------------------------------
// Utility Methods

float Rule::getPot() const {
    // In this simplified example, we define the pot as the sum of small blind, big blind,
    // and both players' initial commitments.
    return smallBlind_ + bigBlind_ + oopCommit_ + ipCommit_;
}

float Rule::getCommit(int player) const {
    // Convention: player 0 is out-of-position (OOP), player 1 is in-position (IP)
    if (player == 0) {
        return oopCommit_;
    } else if (player == 1) {
        return ipCommit_;
    } else {
        throw std::invalid_argument("Rule::getCommit: Invalid player index (must be 0 or 1)");
    }
}

} // namespace PokerSolver
