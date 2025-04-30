#include "nodes/ShowdownNode.h" // Adjust path if necessary

#include <utility> // For std::move
#include <vector>
#include <numeric> // For std::accumulate (optional check)
#include <cmath>   // For std::abs (optional check)
#include <stdexcept> // For exceptions
#include <sstream>   // For error messages
#include <iostream>  // For std::cerr warning

namespace poker_solver {
namespace nodes {

// Use alias
namespace core = poker_solver::core;

// --- Constructor ---

ShowdownNode::ShowdownNode(core::GameRound round,
                           double pot,
                           std::weak_ptr<GameTreeNode> parent,
                           size_t num_players,
                           const std::vector<double>& initial_commitments)
    : core::GameTreeNode(round, pot, std::move(parent)) {

    // Validate inputs
    if (round != core::GameRound::kRiver) {
         // While technically possible, showdown usually implies river in Hold'em solvers
         // Print a warning rather than throwing an error.
         std::cerr << "[WARNING] ShowdownNode created for round other than River: "
                   << core::GameTreeNode::GameRoundToString(round) << std::endl;
    }
    // Currently restrict to 2 players as payoff calculation is specific
    if (num_players != 2) {
         throw std::invalid_argument("ShowdownNode currently only supports 2 players.");
    }
     if (initial_commitments.size() != num_players) {
         std::ostringstream oss;
         oss << "Number of players (" << num_players
             << ") does not match size of initial_commitments vector ("
             << initial_commitments.size() << ").";
        throw std::invalid_argument(oss.str());
     }
     // Check for negative commitments
     for(double commit : initial_commitments) {
         if (commit < 0.0) {
             throw std::invalid_argument("Initial commitments cannot be negative.");
         }
     }

    // Pre-calculate the payoff vectors
    CalculatePayoffs(num_players, initial_commitments);
}

// --- Payoff Calculation ---

void ShowdownNode::CalculatePayoffs(size_t num_players,
                                    const std::vector<double>& commitments) {
    // This implementation assumes num_players == 2
    double p0_commit = commitments[0]; // IP
    double p1_commit = commitments[1]; // OOP
    // Note: GetPot() returns the *final* pot size at showdown, which includes
    // both players' final commitments.

    // Resize payoff vectors
    payoffs_p0_wins_.resize(num_players);
    payoffs_p1_wins_.resize(num_players);
    payoffs_tie_.resize(num_players);

    // Calculate net payoffs (amount won/lost relative to start of hand)
    // If P0 wins, their net change is the amount P1 committed.
    // If P1 wins, their net change is the amount P0 committed.

    // Payoffs if Player 0 (IP) wins:
    payoffs_p0_wins_[0] = p1_commit;  // P0 wins P1's total commitment
    payoffs_p0_wins_[1] = -p1_commit; // P1 loses their total commitment

    // Payoffs if Player 1 (OOP) wins:
    payoffs_p1_wins_[0] = -p0_commit; // P0 loses their total commitment
    payoffs_p1_wins_[1] = p0_commit;  // P1 wins P0's total commitment

    // Payoffs if Tie: Each player gets back their own contribution from the pot.
    // Net change = amount_returned - amount_committed
    // Since pot = p0_commit + p1_commit, each gets pot/2 back.
    // Net P0 = (pot/2) - p0_commit = (p0_commit + p1_commit)/2 - p0_commit = (p1_commit - p0_commit)/2
    // Net P1 = (pot/2) - p1_commit = (p0_commit + p1_commit)/2 - p1_commit = (p0_commit - p1_commit)/2
    // Alternatively, simpler: P0 gets back half of P1's money, P1 gets back half of P0's money.
    payoffs_tie_[0] = p1_commit / 2.0 - p0_commit / 2.0; // Net change for P0
    payoffs_tie_[1] = p0_commit / 2.0 - p1_commit / 2.0; // Net change for P1

    // Optional Sanity Check: Payoffs should sum to zero across players for each outcome
    #ifndef NDEBUG // Only run checks in debug builds
        constexpr double epsilon = 1e-9; // Tolerance for floating point
        double sum_p0_wins = payoffs_p0_wins_[0] + payoffs_p0_wins_[1];
        double sum_p1_wins = payoffs_p1_wins_[0] + payoffs_p1_wins_[1];
        double sum_tie = payoffs_tie_[0] + payoffs_tie_[1];
        if (std::abs(sum_p0_wins) > epsilon || std::abs(sum_p1_wins) > epsilon || std::abs(sum_tie) > epsilon) {
             std::cerr << "[WARNING] ShowdownNode payoffs do not sum to zero!"
                       << " P0WinSum: " << sum_p0_wins
                       << ", P1WinSum: " << sum_p1_wins
                       << ", TieSum: " << sum_tie << std::endl;
             // Consider throwing std::logic_error if this indicates a major problem
        }
    #endif
}


// --- Payoff Retrieval ---

// Use fully qualified name for the parameter type
const std::vector<double>& ShowdownNode::GetPayoffs(
    core::ComparisonResult result) const {
    switch (result) {
        // Use fully qualified names for enum values
        case core::ComparisonResult::kPlayer1Wins: // Player 0 (IP) wins
            return payoffs_p0_wins_;
        case core::ComparisonResult::kPlayer2Wins: // Player 1 (OOP) wins
            return payoffs_p1_wins_;
        case core::ComparisonResult::kTie:
            return payoffs_tie_;
        default:
            // Should not happen with valid ComparisonResult enum
             throw std::logic_error("Invalid ComparisonResult passed to ShowdownNode::GetPayoffs.");
    }
}


} // namespace nodes
} // namespace poker_solver
