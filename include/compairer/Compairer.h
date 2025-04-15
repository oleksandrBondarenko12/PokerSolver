#ifndef POKERSOLVER_COMPAIRER_H
#define POKERSOLVER_COMPAIRER_H

#include "Card.h"
#include "ranges/PrivateCards.h"
#include <vector>

namespace PokerSolver {

/// @brief The Compairer interface defines how to compare two poker hands.
/// 
/// It provides a pure virtual function for comparing two hands given private and board cards,
/// as well as a method for evaluating the rank (strength) of a hand.
class Compairer {
public:
    /// @brief Represents the result of comparing two poker hands.
    enum class ComparisonResult {
        LARGER,   // The first hand is stronger.
        EQUAL,    // The hands are of equal strength.
        SMALLER   // The first hand is weaker.
    };

    /// Virtual destructor for safe polymorphic deletion.
    virtual ~Compairer() = default;

    /// @brief Compares two hands given their private cards and the public board.
    /// 
    /// @param privateHand1 The first player’s private cards.
    /// @param privateHand2 The second player’s private cards.
    /// @param board The shared public board cards.
    /// @return ComparisonResult indicating whether the first hand is LARGER, EQUAL, or SMALLER.
    virtual ComparisonResult compare(const std::vector<Card>& privateHand1,
                                     const std::vector<Card>& privateHand2,
                                     const std::vector<Card>& board) const = 0;

    /// @brief Evaluates and returns a numerical rank for a given hand.
    /// 
    /// A higher rank value indicates a stronger hand.
    ///
    /// @param privateHand The player's private cards.
    /// @param board The public board cards.
    /// @return int representing the hand’s rank.
    virtual int getRank(const std::vector<Card>& privateHand,
                        const std::vector<Card>& board) const = 0;
};

} // namespace PokerSolver

#endif // POKERSOLVER_COMPAIRER_H
