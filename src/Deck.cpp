#include "Deck.h" // Adjust path if necessary

#include <stdexcept> // For potential exceptions
#include <sstream>   // For error messages

namespace poker_solver {
namespace core {

// Constructor for a standard 52-card deck.
Deck::Deck() {
    cards_.reserve(kNumCardsInDeck);
    for (int i = 0; i < kNumCardsInDeck; ++i) {
        // Card constructor validates the integer.
        cards_.emplace_back(i);
    }
}

// Constructor for a custom deck (e.g., for Short Deck).
Deck::Deck(const std::vector<std::string_view>& ranks,
           const std::vector<std::string_view>& suits) {
    cards_.reserve(ranks.size() * suits.size());
    for (const auto& rank : ranks) {
        for (const auto& suit : suits) {
            // Combine rank and suit into a string.
            std::string card_str;
            card_str.append(rank);
            card_str.append(suit);
            try {
                // Create card using the string constructor, which validates.
                cards_.emplace_back(card_str);
            } catch (const std::invalid_argument& e) {
                // Handle potential errors during custom deck creation.
                std::ostringstream oss;
                oss << "Error creating custom deck with rank '" << rank
                    << "' and suit '" << suit << "': " << e.what();
                // Depending on desired behavior, could rethrow, log, or ignore.
                // Rethrowing is often safest during development.
                throw std::runtime_error(oss.str());
            }
        }
    }
    // Note: If using this constructor, the internal card integers might not
    // align perfectly with the 0-51 standard if ranks/suits are non-standard.
    // This class primarily assumes a standard deck structure internally.
}


const std::vector<Card>& Deck::GetCards() const {
    return cards_;
}

Card Deck::FindCard(std::string_view card_str) const {
    // Convert the target string to its integer representation first.
    std::optional<int> target_int = Card::StringToInt(card_str);
    if (!target_int) {
        return Card(); // Return empty card if string is invalid
    }
    return FindCard(target_int.value());
}

Card Deck::FindCard(int card_int) const {
    // Check if the integer is valid and if the deck is standard size.
    // This check assumes the deck intends to be standard if FindCard(int) is used.
    if (Card::IsValidCardInt(card_int) && cards_.size() == kNumCardsInDeck) {
         // In a standard ordered deck, the card at index `card_int` should
         // be the card with that integer value.
         if (!cards_[card_int].IsEmpty() && cards_[card_int].card_int() == card_int) {
             return cards_[card_int];
         }
         // Fallback: If deck isn't ordered or standard, search linearly.
         // This shouldn't happen with the default constructor.
         for (const auto& card : cards_) {
             if (!card.IsEmpty() && card.card_int() == card_int) {
                 return card;
             }
         }
    }
    // Return empty card if not found or index is invalid for standard deck.
    return Card();
}


} // namespace core
} // namespace poker_solver
