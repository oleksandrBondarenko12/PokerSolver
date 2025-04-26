#ifndef POKER_SOLVER_CORE_DECK_H_
#define POKER_SOLVER_CORE_DECK_H_

#include "Card.h" // Includes kNumCardsInDeck
#include <vector>
#include <string>
#include <string_view>

namespace poker_solver {
namespace core {

// Represents a standard 52-card deck.
class Deck {
 public:
  // Creates a standard, ordered 52-card deck.
  Deck();

  // Creates a deck using custom ranks and suits (primarily for testing or
  // non-standard games).
  Deck(const std::vector<std::string_view>& ranks,
       const std::vector<std::string_view>& suits);

  // Returns a const reference to the vector of cards in the deck.
  const std::vector<Card>& GetCards() const;

  // Finds a card by its string representation ("As", "Td", etc.).
  // Returns an empty Card object if not found.
  // Note: Linear search, potentially slow for frequent lookups.
  Card FindCard(std::string_view card_str) const;

  // Finds a card by its integer representation (0-51).
  // Returns an empty Card object if the index is invalid or the deck
  // doesn't contain the standard 52 cards.
  Card FindCard(int card_int) const;

 private:
  // The collection of cards representing the deck.
  std::vector<Card> cards_;
};

} // namespace core
} // namespace poker_solver

#endif // POKER_SOLVER_CORE_DECK_H_
