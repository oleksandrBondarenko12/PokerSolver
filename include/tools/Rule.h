#ifndef POKER_SOLVER_CONFIG_RULE_H_
#define POKER_SOLVER_CONFIG_RULE_H_

#include "Deck.h"                          // For Deck
#include "tools/GameTreeBuildingSettings.h" // For GameTreeBuildingSettings
#include "nodes/GameTreeNode.h"                // For GameRound
#include <vector>
#include <cstddef> // For size_t
#include <stdexcept> // For exceptions
#include <sstream>   // For error messages

namespace poker_solver {
namespace config {

// Defines the static rules and initial state for a specific poker scenario.
// Used primarily during game tree construction.
class Rule {
 public:
  // Constructor to initialize the game rules and state.
  // Args:
  //   deck: The deck object to use (copied).
  //   initial_oop_commit: Amount initially committed by OOP (Player 1).
  //   initial_ip_commit: Amount initially committed by IP (Player 0).
  //   starting_round: The GameRound the scenario begins at.
  //   raise_limit_per_street: Maximum number of raises allowed per street.
  //   small_blind: The small blind amount.
  //   big_blind: The big blind amount.
  //   initial_effective_stack: The starting effective stack size for both players.
  //   build_settings: Betting structure configurations.
  //   all_in_threshold_ratio: Ratio of the stack (0.0 to 1.0) above which a
  //                           bet/raise is considered effectively all-in.
  Rule(const core::Deck& deck,
       double initial_oop_commit,
       double initial_ip_commit,
       core::GameRound starting_round,
       int raise_limit_per_street,
       double small_blind,
       double big_blind,
       double initial_effective_stack,
       const GameTreeBuildingSettings& build_settings,
       double all_in_threshold_ratio = 0.98); // Default threshold

  // --- Accessors ---

  const core::Deck& GetDeck() const { return deck_; }
  double GetInitialOopCommit() const { return initial_oop_commit_; }
  double GetInitialIpCommit() const { return initial_ip_commit_; }
  core::GameRound GetStartingRound() const { return starting_round_; }
  int GetRaiseLimitPerStreet() const { return raise_limit_per_street_; }
  double GetSmallBlind() const { return small_blind_; }
  double GetBigBlind() const { return big_blind_; }
  double GetInitialEffectiveStack() const { return initial_effective_stack_; }
  const GameTreeBuildingSettings& GetBuildSettings() const { return build_settings_; }
  double GetAllInThresholdRatio() const { return all_in_threshold_ratio_; }

  // Calculates the initial pot size based on commitments.
  double GetInitialPot() const;

  // Gets the initial commitment for a specific player.
  // Args:
  //   player_index: 0 for IP, 1 for OOP.
  // Returns:
  //   The initial committed amount for that player.
  // Throws:
  //   std::out_of_range if player_index is invalid.
  double GetInitialCommitment(size_t player_index) const;

  // --- Modifiers (Used internally by GameTree builder) ---
  // NOTE: These modify the state. If Rule needs to be immutable after
  // creation, the tree building logic should pass commitment state differently.
  // These are needed because GameTree::BuildBranch passes Rule by value and modifies the copy.
  void SetInitialOopCommit(double amount) { initial_oop_commit_ = amount; }
  void SetInitialIpCommit(double amount) { initial_ip_commit_ = amount; }


 private:
  // --- Member Variables ---
  core::Deck deck_; // Copy of the deck for this specific scenario
  double initial_oop_commit_;
  double initial_ip_commit_;
  core::GameRound starting_round_;
  int raise_limit_per_street_;
  double small_blind_;
  double big_blind_;
  double initial_effective_stack_;
  GameTreeBuildingSettings build_settings_;
  double all_in_threshold_ratio_;

  // Assuming 2 players: 0 = IP, 1 = OOP (can be made more flexible if needed)
  const std::vector<size_t> players_ = {0, 1};
};

} // namespace config
} // namespace poker_solver

#endif // POKER_SOLVER_CONFIG_RULE_H_
