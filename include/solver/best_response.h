#ifndef POKER_SOLVER_SOLVER_BEST_RESPONSE_H_
#define POKER_SOLVER_SOLVER_BEST_RESPONSE_H_

#include "Deck.h"
#include "nodes/GameTreeNode.h"
#include "ranges/PrivateCards.h"
#include "ranges/PrivateCardsManager.h"
#include "ranges/RiverRangeManager.h"
#include "tools/utils.h" // For ExchangeColorIsomorphism
#include "nodes/ActionNode.h"
#include <vector>
#include <cstdint>
#include <memory> // For std::shared_ptr
#include <string> // For exception messages

// Forward declare node types (ActionNode already included)
namespace poker_solver { namespace nodes {
    // class ActionNode; // Included above now
    class ChanceNode;
    class ShowdownNode;
    class TerminalNode;
}} // namespace poker_solver::nodes

namespace poker_solver {
namespace solver {

// Calculates the best response strategy and exploitability against a fixed strategy
// profile stored within a game tree. Designed to be thread-safe for calculations
// if configured with multiple threads.
class BestResponse {
 public:
  // Configuration struct for BestResponse behavior
  struct Config {
      bool use_suit_isomorphism = true;
      // Use fully qualified name as ActionNode.h is included after this point potentially
      nodes::ActionNode::TrainablePrecision precision = nodes::ActionNode::TrainablePrecision::kFloat;
      int num_threads = 1;
      bool debug_log = false;
      // Round at which to potentially parallelize chance node calculations
      core::GameRound parallel_split_round = core::GameRound::kFlop;
  };


  // Constructor.
  // Args:
  //   player_ranges: Const reference to the vector containing the initial PrivateCards
  //                  range for each player (e.g., index 0 for P0, 1 for P1).
  //                  These ranges should NOT be filtered by board yet.
  //   pcm: Const reference to the PrivateCardsManager holding range info and lookups.
  //   rrm: Const reference to the RiverRangeManager for cached river evaluations.
  //   deck: Const reference to the Deck object.
  //   config: Configuration settings for the best response calculation.
// Inside class BestResponse { public: ...
  BestResponse(
      const std::vector<std::vector<core::PrivateCards>>& player_ranges,
      const ranges::PrivateCardsManager& pcm,
      ranges::RiverRangeManager& rrm,
      const core::Deck& deck,
      // *** Ensure this line uses Config{} ***
      const Config& config); // Default config

  // Calculates the exploitability of the strategy stored in the tree.
  double CalculateExploitability(
        std::shared_ptr<core::GameTreeNode> root_node,
        uint64_t initial_board_mask,
        double initial_pot) const;


  // Calculates the expected value (EV) for a specific player playing a best
  // response strategy against the fixed strategy in the tree.
  double CalculateBestResponseEv(
      std::shared_ptr<core::GameTreeNode> root_node,
      size_t best_response_player_index,
      uint64_t initial_board_mask) const;


 private:
  // Recursive function to calculate node values for the best responder.
  std::vector<double> CalculateNodeValue(
      std::shared_ptr<core::GameTreeNode> node,
      size_t best_response_player_index,
      const std::vector<std::vector<double>>& reach_probs,
      uint64_t current_board_mask,
      size_t deal_abstraction_index) const;

  // Specific handlers for different node types.
  std::vector<double> HandleActionNode(
      std::shared_ptr<nodes::ActionNode> node,
      size_t best_response_player_index,
      const std::vector<std::vector<double>>& reach_probs,
      uint64_t current_board_mask,
      size_t deal_abstraction_index) const;

  std::vector<double> HandleChanceNode(
       std::shared_ptr<nodes::ChanceNode> node,
       size_t best_response_player_index,
       const std::vector<std::vector<double>>& reach_probs,
       uint64_t current_board_mask,
       size_t deal_abstraction_index) const;

  std::vector<double> HandleTerminalNode(
       std::shared_ptr<nodes::TerminalNode> node,
       size_t best_response_player_index,
       const std::vector<std::vector<double>>& reach_probs,
       uint64_t current_board_mask) const;

  std::vector<double> HandleShowdownNode(
       std::shared_ptr<nodes::ShowdownNode> node,
       size_t best_response_player_index,
       const std::vector<std::vector<double>>& reach_probs,
       uint64_t current_board_mask) const;

   // Helper to get initial reach probabilities considering board/opponent ranges.
   std::vector<std::vector<double>> GetInitialReachProbs(
        uint64_t initial_board_mask) const;

   // Helper to calculate the number of possible deals for a chance node.
   int CalculatePossibleDeals(uint64_t current_board_mask) const;

   // Helper to get the next deal abstraction index (used for isomorphism).
   size_t GetNextDealAbstractionIndex(size_t current_deal_index, int card_index) const;

   // Helper to get all isomorphic deal indices for a given abstract index.
   std::vector<size_t> GetIsomorphicDealIndices(size_t deal_abstraction_index) const;

   // Precompute suit isomorphism mappings if enabled.
   void InitializeIsomorphism();


  // --- Member Variables ---
  const std::vector<std::vector<core::PrivateCards>>& player_ranges_;
  const ranges::PrivateCardsManager& pcm_;
  ranges::RiverRangeManager& rrm_;
  const core::Deck& deck_;
  Config config_;

  static constexpr size_t kMaxDealAbstractionIndex = 3000;
  int suit_iso_offset_[kMaxDealAbstractionIndex][core::kNumSuits];
  std::vector<size_t> player_hand_counts_;
};

} // namespace solver
} // namespace poker_solver

#endif // POKER_SOLVER_SOLVER_BEST_RESPONSE_H_
