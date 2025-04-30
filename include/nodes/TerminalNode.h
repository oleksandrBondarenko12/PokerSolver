#ifndef POKER_SOLVER_NODES_TERMINAL_NODE_H_
#define POKER_SOLVER_NODES_TERMINAL_NODE_H_

#include "GameTreeNode.h" // Base class and enums
#include <vector>
#include <memory>    // For std::shared_ptr
#include <stdexcept> // For exceptions
#include <sstream>   // For error messages
#include <cstddef>   // For size_t

namespace poker_solver {
namespace nodes {

// Represents a terminal node in the game tree where the hand ends due to
// one player folding, resulting in the other player winning the pot uncontested.
class TerminalNode : public core::GameTreeNode {
 public:
  // Constructor.
  // Args:
  //   payoffs: A vector where index `i` contains the net payoff for player `i`.
  //            This should reflect the final win/loss based on commitments.
  //   round: The game round in which the fold occurred.
  //   pot: The final pot size when the fold occurred.
  //   parent: Weak pointer to the parent node (likely an ActionNode).
  // Throws:
  //   std::invalid_argument if payoffs vector is empty.
  TerminalNode(std::vector<double> payoffs,
               core::GameRound round,
               double pot,
               std::weak_ptr<GameTreeNode> parent);

  // Virtual destructor.
  ~TerminalNode() override = default;

  // --- Overridden Methods ---
  core::GameTreeNodeType GetNodeType() const override {
    return core::GameTreeNodeType::kTerminal;
  }

  // --- Payoff Retrieval ---

  // Gets the vector of payoffs for all players for this terminal state.
  // Returns:
  //   A const reference to the vector where index `i` contains the net payoff
  //   for player `i`.
  const std::vector<double>& GetPayoffs() const { return payoffs_; }

 private:
  // --- Member Variables ---

  // Stores the net payoff for each player (index corresponds to player index).
  std::vector<double> payoffs_;

  // Note: The original TerminalNode stored a 'winner' index. This isn't
  // strictly necessary if the payoffs vector correctly reflects the win/loss
  // for each player (e.g., for player 0 winning P1's commitment, payoffs[0]
  // would be positive, payoffs[1] negative). We'll rely on the payoffs vector.

  // Deleted copy/move operations.
  TerminalNode(const TerminalNode&) = delete;
  TerminalNode& operator=(const TerminalNode&) = delete;
  TerminalNode(TerminalNode&&) = delete;
  TerminalNode& operator=(TerminalNode&&) = delete;
};

} // namespace nodes
} // namespace poker_solver

#endif // POKER_SOLVER_NODES_TERMINAL_NODE_H_
