#ifndef POKER_SOLVER_CONFIG_RULE_H_
#define POKER_SOLVER_CONFIG_RULE_H_

#include "Deck.h"                          // For Deck
#include "tools/GameTreeBuildingSettings.h" // For GameTreeBuildingSettings
#include "nodes/GameTreeNode.h"                // For GameRound
#include <vector>
#include <cstddef> // For size_t
#include <stdexcept> // For exceptions
#include <sstream>   // For error messages
#include <utility>   // For std::move

namespace poker_solver {
namespace config {

class Rule {
 public:
  Rule(const core::Deck& deck,
       double initial_oop_commit,
       double initial_ip_commit,
       core::GameRound starting_round,
       const std::vector<int>& initial_board_cards, // <<< ADDED PARAMETER
       int raise_limit_per_street,
       double small_blind,
       double big_blind,
       double initial_effective_stack,
       const GameTreeBuildingSettings& build_settings,
       double all_in_threshold_ratio = 0.98);

  // --- Accessors ---
  const core::Deck& GetDeck() const { return deck_; }
  double GetInitialOopCommit() const { return initial_oop_commit_; }
  double GetInitialIpCommit() const { return initial_ip_commit_; }
  core::GameRound GetStartingRound() const { return starting_round_; }
  const std::vector<int>& GetInitialBoardCardsInt() const { return initial_board_cards_int_; } // Getter
  int GetRaiseLimitPerStreet() const { return raise_limit_per_street_; }
  double GetSmallBlind() const { return small_blind_; }
  double GetBigBlind() const { return big_blind_; }
  double GetInitialEffectiveStack() const { return initial_effective_stack_; }
  const GameTreeBuildingSettings& GetBuildSettings() const { return build_settings_; }
  double GetAllInThresholdRatio() const { return all_in_threshold_ratio_; }
  double GetInitialPot() const;
  double GetInitialCommitment(size_t player_index) const;

  // --- Modifiers ---
  void SetInitialOopCommit(double amount) { initial_oop_commit_ = amount; }
  void SetInitialIpCommit(double amount) { initial_ip_commit_ = amount; }

 private:
  core::Deck deck_;
  double initial_oop_commit_;
  double initial_ip_commit_;
  core::GameRound starting_round_;
  std::vector<int> initial_board_cards_int_; // Stores the initial board cards
  int raise_limit_per_street_;
  double small_blind_;
  double big_blind_;
  double initial_effective_stack_;
  GameTreeBuildingSettings build_settings_;
  double all_in_threshold_ratio_;

  const std::vector<size_t> players_ = {0, 1};
};

} // namespace config
} // namespace poker_solver

#endif // POKER_SOLVER_CONFIG_RULE_H_