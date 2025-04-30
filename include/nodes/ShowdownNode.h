#ifndef POKER_SOLVER_NODES_SHOWDOWN_NODE_H_
#define POKER_SOLVER_NODES_SHOWDOWN_NODE_H_

#include "nodes/GameTreeNode.h" // Base class and enums
#include "compairer/Compairer.h"      // <<< ADD THIS INCLUDE for ComparisonResult
#include <vector>
#include <memory>    // For std::shared_ptr
#include <stdexcept> // For exceptions
#include <sstream>   // For error messages
#include <cstddef>   // For size_t

namespace poker_solver {
namespace nodes {

// Represents a terminal node in the game tree where the hand outcome is
// determined by comparing the hands of the remaining players (showdown).
class ShowdownNode : public core::GameTreeNode {
 public:
  // Constructor.
  // Args:
  //   round: The final betting round (should always be kRiver).
  //   pot: The final pot size at showdown.
  //   parent: Weak pointer to the parent node.
  //   num_players: The number of players involved in the game (usually 2).
  //   initial_commitments: Vector containing the total amount committed by each
  //                        player throughout the hand (e.g., {ip_commit, oop_commit}).
  //                        Used to calculate net payoffs.
  // Throws:
  //   std::invalid_argument if num_players != initial_commitments.size() or
  //                         if num_players is not 2 (currently).
  ShowdownNode(core::GameRound round,
               double pot,
               std::weak_ptr<GameTreeNode> parent,
               size_t num_players,
               const std::vector<double>& initial_commitments);


  // Virtual destructor.
  ~ShowdownNode() override = default;

  // --- Overridden Methods ---
  core::GameTreeNodeType GetNodeType() const override {
    return core::GameTreeNodeType::kShowdown;
  }

  // --- Payoff Retrieval ---

  // Gets the vector of payoffs for all players based on the showdown outcome.
  // Args:
  //   result: The result of the hand comparison (kTie, kPlayer1Wins, kPlayer2Wins).
  // Returns:
  //   A const reference to the vector where index `i` contains the net payoff for player `i`.
  // Throws:
  //   std::logic_error if called before payoffs are calculated (shouldn't happen).
  const std::vector<double>& GetPayoffs(core::ComparisonResult result) const; // Use qualified name


 private:
  // Calculates and stores the payoff vectors based on initial commitments.
  void CalculatePayoffs(size_t num_players, const std::vector<double>& commitments);

  // --- Member Variables ---

  // Payoffs if player 0 (IP) wins outright. Index is player index.
  std::vector<double> payoffs_p0_wins_;
  // Payoffs if player 1 (OOP) wins outright. Index is player index.
  std::vector<double> payoffs_p1_wins_;
  // Payoffs if the result is a tie. Index is player index.
  std::vector<double> payoffs_tie_;

  // Deleted copy/move operations.
  ShowdownNode(const ShowdownNode&) = delete;
  ShowdownNode& operator=(const ShowdownNode&) = delete;
  ShowdownNode(ShowdownNode&&) = delete;
  ShowdownNode& operator=(ShowdownNode&&) = delete;
};

} // namespace nodes
} // namespace poker_solver

#endif // POKER_SOLVER_NODES_SHOWDOWN_NODE_H_
