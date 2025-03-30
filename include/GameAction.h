#ifndef POKERSOLVER_GAMEACTIONS_H
#define POKERSOLVER_GAMEACTIONS_H

#include <string>
#include <sstream>
#include "GameTreeNode.h" // For the PokerAction enum

namespace PokerSolver {

/// @brief Represents a single poker action along with an associated amount.
/// This class is used within nodes (for example, in ActionNode) to record the actions
/// available or taken in a game tree.
class GameActions {
public:
    /// @brief Default constructor initializes the action to BEGIN and amount to zero.
    GameActions();

    /// @brief Constructs a GameActions with a specified action and amount.
    /// @param action The poker action (e.g., BET, CALL, FOLD).
    /// @param amount The betting amount associated with the action.
    GameActions(PokerAction action, double amount);

    /// @brief Returns the poker action.
    PokerAction action() const;

    /// @brief Returns the associated amount.
    double amount() const;

    /// @brief Sets the poker action.
    void setAction(PokerAction action);

    /// @brief Sets the associated amount.
    void setAmount(double amount);

    /// @brief Returns a human-readable string representing the action and amount.
    std::string toString() const;

private:
    PokerAction action_;
    double amount_;
};

} // namespace PokerSolver

#endif // POKERSOLVER_GAMEACTIONS_H
