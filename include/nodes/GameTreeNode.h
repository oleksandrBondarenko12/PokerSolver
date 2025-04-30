#ifndef POKER_SOLVER_CORE_GAME_TREE_NODE_H_
#define POKER_SOLVER_CORE_GAME_TREE_NODE_H_

#include <cstdint>
#include <memory> // For std::shared_ptr, std::weak_ptr, std::enable_shared_from_this
#include <string>
#include <vector>
#include <stdexcept> // For std::runtime_error, std::out_of_range
#include <sstream>   // For ostringstream

namespace poker_solver {
namespace core {

// Enum representing the different betting rounds in Hold'em.
enum class GameRound { kPreflop = 0, kFlop = 1, kTurn = 2, kRiver = 3 };

// Enum representing the possible types of nodes in the game tree.
enum class GameTreeNodeType { kAction, kChance, kShowdown, kTerminal };

// Enum representing the possible actions a player can take, plus game states.
enum class PokerAction {
  kBegin,      // Represents the absolute start of the game/tree
  kRoundBegin, // Represents the start of a betting round
  kBet,
  kRaise,
  kCheck,
  kFold,
  kCall
};


// Abstract base class for all nodes in the poker game tree.
// Inherits from enable_shared_from_this to allow nodes to safely create
// shared_ptrs to themselves (useful for setting parent pointers).
class GameTreeNode : public std::enable_shared_from_this<GameTreeNode> {
 public:
  // Virtual destructor is crucial for base classes with virtual functions.
  virtual ~GameTreeNode() = default;

  // --- Accessors ---

  // Returns the type of this specific node. Must be implemented by subclasses.
  virtual GameTreeNodeType GetNodeType() const = 0;

  // Returns the betting round this node belongs to.
  GameRound GetRound() const { return round_; }

  // Returns the total pot size at this node.
  double GetPot() const { return pot_; }

  // Returns a shared pointer to the parent node (may be expired/null if root).
  std::shared_ptr<GameTreeNode> GetParent() const { return parent_.lock(); }

  // Sets the parent node (used during tree construction).
  // Takes a shared_ptr but stores it as a weak_ptr.
  void SetParent(std::shared_ptr<GameTreeNode> parent);

  // Returns the depth of the node in the tree (root is 0). Set during tree analysis.
  int GetDepth() const { return depth_; }
  void SetDepth(int depth) { depth_ = depth; }

  // Returns the total number of nodes in the subtree rooted at this node. Set during tree analysis.
  int GetSubtreeSize() const { return subtree_size_; }
  void SetSubtreeSize(int size) { subtree_size_ = size; }

  // --- Static Helpers ---

  // Converts an integer (0-3) to a GameRound enum.
  // Throws std::out_of_range if the integer is invalid.
  static GameRound IntToGameRound(int round_int);

  // Converts a GameRound enum to its integer representation (0-3).
  static int GameRoundToInt(GameRound game_round);

  // Converts a GameRound enum to its string representation.
  static std::string GameRoundToString(GameRound game_round);

 protected:
  // Constructor for subclasses.
  // Args:
  //   round: The betting round.
  //   pot: The pot size at this node.
  //   parent: A weak_ptr to the parent node (to avoid circular references).
  //           Typically created from a shared_ptr using std::weak_ptr<GameTreeNode>(shared_parent).
  GameTreeNode(GameRound round, double pot,
               std::weak_ptr<GameTreeNode> parent);

  // Default constructor (protected to prevent direct instantiation).
  GameTreeNode() = default;

 private:
  // --- Member Variables ---
  GameRound round_ = GameRound::kPreflop; // Default to preflop
  double pot_ = 0.0;
  std::weak_ptr<GameTreeNode> parent_; // Use weak_ptr to break cycles

  // Metadata often calculated after tree construction
  int depth_ = -1;        // Depth in the tree (root = 0)
  int subtree_size_ = 0; // Number of nodes in the subtree rooted here
};

} // namespace core
} // namespace poker_solver

#endif // POKER_SOLVER_CORE_GAME_TREE_NODE_H_
