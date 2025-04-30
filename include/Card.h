#ifndef POKER_SOLVER_CORE_CARD_H_
#define POKER_SOLVER_CORE_CARD_H_

#include <cstdint>
#include <optional> // Added include
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <stdexcept> // Include for exceptions used in header declarations
#include <sstream>   // Include for ostringstream used in header declarations


namespace poker_solver {
namespace core {

// Constants related to a standard 52-card deck.
constexpr int kNumCardsInDeck = 52;
constexpr int kNumSuits = 4;
constexpr int kNumRanks = 13;

// Represents a single playing card.
class Card {
 public:
  // --- Constructors ---
  Card() = default;
  explicit Card(int card_int);
  explicit Card(std::string_view card_str);

  // --- Accessors ---
  std::optional<int> card_int() const { return card_int_; }
  std::string ToString() const;
  bool IsEmpty() const { return !card_int_.has_value(); }

  // --- Comparison Operators ---
  // Checks if two cards are the same (based on card_int_). Handles empty cards.
  bool operator==(const Card& other) const;
  bool operator!=(const Card& other) const;

  // --- Static Conversion Utilities ---
  static std::optional<int> StringToInt(std::string_view card_str);
  static std::string IntToString(int card_int);

  // --- Static Bitmask Utilities ---
  static uint64_t CardIntsToUint64(const std::vector<int>& card_ints);
  static uint64_t CardsToUint64(const std::vector<Card>& cards);
  static uint64_t CardIntToUint64(int card_int);
  static uint64_t CardToUint64(const Card& card);
  static std::vector<int> Uint64ToCardInts(uint64_t board_mask);
  static std::vector<Card> Uint64ToCards(uint64_t board_mask);
  static bool DoBoardsOverlap(uint64_t board_mask1, uint64_t board_mask2);

  // --- Static Rank/Suit Helpers ---
  static char SuitIndexToChar(int suit_index);
  static char RankIndexToChar(int rank_index);
  static int SuitCharToIndex(char suit_char);
  static int RankCharToIndex(char rank_char);
  static const std::array<char, kNumSuits>& GetAllSuitChars();
  static const std::array<char, kNumRanks>& GetAllRankChars();

  // --- Static Validation Helper ---
  static bool IsValidCardInt(int card_int);

 private:
  std::optional<int> card_int_ = std::nullopt;
};

} // namespace core
} // namespace poker_solver

#endif // POKER_SOLVER_CORE_CARD_H_
