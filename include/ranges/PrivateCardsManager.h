#ifndef POKER_SOLVER_RANGES_PRIVATE_CARDS_MANAGER_H_
#define POKER_SOLVER_RANGES_PRIVATE_CARDS_MANAGER_H_

#include "PrivateCards.h" // For PrivateCards
#include "Card.h"          // For Card utilities
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <optional> // For optional index return

namespace poker_solver {
namespace ranges {

// Manages the initial private hand ranges for multiple players,
// accounting for initial board cards and calculating reach probabilities.
class PrivateCardsManager {
 public:
  // Constructor.
  // Args:
  //   initial_ranges: A vector where each inner vector contains the
  //                   PrivateCards range for a player (e.g., index 0 for P1, 1 for P2).
  //                   Ranges should be pre-filtered against the initial board.
  //   initial_board_mask: The bitmask representation of the initial board cards.
  // Throws:
  //   std::invalid_argument if initial_ranges is empty.
  PrivateCardsManager(
      std::vector<std::vector<core::PrivateCards>> initial_ranges,
      uint64_t initial_board_mask);

  // Returns the number of players managed.
  size_t GetNumPlayers() const { return num_players_; }

  // Returns a const reference to the initial (pre-filtered) range for a player.
  // Args:
  //   player_index: The index of the player (0-based).
  // Returns:
  //   A const reference to the player's PrivateCards vector.
  // Throws:
  //   std::out_of_range if player_index is invalid.
  const std::vector<core::PrivateCards>& GetPlayerRange(
      size_t player_index) const;

  // Finds the index of a specific hand combination within another player's range.
  // Uses the canonical hash of the PrivateCards.
  // Args:
  //   from_player_index: The index of the player whose hand we are starting with.
  //   to_player_index: The index of the player whose range we are searching in.
  //   from_hand_index: The index of the hand within from_player_index's range.
  // Returns:
  //   The index of the equivalent hand in to_player_index's range, or
  //   std::nullopt if the hand doesn't exist in the target range or if indices
  //   are out of bounds.
  std::optional<size_t> GetOpponentHandIndex(
      size_t from_player_index,
      size_t to_player_index,
      size_t from_hand_index) const;

  // Calculates the initial reach probability vector for a specific player.
  // This vector reflects the initial weights, adjusted for card removal effects
  // against the initial board and *all other players'* possible hands.
  // Args:
  //   player_index: The index of the player.
  // Returns:
  //   A vector of floats representing the normalized reach probability for each
  //   hand in the player's range.
  // Throws:
  //   std::out_of_range if player_index is invalid.
  const std::vector<double>& GetInitialReachProbs(size_t player_index) const;


 private:
  // Calculates the relative reach probabilities for all players.
  // This considers card removal between players and the initial board.
  // Stores the results in initial_reach_probs_.
  // NOTE: Current implementation assumes exactly 2 players.
  void CalculateInitialReachProbs();

  // --- Member Variables ---

  size_t num_players_;
  uint64_t initial_board_mask_;

  // Stores the initial ranges provided to the constructor.
  std::vector<std::vector<core::PrivateCards>> player_ranges_;

  // Lookup table: maps PrivateCards hash -> player index -> hand index in player_ranges_
  // Using std::size_t for the hash key (result of std::hash<PrivateCards>).
  std::unordered_map<std::size_t, std::vector<std::optional<size_t>>>
      hand_hash_to_indices_;

  // Stores the calculated initial reach probabilities for each player,
  // considering card removal effects.
  std::vector<std::vector<double>> initial_reach_probs_;
};

} // namespace ranges
} // namespace poker_solver

#endif // POKER_SOLVER_RANGES_PRIVATE_CARDS_MANAGER_H_

