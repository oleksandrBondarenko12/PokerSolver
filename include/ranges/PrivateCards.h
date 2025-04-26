#ifndef POKER_SOLVER_CORE_PRIVATE_CARDS_H_
#define POKER_SOLVER_CORE_PRIVATE_CARDS_H_

#include "Card.h" // For Card class and constants
#include <cstdint>
#include <string>
#include <vector>
#include <functional> // For std::hash
#include <utility>    // For std::pair

namespace poker_solver {
namespace core {

// Represents a player's two private hole cards, along with an associated weight.
// Cards are stored internally in a canonical order (lower int first).
class PrivateCards {
 public:
  // Default constructor (creates an invalid state, use with caution or init).
  PrivateCards();

  // Constructs a PrivateCards object.
  // Args:
  //   card1_int: Integer representation (0-51) of the first card.
  //   card2_int: Integer representation (0-51) of the second card.
  //   weight: The weight associated with this hand combination (default 1.0).
  // Throws:
  //   std::out_of_range if card integers are invalid.
  //   std::invalid_argument if card1_int == card2_int.
  PrivateCards(int card1_int, int card2_int, double weight = 1.0);

  // --- Accessors ---

  // Returns the integer representation of the first card (always the lower int).
  int Card1Int() const { return card1_int_; }

  // Returns the integer representation of the second card (always the higher int).
  int Card2Int() const { return card2_int_; }

  // Returns the weight associated with this hand.
  double Weight() const { return weight_; }

  // Returns the pre-calculated 64-bit bitmask for these two cards.
  uint64_t GetBoardMask() const { return board_mask_; }

  // Returns a canonical string representation (e.g., "AKs", "TdTs").
  // Note: This requires converting back to Card objects, potentially less efficient.
  // Consider if only int representation is needed internally.
  std::string ToString() const;

  // Returns the two card integers as a std::pair.
  std::pair<int, int> GetCardInts() const { return {card1_int_, card2_int_}; }

  // --- Operators ---

  // Equality comparison based on the two card integers.
  bool operator==(const PrivateCards& other) const;

  // Inequality comparison.
  bool operator!=(const PrivateCards& other) const;

  // Less-than comparison based first on card1_int_, then card2_int_.
  // Useful for sorting or using in ordered containers.
  bool operator<(const PrivateCards& other) const;


 private:
  // Ensure canonical order and calculate derived members.
  void Initialize(int c1, int c2, double w);

  int card1_int_;      // Lower card integer (0-51)
  int card2_int_;      // Higher card integer (0-51)
  double weight_;      // Weight/frequency of this hand combo
  uint64_t board_mask_; // Bitmask representation (1ULL << c1) | (1ULL << c2)
};

// --- Hash Function Specialization ---
// Allows PrivateCards to be used as keys in unordered containers.

} // namespace core
} // namespace poker_solver


// Specialize std::hash for PrivateCards within the std namespace
// (as per standard practice for hash specializations).
namespace std {
template <>
struct hash<poker_solver::core::PrivateCards> {
  std::size_t operator()(
      const poker_solver::core::PrivateCards& pc) const noexcept;
};
} // namespace std


#endif // POKER_SOLVER_CORE_PRIVATE_CARDS_H_
