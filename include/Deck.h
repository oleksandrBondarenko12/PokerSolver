#ifndef POKERSOLVER_DECK_H
#define POKERSOLVER_DECK_H

#include "Card.h"
#include <vector>
#include <random>
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace PokerSolver {

// The Deck class encapsulates a collection of Card objects. It provides functionality
// for creating a standard deck (52 cards), shuffling, drawing cards, and resetting.
class Deck {
public:
    // Constructs a standard 52-card deck in ordered sequence.
    Deck();

    // Constructs a deck from a given list of cards (useful for custom decks or testing).
    explicit Deck(const std::vector<Card>& cards);

    // Resets the deck to a standard ordered 52-card deck.
    void reset();

    // Shuffles the deck using a Mersenne Twister random engine.
    void shuffle();

    // Draws (removes and returns) the top card from the deck.
    // Throws std::out_of_range if no cards remain.
    Card draw();

    // Returns the number of cards remaining in the deck.
    [[nodiscard]] size_t size() const noexcept;

    // Returns a const reference to the internal card vector.
    [[nodiscard]] const std::vector<Card>& getCards() const noexcept;

private:
    std::vector<Card> cards_;
    std::mt19937 randomEngine_; // Mersenne Twister for shuffling.
};

} // namespace PokerSolver

#endif // POKERSOLVER_DECK_H
