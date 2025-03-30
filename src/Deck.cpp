#include "Deck.h"

namespace PokerSolver {

// Default constructor: Create a standard deck and seed the random engine.
Deck::Deck() {
    reset();
    // Seed using the current time.
    auto seed = static_cast<unsigned>(std::chrono::system_clock::now().time_since_epoch().count());
    randomEngine_.seed(seed);
}

// Constructor that accepts a custom vector of cards.
Deck::Deck(const std::vector<Card>& cards)
    : cards_(cards)
{
    auto seed = static_cast<unsigned>(std::chrono::system_clock::now().time_since_epoch().count());
    randomEngine_.seed(seed);
}

// Reset the deck to the standard 52 cards in a defined order.
void Deck::reset() {
    cards_.clear();
    // Iterate over all ranks and suits.
    for (int r = static_cast<int>(Rank::Two); r <= static_cast<int>(Rank::Ace); ++r) {
        for (int s = 0; s < 4; ++s) {
            cards_.push_back(Card(static_cast<Rank>(r), static_cast<Suit>(s)));
        }
    }
}

// Shuffle the deck using std::shuffle with our Mersenne Twister engine.
void Deck::shuffle() {
    std::shuffle(cards_.begin(), cards_.end(), randomEngine_);
}

// Draw the top card from the deck. Throws if the deck is empty.
Card Deck::draw() {
    if (cards_.empty()) {
        throw std::out_of_range("Deck::draw: No more cards in the deck.");
    }
    Card drawn = cards_.back();
    cards_.pop_back();
    return drawn;
}

// Return the number of cards remaining in the deck.
size_t Deck::size() const noexcept {
    return cards_.size();
}

// Return a constant reference to the underlying cards vector.
const std::vector<Card>& Deck::getCards() const noexcept {
    return cards_;
}

} // namespace PokerSolver
