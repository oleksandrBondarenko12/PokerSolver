#include "tools/Rule.h" // Adjust path if necessary
#include <stdexcept>
#include <sstream>
#include <utility> // For std::move
#include <iostream> // <<< ADD THIS LINE

namespace poker_solver {
namespace config {

// --- Constructor ---
Rule::Rule(const core::Deck& deck,
           double initial_oop_commit,
           double initial_ip_commit,
           core::GameRound starting_round,
           const std::vector<int>& initial_board_cards, // <<< ADDED PARAMETER
           int raise_limit_per_street,
           double small_blind,
           double big_blind,
           double initial_effective_stack,
           const GameTreeBuildingSettings& build_settings,
           double all_in_threshold_ratio)
    : deck_(deck),
      initial_oop_commit_(initial_oop_commit),
      initial_ip_commit_(initial_ip_commit),
      starting_round_(starting_round),
      initial_board_cards_int_(initial_board_cards), // <<< INITIALIZE MEMBER
      raise_limit_per_street_(raise_limit_per_street),
      small_blind_(small_blind),
      big_blind_(big_blind),
      initial_effective_stack_(initial_effective_stack),
      build_settings_(build_settings),
      all_in_threshold_ratio_(all_in_threshold_ratio) {

    // Basic validation (remains the same)
    if (initial_oop_commit < 0 || initial_ip_commit < 0 ||
        small_blind < 0 || big_blind < 0 || initial_effective_stack <= 0) {
         throw std::invalid_argument("Monetary values in Rule cannot be negative (stack must be positive).");
    }
     if (raise_limit_per_street < 0) {
          throw std::invalid_argument("Raise limit cannot be negative.");
     }
     if (all_in_threshold_ratio < 0.0 || all_in_threshold_ratio > 1.0) {
          throw std::invalid_argument("All-in threshold ratio must be between 0.0 and 1.0.");
     }
    size_t actual_board_size = initial_board_cards_int_.size();
    bool board_size_ok = true;
    size_t expected_board_size = 0; // Default to 0

    switch (starting_round_) {
        case core::GameRound::kPreflop: expected_board_size = 0; break;
        case core::GameRound::kFlop:    expected_board_size = 3; break;
        case core::GameRound::kTurn:    expected_board_size = 4; break;
        case core::GameRound::kRiver:   expected_board_size = 5; break;
        // No default needed as GameRound is an enum class
    }

    if (actual_board_size != expected_board_size) {
        board_size_ok = false;
    }

    if (!board_size_ok) {
        std::ostringstream oss;
        oss << "Mismatch between starting_round (" << core::GameTreeNode::GameRoundToString(starting_round_)
            << ") and number of initial_board_cards provided (" << actual_board_size
            << "). Expected " << expected_board_size << " cards.";
        std::cerr << "[WARNING Rule.cpp] " << oss.str() << std::endl;
    }
}

// --- Methods ---

double Rule::GetInitialPot() const {
    return initial_oop_commit_ + initial_ip_commit_;
}

double Rule::GetInitialCommitment(size_t player_index) const {
    if (player_index == 0) { // IP
        return initial_ip_commit_;
    } else if (player_index == 1) { // OOP
        return initial_oop_commit_;
    } else {
        std::ostringstream oss;
        oss << "Invalid player index in GetInitialCommitment: " << player_index;
        throw std::out_of_range(oss.str());
    }
}

} // namespace config
} // namespace poker_solver