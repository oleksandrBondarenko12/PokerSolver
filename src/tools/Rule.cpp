#include "tools/Rule.h" // Adjust path if necessary
#include <stdexcept> // Added for exceptions
#include <sstream>   // Added for error messages

namespace poker_solver {
namespace config {

// --- Constructor ---

Rule::Rule(const core::Deck& deck,
           double initial_oop_commit,
           double initial_ip_commit,
           core::GameRound starting_round,
           int raise_limit_per_street,
           double small_blind,
           double big_blind,
           double initial_effective_stack,
           const GameTreeBuildingSettings& build_settings,
           double all_in_threshold_ratio)
    : deck_(deck), // Copy the deck
      initial_oop_commit_(initial_oop_commit),
      initial_ip_commit_(initial_ip_commit),
      starting_round_(starting_round),
      raise_limit_per_street_(raise_limit_per_street),
      small_blind_(small_blind),
      big_blind_(big_blind),
      initial_effective_stack_(initial_effective_stack),
      build_settings_(build_settings), // Copy settings
      all_in_threshold_ratio_(all_in_threshold_ratio) {

    // Basic validation
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

// Setters (SetInitialOopCommit, SetInitialIpCommit) are defined inline in the header file (rule.h)

} // namespace config
} // namespace poker_solver
