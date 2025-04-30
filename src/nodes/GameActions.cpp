#include "nodes/GameActions.h" // Adjust path if necessary


#include <limits> // For std::numeric_limits (though not used currently)
#include <sstream> // For std::ostringstream
#include <stdexcept> // For std::invalid_argument

namespace poker_solver {
namespace core {

// Define default amount sentinel value more clearly
constexpr double kNoAmount = -1.0;

GameAction::GameAction()
    : action_(PokerAction::kBegin), // Default to an initial state action
      amount_(kNoAmount) {}

GameAction::GameAction(PokerAction action, double amount)
    : action_(action), amount_(amount) {
  // Validate amount based on action type
  bool requires_amount = (action_ == PokerAction::kBet ||
                          action_ == PokerAction::kRaise);
  bool has_amount = (amount != kNoAmount); // Check against sentinel

  if (requires_amount && !has_amount) {
    std::ostringstream oss;
    oss << "Action " << ActionToString(action_)
        << " requires an amount, but none was provided (amount=" << amount
        << ").";
    throw std::invalid_argument(oss.str());
  }

  if (!requires_amount && has_amount) {
    std::ostringstream oss;
    oss << "Action " << ActionToString(action_)
        << " should not have an amount, but amount=" << amount
        << " was provided.";
    // Optionally, just ignore the amount instead of throwing:
    // amount_ = kNoAmount;
    // Throwing is stricter during development.
    throw std::invalid_argument(oss.str());
  }

  // Optional: Check if amount is non-negative for BET/RAISE
  if (requires_amount && amount < 0) {
       std::ostringstream oss;
       oss << "Amount for " << ActionToString(action_)
           << " cannot be negative: " << amount;
       throw std::invalid_argument(oss.str());
  }
}

PokerAction GameAction::GetAction() const {
  return action_;
}

double GameAction::GetAmount() const {
  // Return the stored amount; it will be kNoAmount if not applicable.
  return amount_;
}

std::string GameAction::ActionToString(PokerAction action) {
  switch (action) {
    case PokerAction::kBegin:      return "BEGIN";
    case PokerAction::kRoundBegin: return "ROUND_BEGIN"; // Use underscore style
    case PokerAction::kBet:        return "BET";
    case PokerAction::kRaise:      return "RAISE";
    case PokerAction::kCheck:      return "CHECK";
    case PokerAction::kFold:       return "FOLD";
    case PokerAction::kCall:       return "CALL";
    default:
      // Handle potential unknown enum values gracefully
      return "UNKNOWN_ACTION";
  }
}

std::string GameAction::ToString() const {
  std::string action_str = ActionToString(action_);
  if (amount_ != kNoAmount) {
    // Append amount for actions that have one
    std::ostringstream oss;
    // Use default precision for double-to-string conversion
    oss << action_str << " " << amount_;
    return oss.str();
  } else {
    return action_str;
  }
}

} // namespace core
} // namespace poker_solver
