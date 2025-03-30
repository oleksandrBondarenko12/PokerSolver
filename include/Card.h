#ifndef POKERSOLVER_CARD_H
#define POKERSOLVER_CARD_H

#include <cstdint>
#include <string>
#include <stdexcept>
#include <array>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace PokerSolver {

// Enumeration for the card suit
enum class Suit : uint8_t {
    Clubs = 0,
    Diamonds,
    Hearts,
    Spades
};

// Enumeration for the card rank; note that numeric values start at 2.
enum class Rank : uint8_t {
    Two = 2, Three, Four, Five, Six, Seven, Eight, Nine, Ten,
    Jack, Queen, King, Ace
};

class Card {
public:
    // No default constructor: a card must be constructed with rank and suit.
    Card() = delete;

    // Constructor from rank and suit
    Card(Rank rank, Suit suit);

    // Construct a Card from a string representation.
    // Accepted formats: "Ah", "10c", "Kd" (case insensitive)
    explicit Card(const std::string &cardStr);

    // Accessors
    [[nodiscard]] Rank rank() const noexcept { return rank_; }
    [[nodiscard]] Suit suit() const noexcept { return suit_; }

    // Convert the card to a string (e.g. "Ah", "10c")
    [[nodiscard]] std::string toString() const;

    // Return a unique byte representation (0-51)
    [[nodiscard]] uint8_t toByte() const noexcept;

    // Create a Card from a byte representation.
    static Card fromByte(uint8_t byte);

    // Comparison operators (useful for sorting and storing in containers)
    bool operator==(const Card &other) const noexcept;
    bool operator!=(const Card &other) const noexcept;
    bool operator<(const Card &other) const noexcept;

private:
    Rank rank_;
    Suit suit_;
};

} // namespace PokerSolver

#endif // POKERSOLVER_CARD_H
