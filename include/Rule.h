#ifndef POKERSOLVER_RULE_H
#define POKERSOLVER_RULE_H

#include "Deck.h"                        // Our Deck implementation.
#include "StreetSetting.h"               // Settings per street (flop/turn/river).
#include "GameTreeBuildingSettings.h"    // Rules for constructing the game tree.
#include "GameTreeNode.h"                // For the GameRound enum.
#include <stdexcept>
#include <string>

namespace PokerSolver {

// Rule encapsulates all the parameters that define a particular game configuration.
// These include the deck, pre-flop commitments (for out-of-position (OOP) and in-position (IP) players),
// current round, raise limits, blinds, stack size, game tree building settings, and an all-in threshold.
class Rule {
public:
    // Constructor: all parameters must be provided.
    Rule(Deck deck,
         float oopCommit,
         float ipCommit,
         GameRound currentRound,
         int raiseLimit,
         float smallBlind,
         float bigBlind,
         float stack,
         const GameTreeBuildingSettings& buildSettings,
         float allinThreshold);

    // --- Accessors ---
    Deck deck() const;
    float oopCommit() const;
    float ipCommit() const;
    GameRound currentRound() const;
    int raiseLimit() const;
    float smallBlind() const;
    float bigBlind() const;
    float stack() const;
    GameTreeBuildingSettings buildSettings() const;
    float allinThreshold() const;
    float initialEffectiveStack() const;

    // --- Utility Methods ---
    // Computes the current pot. For simplicity, we assume the pot is the sum of the blinds and both players' commitments.
    float getPot() const;

    // Returns the commitment amount for a given player.
    // Convention: player 0 is OOP and player 1 is IP.
    float getCommit(int player) const;

private:
    Deck deck_;
    float oopCommit_;
    float ipCommit_;
    GameRound currentRound_;
    int raiseLimit_;
    float smallBlind_;
    float bigBlind_;
    float stack_;
    GameTreeBuildingSettings buildSettings_;
    float allinThreshold_;
    float initialEffectiveStack_; // Could be computed as the effective stack after initial commits.
};

} // namespace PokerSolver

#endif // POKERSOLVER_RULE_H
