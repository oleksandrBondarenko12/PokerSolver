#include "nodes/TerminalNode.h" // Adjust path if necessary

#include <utility> // For std::move
#include <numeric> // For std::accumulate (optional check)
#include <cmath>   // For std::abs (optional check)

namespace poker_solver {
namespace nodes {

// Use alias
namespace core = poker_solver::core;

// --- Constructor ---

TerminalNode::TerminalNode(std::vector<double> payoffs,
                           core::GameRound round,
                           double pot,
                           std::weak_ptr<GameTreeNode> parent)
    : core::GameTreeNode(round, pot, std::move(parent)),
      payoffs_(std::move(payoffs)) { // Move the payoffs vector

    if (payoffs_.empty()) {
        throw std::invalid_argument("Payoffs vector cannot be empty for TerminalNode.");
    }

    // Optional Sanity Check: Payoffs in a zero-sum game should sum to zero.
    // double sum = std::accumulate(payoffs_.begin(), payoffs_.end(), 0.0);
    // constexpr double epsilon = 1e-9; // Tolerance for floating point
    // if (std::abs(sum) > epsilon) {
    //     std::cerr << "[WARNING] TerminalNode payoffs do not sum to zero (Sum: "
    //               << sum << ")" << std::endl;
    //     // Depending on requirements, could throw std::logic_error here.
    // }
}

// GetPayoffs() is implemented inline in the header file.

} // namespace nodes
} // namespace poker_solver
