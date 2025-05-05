#ifndef POKER_SOLVER_TREE_GAME_TREE_H_
#define POKER_SOLVER_TREE_GAME_TREE_H_

#include "Deck.h"                          // For Deck
#include "nodes/GameTreeNode.h"                // For GameTreeNode and enums
#include "nodes/GameActions.h"                  // <<< ADD THIS INCLUDE
#include "tools/Rule.h"                        // For Rule
#include "tools/GameTreeBuildingSettings.h" // For settings used in build
#include <string>
#include <vector>
#include <memory> // For std::shared_ptr
#include <cstdint> // For uint64_t
#include <optional> // For std::optional
#include <json.hpp> // For JSON loading

// Forward declarations for node types
namespace poker_solver { namespace nodes {
    class ActionNode;
    class ChanceNode;
    class ShowdownNode;
    class TerminalNode;
}} // namespace poker_solver::nodes

namespace poker_solver {
namespace tree {

// Represents the entire game tree for a specific poker scenario.
class GameTree {
 public:
  // Constructor for loading a pre-built tree from a JSON file.
  GameTree(const std::string& json_filepath, const core::Deck& deck);

  // Constructor for building the tree dynamically based on rules.
  explicit GameTree(const config::Rule& rule);

  // --- Accessors ---
  std::shared_ptr<core::GameTreeNode> GetRoot() const { return root_; }
  const core::Deck& GetDeck() const { return deck_; }

  // --- Tree Analysis ---
  void CalculateTreeMetadata();
  void PrintTree(int max_depth = -1) const;
  uint64_t EstimateTrainableMemory(size_t p0_range_size, size_t p1_range_size) const;


 private:
  // --- Dynamic Tree Building Helpers ---
  // Pass Rule by value because commitments change down branches
  void BuildBranch(std::shared_ptr<core::GameTreeNode> current_node,
                   config::Rule current_rule, // Pass Rule by value
                   const core::GameAction& last_action,
                   int actions_this_round,
                   int raises_this_street);

  // Pass original build rule by const ref as it doesn't change state here
  void BuildChanceNode(std::shared_ptr<nodes::ChanceNode> node,
                       const config::Rule& rule);

  // Pass Rule by value
  void BuildActionNode(std::shared_ptr<nodes::ActionNode> node,
                       config::Rule current_rule_state, // Pass Rule by value
                       const core::GameAction& last_action,
                       int actions_this_round,
                       int raises_this_street);

  // Mark as const
  std::vector<double> GetPossibleBets(
        const config::Rule& rule,
        size_t player_index,
        size_t opponent_index,
        double current_player_commit,
        double opponent_commit,
        double effective_stack,
        const core::GameAction& last_action,
        int raises_this_street
        double pot_before_action) const;

  static double RoundBet(double amount, double min_bet_increment);


  // --- JSON Loading Helpers ---
  std::shared_ptr<core::GameTreeNode> ParseNodeJson(
      const nlohmann::json& node_json,
      std::weak_ptr<core::GameTreeNode> parent);
  std::shared_ptr<nodes::ActionNode> ParseActionNode(
      const nlohmann::json& node_json,
      std::weak_ptr<core::GameTreeNode> parent);
  std::shared_ptr<nodes::ChanceNode> ParseChanceNode(
       const nlohmann::json& node_json,
       std::weak_ptr<core::GameTreeNode> parent);
   std::shared_ptr<nodes::ShowdownNode> ParseShowdownNode(
       const nlohmann::json& node_json,
       std::weak_ptr<core::GameTreeNode> parent);
   std::shared_ptr<nodes::TerminalNode> ParseTerminalNode(
       const nlohmann::json& node_json,
       std::weak_ptr<core::GameTreeNode> parent);


  // --- Tree Metadata Calculation Helpers ---
  int CalculateMetadataRecursive(std::shared_ptr<core::GameTreeNode> node, int depth);
  static void PrintTreeRecursive(const std::shared_ptr<core::GameTreeNode>& node,
                                 int current_depth, int max_depth,
                                 const std::string& prefix);
   // Mark as const
   uint64_t EstimateMemoryRecursive(
       const std::shared_ptr<core::GameTreeNode>& node,
       size_t p0_range_size, size_t p1_range_size,
       size_t num_deals_multiplier) const;


  // --- Member Variables ---
  std::shared_ptr<core::GameTreeNode> root_;
  core::Deck deck_;
  std::optional<config::Rule> build_rule_; // Store the rule used for building

  // Deleted copy/move operations.
  GameTree(const GameTree&) = delete;
  GameTree& operator=(const GameTree&) = delete;
  GameTree(GameTree&&) = delete;
  GameTree& operator=(GameTree&&) = delete;
};

} // namespace tree
} // namespace poker_solver

#endif // POKER_SOLVER_TREE_GAME_TREE_H_
