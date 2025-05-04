#ifndef POKER_SOLVER_NODES_ACTION_NODE_H_
#define POKER_SOLVER_NODES_ACTION_NODE_H_

#include "GameTreeNode.h" // Base class and enums
#include "GameActions.h"   // For GameAction
#include "ranges/PrivateCards.h"  // For PrivateCards (range association)
#include <vector>
#include <memory> // For std::shared_ptr
#include <cstddef> // For size_t
#include <optional> // For optional return values
#include <trainable/Trainable.h>

// Forward declaration for Trainable interface
// namespace poker_solver { namespace solver { class Trainable; } }
// Forward declaration for concrete Trainable types (if needed in header, though unlikely)
// namespace poker_solver { namespace solver { class DiscountedCfrTrainable; } }

namespace poker_solver {
namespace nodes {

// Represents a decision node in the game tree where a player must choose an action.
class ActionNode : public core::GameTreeNode {
 public:
  // Enum to specify float precision for Trainable objects (if needed)
  enum class TrainablePrecision { kFloat, kHalf, kSingle }; // Example

  // Constructor.
  // Args:
  //   player_index: The index of the player whose turn it is (0=IP, 1=OOP).
  //   round: The current game round.
  //   pot: The pot size at this node.
  //   parent: Weak pointer to the parent node.
  //   num_possible_deals: The number of different abstract chance outcomes
  //                       that can lead to this game state. This determines
  //                       the size of the trainables_ vector for imperfect recall.
  //                       Defaults to 1 for perfect recall scenarios.
  ActionNode(size_t player_index,
             core::GameRound round,
             double pot,
             std::weak_ptr<GameTreeNode> parent,
             size_t num_possible_deals = 1); // Default to 1 trainable state

  // Virtual destructor.
  ~ActionNode() override = default;

  // --- Overridden Methods ---
  core::GameTreeNodeType GetNodeType() const override {
    return core::GameTreeNodeType::kAction;
  }

  // --- Accessors ---
  size_t GetPlayerIndex() const { return player_index_; }
  const std::vector<core::GameAction>& GetActions() const { return actions_; }
  const std::vector<std::shared_ptr<GameTreeNode>>& GetChildren() const {
    return children_;
  }

  // --- Modifiers (Used during tree construction) ---
  void AddChild(core::GameAction action, std::shared_ptr<GameTreeNode> child);
  // Sets the actions and children directly (less common than AddChild)
  void SetActionsAndChildren(
      std::vector<core::GameAction> actions,
      std::vector<std::shared_ptr<GameTreeNode>> children);

  // --- Trainable Management ---

  // Associates the relevant player range with this node.
  // This pointer's lifetime must be managed externally (e.g., by the Solver).
  void SetPlayerRange(const std::vector<core::PrivateCards>* player_range);

  // Gets the Trainable object associated with a specific deal abstraction index.
  // Lazily creates the Trainable object if it doesn't exist.
  // Args:
  //   deal_index: The index representing the abstract chance outcome (0 for
  //               perfect recall, 0 to num_possible_deals-1 for imperfect recall).
  //   precision: The desired float precision for the Trainable object.
  // Returns:
  //   A shared pointer to the Trainable object.
  // Throws:
  //   std::out_of_range if deal_index is invalid.
  //   std::runtime_error if player_range_ has not been set.
  //   std::bad_alloc if creation fails.
  std::shared_ptr<solver::Trainable> GetTrainable(
      size_t deal_index,
      TrainablePrecision precision = TrainablePrecision::kFloat);

  // Gets the Trainable object without creating it if it doesn't exist.
  std::shared_ptr<solver::Trainable> GetTrainableIfExists(size_t deal_index) const;


 private:
  size_t player_index_; // Player whose turn it is (0=IP, 1=OOP)
  std::vector<core::GameAction> actions_; // Possible actions from this node
  std::vector<std::shared_ptr<GameTreeNode>> children_; // Resulting child nodes

  // Pointer to the relevant player range (must be set before GetTrainable).
  // Lifetime managed externally. Raw pointer is acceptable if lifetime is guaranteed.
  const std::vector<core::PrivateCards>* player_range_ = nullptr;

  // Strategy/regret storage. Vector size depends on abstraction/recall level.
  // Size is determined by num_possible_deals in the constructor.
  std::vector<std::shared_ptr<solver::Trainable>> trainables_;

  // Deleted copy/move operations because we manage shared_ptrs and a raw pointer.
  ActionNode(const ActionNode&) = delete;
  ActionNode& operator=(const ActionNode&) = delete;
  ActionNode(ActionNode&&) = delete;
  ActionNode& operator=(ActionNode&&) = delete;
};

} // namespace nodes
} // namespace poker_solver

#endif // POKER_SOLVER_NODES_ACTION_NODE_H_
