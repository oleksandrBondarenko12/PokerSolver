#ifndef POKER_SOLVER_RANGES_PRIVATE_RANGE_CONVERTER_H_
#define POKER_SOLVER_RANGES_PRIVATE_RANGE_CONVERTER_H_

#include "ranges/PrivateCards.h" // For PrivateCards
#include "Card.h"          // For Card utilities
#include <vector>
#include <string>
#include <string_view>
#include <stdexcept> // For exceptions

namespace poker_solver {
namespace ranges {

// Utility class to convert poker hand range strings into collections of
// PrivateCards objects.
class PrivateRangeConverter {
 public:
  // Parses a standard poker range string (e.g., "AKs,QQ,T9s:0.5,AcKc")
  // into a vector of PrivateCards objects.
  // Args:
  //   range_string: The input string representing the hand range.
  //                 Components are comma-separated. Weights can be specified
  //                 after a colon (e.g., "JJ:0.5").
  //   initial_board_ints: Optional vector of card integers representing the
  //                       initial board cards. Hands conflicting with these
  //                       cards will be excluded from the result.
  // Returns:
  //   A vector of PrivateCards representing the parsed range, with weights
  //   applied and filtered against the initial board.
  // Throws:
  //   std::invalid_argument if the range string format is invalid or contains
  //                         duplicate hand definitions.
  //   std::out_of_range if card integers derived from the string are invalid.
  static std::vector<core::PrivateCards> StringToPrivateCards(
      std::string_view range_string,
      const std::vector<int>& initial_board_ints = {});

 private:
   // Prevent instantiation - static methods only.
   PrivateRangeConverter() = delete;

   // Helper to parse a single range component (e.g., "AKs", "QQ:0.5").
   // Adds the generated PrivateCards to the output vector.
   static void ParseRangeComponent(
       std::string_view component,
       uint64_t initial_board_mask,
       std::vector<core::PrivateCards>& output_cards);

   // Helper to generate pair combinations (e.g., for "QQ").
   static void GeneratePairCombos(
       char rank_char,
       double weight,
       uint64_t initial_board_mask,
       std::vector<core::PrivateCards>& output_cards);

   // Helper to generate suited combinations (e.g., for "AKs").
   static void GenerateSuitedCombos(
       char rank1_char,
       char rank2_char,
       double weight,
       uint64_t initial_board_mask,
       std::vector<core::PrivateCards>& output_cards);

    // Helper to generate offsuit combinations (e.g., for "AKo").
   static void GenerateOffsuitCombos(
       char rank1_char,
       char rank2_char,
       double weight,
       uint64_t initial_board_mask,
       std::vector<core::PrivateCards>& output_cards);

    // Helper to generate a specific hand combination (e.g., for "AcKc").
    static void GenerateSpecificCombo(
        std::string_view combo_str,
        double weight,
        uint64_t initial_board_mask,
        std::vector<core::PrivateCards>& output_cards);

};

} // namespace ranges
} // namespace poker_solver

#endif // POKER_SOLVER_RANGES_PRIVATE_RANGE_CONVERTER_H_
