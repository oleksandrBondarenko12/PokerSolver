#ifndef POKER_SOLVER_NODES_CHANCE_NODE_H_
#define POKER_SOLVER_NODES_CHANCE_NODE_H_

#include "nodes/GameTreeNode.h" // Base class and enums
#include "Card.h"          // For Card
#include <vector>
#include <memory> // For std::shared_ptr
#include <stdexcept> // For exceptions

namespace poker_solver {
namespace nodes {

// Represents a chance node in the game tree, typically occurring after a
// betting round concludes and community cards are dealt.
// This node stores the specific cards dealt at this chance event and points
// to the single resulting child node.
class ChanceNode : public core::GameTreeNode {
 public:
  // Constructor.
  // Args:
  //   round: The game round *after* these cards are dealt (e.g., kFlop if flop cards were dealt).
  //   pot: The pot size carried over into this node.
  //   parent: Weak pointer to the parent node (usually an ActionNode).
  //   dealt_cards: The specific community card(s) dealt at this chance event.
  //   child_node: The single GameTreeNode that follows this chance event. Can be null initially.
  //   is_donk_opportunity: Flag indicating if the next action node represents
  //                        a potential donk bet spot (OOP betting into PFR).
  ChanceNode(core::GameRound round,
             double pot,
             std::weak_ptr<GameTreeNode> parent,
             std::vector<core::Card> dealt_cards, // Store the dealt cards
             std::shared_ptr<GameTreeNode> child_node);

  // Virtual destructor.
  ~ChanceNode() override = default;

  // --- Overridden Methods ---
  core::GameTreeNodeType GetNodeType() const override {
    return core::GameTreeNodeType::kChance;
  }

  // --- Accessors ---

  // Returns the specific community card(s) dealt at this node.
  const std::vector<core::Card>& GetDealtCards() const { return dealt_cards_; } // Method exists here

  // Returns the single child node that follows this chance event.
  std::shared_ptr<GameTreeNode> GetChild() const { return child_node_; }

  // --- Modifiers (Used during tree construction/linking) ---

  // Sets the child node (primarily used if constructed with null child initially).
  void SetChild(std::shared_ptr<GameTreeNode> child);


 private:
  // The specific community card(s) dealt at this chance event.
  std::vector<core::Card> dealt_cards_;

  // The single child node following this chance event.
  std::shared_ptr<GameTreeNode> child_node_;

  // Deleted copy/move operations.
  ChanceNode(const ChanceNode&) = delete;
  ChanceNode& operator=(const ChanceNode&) = delete;
  ChanceNode(ChanceNode&&) = delete;
  ChanceNode& operator=(ChanceNode&&) = delete;
};

} // namespace nodes
} // namespace poker_solver

#endif // POKER_SOLVER_NODES_CHANCE_NODE_H_
