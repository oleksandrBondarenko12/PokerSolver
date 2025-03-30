#include "GameActions.h"

namespace PokerSolver {

// -----------------------------------------------------------------------------
// Constructors

GameActions::GameActions()
    : action_(PokerAction::BEGIN), amount_(0.0)
{
    // Default constructor sets the action to BEGIN and amount to 0.0.
}

GameActions::GameActions(PokerAction action, double amount)
    : action_(action), amount_(amount)
{
}

// -----------------------------------------------------------------------------
// Accessors

PokerAction GameActions::action() const {
    return action_;
}

double GameActions::amount() const {
    return amount_;
}

// -----------------------------------------------------------------------------
// Mutators

void GameActions::setAction(PokerAction action) {
    action_ = action;
}

void GameActions::setAmount(double amount) {
    amount_ = amount;
}

// -----------------------------------------------------------------------------
// Utility Functions

/// @brief Converts a PokerAction enum value to its string representation.
static std::string pokerActionToString(PokerAction action) {
    switch (action) {
        case PokerAction::BEGIN:      return "BEGIN";
        case PokerAction::ROUNDBEGIN: return "ROUNDBEGIN";
        case PokerAction::BET:        return "BET";
        case PokerAction::RAISE:      return "RAISE";
        case PokerAction::CHECK:      return "CHECK";
        case PokerAction::FOLD:       return "FOLD";
        case PokerAction::CALL:       return "CALL";
        default:                      return "UNKNOWN";
    }
}

/// @brief Returns a string representation of the GameActions instance.
std::string GameActions::toString() const {
    std::ostringstream oss;
    oss << "Action: " << pokerActionToString(action_) 
        << ", Amount: " << amount_;
    return oss.str();
}

} // namespace PokerSolver
