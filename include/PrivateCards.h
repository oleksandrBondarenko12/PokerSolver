#ifndef POKERSOLVER_PRIVATECARDS_H
#define POKERSOLVER_PRIVATECARDS_H

#include "Card.h"
#include <cstdint>
#include <string>
#include <vector>
#include <sstream>
#include <functional>
#include <stdexcept>
#include <algorithm>

namespace PokerSolver {

// The PrivateCards class represents a two–card hand (the “private” cards)
// assigned to a player. It stores two Card objects along with a weight (which
// may be used for frequency or probability purposes) and a relative probability.
class PrivateCards {
public:
    // Constructor: creates a PrivateCards instance from two Card objects.
    // The cards are automatically sorted (lowest first) to enforce a canonical order.
    PrivateCards(const Card &card1, const Card &card2, float weight = 1.0f, float relativeProbability = 1.0f);

    // Returns the first card (lowest in canonical order)
    const Card& card1() const noexcept { return card1_; }
    
    // Returns the second card (highest in canonical order)
    const Card& card2() const noexcept { return card2_; }

    // Getters for weight and relative probability
    float weight() const noexcept { return weight_; }
    float relativeProbability() const noexcept { return relativeProbability_; }

    // Setters for weight and relative probability
    void setWeight(float weight) noexcept { weight_ = weight; }
    void setRelativeProbability(float relProb) noexcept { relativeProbability_ = relProb; }

    // Returns a string representation, e.g. "Ah Kd"
    std::string toString() const;

    // Computes a hash code for this hand. (The ordering is canonical.)
    int hashCode() const;

    // Convert the two cards to a 64–bit bitmask.
    // Each card is represented as a single bit (0–51), and the hand is the bitwise OR.
    uint64_t toBoardMask() const;

private:
    Card card1_;
    Card card2_;
    float weight_;
    float relativeProbability_;
};

} // namespace PokerSolver

#endif // POKERSOLVER_PRIVATECARDS_H
