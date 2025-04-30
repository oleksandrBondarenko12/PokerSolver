#ifndef POKER_SOLVER_CORE_GAME_ACTIONS_H_
#define POKER_SOLVER_CORE_GAME_ACTIONS_H_

#include "GameTreeNode.h" // For PokerAction enum (adjust path if needed)
#include <string>
#include <stdexcept> // For exceptions
#include <sstream>   // For error messages

namespace poker_solver {
namespace core {

// Represents a specific action taken in a poker game (e.g., BET, CHECK, FOLD)
// along with an associated amount, if applicable.
class GameAction {
 public:
  // Default constructor (creates an invalid BEGIN action).
  GameAction();

  // Constructs a game action.
  // Args:
  //   action: The type of poker action (e.g., PokerAction::kBet).
  //   amount: The amount associated with the action (required for BET/RAISE,
  //           should be -1 or ignored otherwise). Defaults to -1.0.
  // Throws:
  //   std::invalid_argument if amount rules are violated (e.g., amount provided
  //                         for CHECK, or no amount for BET/RAISE).
  explicit GameAction(PokerAction action, double amount = -1.0);

  // Returns the type of the action.
  PokerAction GetAction() const;

  // Returns the amount associated with the action.
  // Returns -1.0 if no amount is applicable (e.g., for CHECK, FOLD, CALL).
  double GetAmount() const;

  // Returns a string representation of the action (e.g., "BET 50.0", "CALL").
  std::string ToString() const;

  // Static helper to convert PokerAction enum to string.
  static std::string ActionToString(PokerAction action);

 private:
  PokerAction action_;
  double amount_; // Amount associated with BET/RAISE actions. -1 otherwise.
};

} // namespace core
} // namespace poker_solver

#endif // POKER_SOLVER_CORE_GAME_ACTIONS_H_
