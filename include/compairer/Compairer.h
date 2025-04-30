#ifndef POKER_SOLVER_CORE_COMPAIRER_H_
#define POKER_SOLVER_CORE_COMPAIRER_H_

#include "Card.h" // For Card definition (adjust path if needed)
#include <vector>
#include <cstdint>

namespace poker_solver {
namespace core {

// Enum representing the result of comparing two poker hands.
enum class ComparisonResult { kPlayer1Wins, kPlayer2Wins, kTie };

// Abstract base class (interface) for poker hand evaluation and comparison.
// Concrete implementations will provide specific evaluation logic (e.g., using
// lookup tables).
class Compairer {
 public:
  // Virtual destructor is essential for base classes.
  virtual ~Compairer() = default;

  // Compares two players' private hands given a shared public board.
  // Args:
  //   private_hand1: Vector of card integers for player 1's hole cards.
  //   private_hand2: Vector of card integers for player 2's hole cards.
  //   public_board: Vector of card integers for the community cards.
  // Returns:
  //   ComparisonResult indicating which hand is stronger or if it's a tie.
  virtual ComparisonResult CompareHands(
      const std::vector<int>& private_hand1,
      const std::vector<int>& private_hand2,
      const std::vector<int>& public_board) const = 0;

  // Compares two players' private hands given a shared public board (uint64_t version).
  // Args:
  //   private_mask1: Bitmask for player 1's hole cards.
  //   private_mask2: Bitmask for player 2's hole cards.
  //   public_mask: Bitmask for the community cards.
  // Returns:
  //   ComparisonResult indicating which hand is stronger or if it's a tie.
  virtual ComparisonResult CompareHands(uint64_t private_mask1,
                                        uint64_t private_mask2,
                                        uint64_t public_mask) const = 0;


  // Calculates the rank (strength) of a single hand combined with public cards.
  // Lower ranks typically represent stronger hands (e.g., 1 = Straight Flush).
  // The exact ranking scale depends on the concrete implementation.
  // Args:
  //   private_hand: Vector of card integers for the player's hole cards.
  //   public_board: Vector of card integers for the community cards.
  // Returns:
  //   An integer representing the hand's rank. Returns a value indicating
  //   an invalid hand or error if inputs are inconsistent (e.g., overlapping cards).
  virtual int GetHandRank(const std::vector<int>& private_hand,
                          const std::vector<int>& public_board) const = 0;

  // Calculates the rank of a hand represented by bitmasks.
  // Args:
  //   private_mask: Bitmask for the player's hole cards.
  //   public_mask: Bitmask for the community cards.
  // Returns:
  //   An integer representing the hand's rank.
  virtual int GetHandRank(uint64_t private_mask,
                          uint64_t public_mask) const = 0;


 protected:
  // Protected default constructor to allow inheritance but prevent direct instantiation.
   Compairer() = default;
};

} // namespace core
} // namespace poker_solver

#endif // POKER_SOLVER_CORE_COMPAIRER_H_
