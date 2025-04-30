#ifndef POKER_SOLVER_RANGES_RIVER_COMBS_H_
#define POKER_SOLVER_RANGES_RIVER_COMBS_H_

#include "PrivateCards.h" // For PrivateCards
#include <cstddef> // For size_t

namespace poker_solver {
namespace ranges {

// Represents an evaluated hand combination on the river.
// Stores the original private cards, their evaluated rank on the river board,
// and the index they held in the player's initial range.
struct RiverCombs {
  // Default constructor (creates an invalid state).
  RiverCombs();

  // Constructs a RiverCombs object.
  // Args:
  //   private_cards: The PrivateCards object representing the hole cards.
  //   rank: The evaluated rank of the 7-card hand (private + river board).
  //         Lower rank values are typically better.
  //   original_range_index: The index this hand occupied in the player's
  //                         initial range vector (before river filtering).
  RiverCombs(const core::PrivateCards& private_cards, int rank,
             size_t original_range_index);

  // The player's private hole cards for this specific combination.
  core::PrivateCards private_cards;

  // The evaluated rank (strength) of the 7-card hand (private + river board).
  // Lower values indicate stronger hands. A value like kInvalidRank from the
  // evaluator might indicate an error or impossible hand.
  int rank = -1; // Initialize to an invalid rank

  // The original index of this hand within the player's pre-river range vector.
  // Useful for mapping results back or calculating reach probabilities.
  size_t original_range_index = static_cast<size_t>(-1); // Initialize to invalid index

  // Note: The original code also stored the board vector here. This is removed
  // as RiverCombs objects are typically stored in a structure (like RiverRangeManager)
  // that is already keyed by the specific river board, making storing it again redundant.
};

} // namespace ranges
} // namespace poker_solver

#endif // POKER_SOLVER_RANGES_RIVER_COMBS_H_
