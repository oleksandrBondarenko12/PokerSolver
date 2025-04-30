#include "tools/PrivateRangeConverter.h" // Adjust path if necessary
#include "Library.h" // For StringSplit (adjust path if needed)
#include "Card.h"    // Need full Card definition for static methods

#include <vector>
#include <string>
#include <string_view>
#include <sstream>
#include <stdexcept>
#include <unordered_set> // To check for duplicate generated hands
#include <charconv>      // For std::from_chars (C++17) - REMOVED FOR DOUBLE
#include <system_error>  // For std::errc - REMOVED
#include <functional>    // For std::hash<PrivateCards>
#include <cstdlib>       // For std::strtod (alternative to stod)
#include <cerrno>        // For errno with strtod
#include <cctype>        // For std::tolower

// Use aliases for namespaces
// Assuming namespaces defined elsewhere match our structure
namespace core = poker_solver::core;
namespace utils = poker_solver::utils;
namespace ranges = poker_solver::ranges; // Assuming this namespace

namespace poker_solver {
namespace ranges { // Assuming this namespace

// --- Static Public Method ---

std::vector<core::PrivateCards> PrivateRangeConverter::StringToPrivateCards(
    std::string_view range_string,
    const std::vector<int>& initial_board_ints) {

    std::vector<core::PrivateCards> result_cards;
    uint64_t initial_board_mask = 0;
    try {
        initial_board_mask = core::Card::CardIntsToUint64(initial_board_ints);
    } catch (const std::out_of_range& e) {
        throw std::invalid_argument(
            "Invalid card integer found in initial_board_ints.");
    }

    // Split the main string by commas
    std::vector<std::string> components = utils::StringSplit(range_string, ',');

    // Keep track of generated hands to detect duplicates from input string
    std::unordered_set<core::PrivateCards, std::hash<core::PrivateCards>> generated_hands;

    for (const std::string& component_str : components) {
        // Trim whitespace (optional but good practice)
        std::string_view component = component_str;
        size_t first = component.find_first_not_of(" \t\n\r");
        if (first == std::string_view::npos) continue; // Skip empty/whitespace components
        size_t last = component.find_last_not_of(" \t\n\r");
        component = component.substr(first, (last - first + 1));

        if (component.empty()) continue;

        // Store current size to check for duplicates added by this component
        size_t cards_before_component = result_cards.size();

        // Parse the component (handles weights and calls generation helpers)
        try {
             ParseRangeComponent(component, initial_board_mask, result_cards);
        } catch (const std::invalid_argument& e) {
             // Re-throw parsing errors with more context if desired, or just let them propagate
             throw; // Propagate argument errors
        } catch (const std::out_of_range& e) {
            // Propagate card errors
            throw;
        }


        // Check for duplicates introduced by this component against previous ones
        for (size_t i = cards_before_component; i < result_cards.size(); ++i) {
            if (!generated_hands.insert(result_cards[i]).second) {
                // Insertion failed -> duplicate hand
                 std::ostringstream oss;
                 oss << "Duplicate hand definition found in range string for component '"
                     << std::string(component) << "'. Hand: " << result_cards[i].ToString(); // Convert component back to string for message
                 throw std::invalid_argument(oss.str());
            }
        }
    }

    return result_cards;
}


// --- Private Static Helpers ---

void PrivateRangeConverter::ParseRangeComponent(
    std::string_view component,
    uint64_t initial_board_mask,
    std::vector<core::PrivateCards>& output_cards) {

    double weight = 1.0;
    std::string_view hand_notation = component;

    // Check for weight specified with ':'
    size_t colon_pos = component.find(':');
    if (colon_pos != std::string_view::npos) {
        hand_notation = component.substr(0, colon_pos);
        std::string_view weight_str_view = component.substr(colon_pos + 1);

        // Trim whitespace from weight string
        size_t first = weight_str_view.find_first_not_of(" \t");
        size_t last = weight_str_view.find_last_not_of(" \t");
        if (first == std::string_view::npos) {
             throw std::invalid_argument("Empty weight specified after colon for component: " + std::string(component));
        }
        weight_str_view = weight_str_view.substr(first, (last - first + 1));

        // *** FIX: Use std::stod (or strtod) instead of std::from_chars for double ***
        std::string weight_str(weight_str_view); // Need null-terminated string for stod/strtod
        try {
            size_t processed_chars = 0;
            weight = std::stod(weight_str, &processed_chars);
            // Check if the entire string was consumed
            if (processed_chars != weight_str.length()) {
                 throw std::invalid_argument("Invalid characters after weight value.");
            }
        } catch (const std::invalid_argument& ) {
            throw std::invalid_argument("Invalid weight format (not a number) specified for component: " + std::string(component));
        } catch (const std::out_of_range& ) {
             throw std::invalid_argument("Weight value out of range for component: " + std::string(component));
        }
        // **************************************************************************


        // Ignore components with near-zero weight (as in original)
        if (weight <= 0.005) {
            return;
        }
    }

    // Trim whitespace from hand notation
    size_t first = hand_notation.find_first_not_of(" \t");
    size_t last = hand_notation.find_last_not_of(" \t");
    if (first == std::string_view::npos) {
        throw std::invalid_argument("Empty hand notation in component: " + std::string(component));
    }
    hand_notation = hand_notation.substr(first, (last - first + 1));


    // Determine hand notation type and call appropriate generator
    size_t len = hand_notation.length();

    if (len == 2) { // Pair (e.g., "QQ")
        if (hand_notation[0] == hand_notation[1]) {
            GeneratePairCombos(hand_notation[0], weight, initial_board_mask, output_cards);
        } else {
             throw std::invalid_argument("Invalid 2-char hand notation (expected pair like 'QQ'): " + std::string(hand_notation));
        }
    } else if (len == 3) { // Suited (AKs) or Offsuit (AKo)
        if (hand_notation[0] == hand_notation[1]) {
             throw std::invalid_argument("Invalid notation: Cannot specify suited/offsuit for pairs: " + std::string(hand_notation));
        }
        char suffix = std::tolower(hand_notation[2]); // Use std::tolower
        if (suffix == 's') {
            GenerateSuitedCombos(hand_notation[0], hand_notation[1], weight, initial_board_mask, output_cards);
        } else if (suffix == 'o') {
            GenerateOffsuitCombos(hand_notation[0], hand_notation[1], weight, initial_board_mask, output_cards);
        } else {
            throw std::invalid_argument("Invalid suffix (expected 's' or 'o'): " + std::string(hand_notation));
        }
    } else if (len == 4) { // Specific combo (e.g., "AcKc")
         GenerateSpecificCombo(hand_notation, weight, initial_board_mask, output_cards);
    }
    else {
        throw std::invalid_argument("Invalid hand notation length: " + std::string(hand_notation));
    }
}


void PrivateRangeConverter::GeneratePairCombos(
    char rank_char,
    double weight,
    uint64_t initial_board_mask,
    std::vector<core::PrivateCards>& output_cards) {

    int rank_index = core::Card::RankCharToIndex(rank_char);
    if (rank_index == -1) {
        throw std::invalid_argument("Invalid rank character for pair: " + std::string(1, rank_char));
    }

    const auto& suits = core::Card::GetAllSuitChars(); // Get {'c', 'd', 'h', 's'}

    // Iterate through all pairs of suits
    for (int i = 0; i < core::kNumSuits; ++i) {
        for (int j = i + 1; j < core::kNumSuits; ++j) {
            // *** ERROR: Incorrectly calling StringToInt with initializer list ***
            // std::optional<int> c1_opt = core::Card::StringToInt({&rank_char, 1, &suits[i], 1});
            // std::optional<int> c2_opt = core::Card::StringToInt({&rank_char, 1, &suits[j], 1});
            // *** CORRECTED CODE: ***
            std::string card1_str; card1_str += rank_char; card1_str += suits[i];
            std::string card2_str; card2_str += rank_char; card2_str += suits[j];
            std::optional<int> c1_opt = core::Card::StringToInt(card1_str);
            std::optional<int> c2_opt = core::Card::StringToInt(card2_str);
            // ***********************

            if (!c1_opt || !c2_opt) continue; // Should not happen if rank_char is valid

            int c1 = c1_opt.value();
            int c2 = c2_opt.value();

            uint64_t hand_mask = core::Card::CardIntToUint64(c1) | core::Card::CardIntToUint64(c2);
            if (!core::Card::DoBoardsOverlap(hand_mask, initial_board_mask)) {
                output_cards.emplace_back(c1, c2, weight);
            }
        }
    }
}

void PrivateRangeConverter::GenerateSuitedCombos(
    char rank1_char,
    char rank2_char,
    double weight,
    uint64_t initial_board_mask,
    std::vector<core::PrivateCards>& output_cards) {

     int rank1_index = core::Card::RankCharToIndex(rank1_char);
     int rank2_index = core::Card::RankCharToIndex(rank2_char);
     if (rank1_index == -1 || rank2_index == -1 || rank1_index == rank2_index) {
         throw std::invalid_argument("Invalid rank characters for suited hand: " + std::string(1, rank1_char) + std::string(1, rank2_char) + "s");
     }

    const auto& suits = core::Card::GetAllSuitChars();

    // Iterate through each suit
    for (int i = 0; i < core::kNumSuits; ++i) {
        // *** ERROR: Incorrectly calling StringToInt with initializer list ***
        // std::optional<int> c1_opt = core::Card::StringToInt({&rank1_char, 1, &suits[i], 1});
        // std::optional<int> c2_opt = core::Card::StringToInt({&rank2_char, 1, &suits[i], 1});
        // *** CORRECTED CODE: ***
        std::string card1_str; card1_str += rank1_char; card1_str += suits[i];
        std::string card2_str; card2_str += rank2_char; card2_str += suits[i];
        std::optional<int> c1_opt = core::Card::StringToInt(card1_str);
        std::optional<int> c2_opt = core::Card::StringToInt(card2_str);
        // ***********************

        if (!c1_opt || !c2_opt) continue;

        int c1 = c1_opt.value();
        int c2 = c2_opt.value();

        uint64_t hand_mask = core::Card::CardIntToUint64(c1) | core::Card::CardIntToUint64(c2);
        if (!core::Card::DoBoardsOverlap(hand_mask, initial_board_mask)) {
            output_cards.emplace_back(c1, c2, weight);
        }
    }
}

void PrivateRangeConverter::GenerateOffsuitCombos(
    char rank1_char,
    char rank2_char,
    double weight,
    uint64_t initial_board_mask,
    std::vector<core::PrivateCards>& output_cards) {

    int rank1_index = core::Card::RankCharToIndex(rank1_char);
    int rank2_index = core::Card::RankCharToIndex(rank2_char);
     if (rank1_index == -1 || rank2_index == -1 || rank1_index == rank2_index) {
         throw std::invalid_argument("Invalid rank characters for offsuit hand: " + std::string(1, rank1_char) + std::string(1, rank2_char) + "o");
     }

    const auto& suits = core::Card::GetAllSuitChars();

    // Iterate through all pairs of different suits
    for (int i = 0; i < core::kNumSuits; ++i) { // Suit for first rank
        for (int j = 0; j < core::kNumSuits; ++j) { // Suit for second rank
            if (i == j) continue; // Skip suited combinations

            // *** ERROR: Incorrectly calling StringToInt with initializer list ***
            // std::optional<int> c1_opt = core::Card::StringToInt({&rank1_char, 1, &suits[i], 1});
            // std::optional<int> c2_opt = core::Card::StringToInt({&rank2_char, 1, &suits[j], 1});
            // *** CORRECTED CODE: ***
            std::string card1_str; card1_str += rank1_char; card1_str += suits[i];
            std::string card2_str; card2_str += rank2_char; card2_str += suits[j];
            std::optional<int> c1_opt = core::Card::StringToInt(card1_str);
            std::optional<int> c2_opt = core::Card::StringToInt(card2_str);
            // ***********************

             if (!c1_opt || !c2_opt) continue;

            int c1 = c1_opt.value();
            int c2 = c2_opt.value();

            uint64_t hand_mask = core::Card::CardIntToUint64(c1) | core::Card::CardIntToUint64(c2);
            if (!core::Card::DoBoardsOverlap(hand_mask, initial_board_mask)) {
                output_cards.emplace_back(c1, c2, weight);
            }
        }
    }
}

void PrivateRangeConverter::GenerateSpecificCombo(
    std::string_view combo_str, // e.g., "AcKc"
    double weight,
    uint64_t initial_board_mask,
    std::vector<core::PrivateCards>& output_cards) {

    if (combo_str.length() != 4) {
        throw std::invalid_argument("Invalid specific combo length: " + std::string(combo_str));
    }

    std::string_view card1_str = combo_str.substr(0, 2);
    std::string_view card2_str = combo_str.substr(2, 2);

    std::optional<int> c1_opt = core::Card::StringToInt(card1_str);
    std::optional<int> c2_opt = core::Card::StringToInt(card2_str);

    if (!c1_opt || !c2_opt) {
         throw std::invalid_argument("Invalid card string in specific combo: " + std::string(combo_str));
    }
     if (c1_opt.value() == c2_opt.value()) {
         throw std::invalid_argument("Specific combo cards cannot be identical: " + std::string(combo_str));
    }


    int c1 = c1_opt.value();
    int c2 = c2_opt.value();

    uint64_t hand_mask = core::Card::CardIntToUint64(c1) | core::Card::CardIntToUint64(c2);
    if (!core::Card::DoBoardsOverlap(hand_mask, initial_board_mask)) {
        output_cards.emplace_back(c1, c2, weight);
    }
}


} // namespace ranges
} // namespace poker_solver
