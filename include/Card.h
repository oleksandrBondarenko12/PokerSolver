#ifndef POKER_SOLVER_CORE_CARD_H_
#define POKER_SOLVER_CORE_CARD_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <array>

namespace poker_solver {
namespace core {

// Constants related to a standard 52-card deck.
constexpr int kNumCardsInDeck = 52;
constexpr int kNumSuits = 4;
constexpr int kNumRanks = 13;

// Represents a single playing card.
// Cards are internally represented by an integer from 0 to 51.
// 0 = 2c, 1 = 2d, 2 = 2h, 3 = 2s, 4 = 3c, ..., 51 = As.
// An empty or invalid card is represented by a disengaged std::optional.
class Card {
 public:
  // --- Constructors ---

  // Creates an empty card.
  Card() = default;

  // Creates a card from its integer representation (0-51).
  // Throws std::out_of_range if card_int is invalid.
  explicit Card(int card_int);

  // Creates a card from its string representation (e.g., "As", "Td", "2c").
  // Throws std::invalid_argument if card_str is invalid.
  explicit Card(std::string_view card_str);

  // --- Accessors ---

  // Returns the integer representation (0-51), or std::nullopt if empty.
  std::optional<int> card_int() const { return card_int_; }

  // Returns the string representation (e.g., "As"), or "Empty" if empty.
  std::string ToString() const;

  // Returns true if the card object represents an empty/invalid card.
  bool IsEmpty() const { return !card_int_.has_value(); }

  // --- Static Conversion Utilities ---

  // Converts a card string ("As", "Td", "2c") to its integer representation (0-51).
  // Returns std::nullopt if the string is invalid.
  static std::optional<int> StringToInt(std::string_view card_str);

  // Converts an integer representation (0-51) to its string representation.
  // Returns "Invalid" if the integer is out of range.
  static std::string IntToString(int card_int);

  // --- Static Bitmask Utilities (for card sets/boards) ---

  // Converts a vector of card integers to a 64-bit bitmask.
  // Each bit position corresponds to a card index (0-51).
  // Throws std::out_of_range if any card_int is invalid.
  static uint64_t CardIntsToUint64(const std::vector<int>& card_ints);

  // Converts a vector of Card objects to a 64-bit bitmask.
  // Skips empty cards.
  // Throws std::out_of_range if any non-empty card is invalid (shouldn't happen
  // if cards were constructed properly).
  static uint64_t CardsToUint64(const std::vector<Card>& cards);

  // Converts a single card integer to its 64-bit bitmask representation.
  // Throws std::out_of_range if card_int is invalid.
  static uint64_t CardIntToUint64(int card_int);

  // Converts a single Card object to its 64-bit bitmask representation.
  // Returns 0 if the card is empty.
  // Throws std::out_of_range if the card is invalid (shouldn't happen).
  static uint64_t CardToUint64(const Card& card);

  // Converts a 64-bit bitmask back to a vector of card integers.
  static std::vector<int> Uint64ToCardInts(uint64_t board_mask);

  // Converts a 64-bit bitmask back to a vector of Card objects.
  static std::vector<Card> Uint64ToCards(uint64_t board_mask);

  // Checks if two board masks have any overlapping cards.
  static bool DoBoardsOverlap(uint64_t board_mask1, uint64_t board_mask2);

  // --- Static Rank/Suit Helpers ---

  // Returns the character representation ('c', 'd', 'h', 's') for a suit index (0-3).
  static char SuitIndexToChar(int suit_index);
  // Returns the character representation ('2'-'9', 'T', 'J', 'Q', 'K', 'A') for a rank index (0-12).
  static char RankIndexToChar(int rank_index);
  // Returns the suit index (0-3) for a suit character. Returns -1 if invalid.
  static int SuitCharToIndex(char suit_char);
  // Returns the rank index (0-12) for a rank character. Returns -1 if invalid.
  static int RankCharToIndex(char rank_char);

  // Returns an array of the valid suit characters.
  static const std::array<char, kNumSuits>& GetAllSuitChars();
  // Returns an array of the valid rank characters.
  static const std::array<char, kNumRanks>& GetAllRankChars();

 private:
  // Internal representation: integer 0-51, or std::nullopt for empty.
  std::optional<int> card_int_ = std::nullopt;

  // Helper to validate card integer range.
  static bool IsValidCardInt(int card_int);
};

} // namespace core
} // namespace poker_solver

#endif // POKER_SOLVER_CORE_CARD_H_
