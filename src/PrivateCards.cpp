#include "PrivateCards.h"

namespace PokerSolver {

PrivateCards::PrivateCards(const Card &c1, const Card &c2, float weight, float relativeProbability)
    : weight_(weight), relativeProbability_(relativeProbability)
{
    // Enforce canonical order: store the lower card (according to operator<) first.
    if (c2 < c1) {
        card1_ = c2;
        card2_ = c1;
    } else {
        card1_ = c1;
        card2_ = c2;
    }
}

std::string PrivateCards::toString() const {
    // Use the Card::toString() methods to generate a human-readable representation.
    return card1_.toString() + " " + card2_.toString();
}

int PrivateCards::hashCode() const {
    // A simple hash combination that uses the unique byte representation from Card::toByte().
    // Because our hand is stored in canonical order, this hash is consistent.
    int byte1 = static_cast<int>(card1_.toByte());
    int byte2 = static_cast<int>(card2_.toByte());
    // For example, use a simple mixing function.
    return byte1 * 53 + byte2;
}

uint64_t PrivateCards::toBoardMask() const {
    // Create a 64-bit mask where each card's unique bit (0-51) is set.
    // For example, if card1_ is represented by 5 and card2_ by 20, then the mask is:
    // (1 << 5) | (1 << 20)
    uint64_t mask = (1ULL << card1_.toByte()) | (1ULL << card2_.toByte());
    return mask;
}

} // namespace PokerSolver
