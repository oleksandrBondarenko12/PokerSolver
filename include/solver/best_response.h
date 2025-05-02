///////////////////////////////////////////////////////////////////////////////
// include/solver/best_response.h (Corrected HandleTerminalNode Signature)
///////////////////////////////////////////////////////////////////////////////
#ifndef POKER_SOLVER_SOLVER_BEST_RESPONSE_H_
#define POKER_SOLVER_SOLVER_BEST_RESPONSE_H_

#include "nodes/ActionNode.h"        // Includes GameTreeNode, GameRound, PokerAction
#include "ranges/PrivateCards.h"
#include "compairer/Compairer.h"         // For ComparisonResult
#include "Card.h"                  // For kNumSuits, kNumCardsInDeck (use constants)

#include <vector>
#include <cstdint>
#include <memory>                   // For std::shared_ptr
#include <string>                   // For exception messages
#include <array>                    // For suit isomorphism offset storage
#include <stdexcept>                // For exception types

// Forward declarations
namespace poker_solver { namespace core { class Deck; }}
namespace poker_solver { namespace nodes { class ChanceNode; class ShowdownNode; class TerminalNode; }}
namespace poker_solver { namespace ranges { class PrivateCardsManager; class RiverRangeManager; }}

namespace poker_solver {
namespace solver {

// Configuration struct for BestResponse behavior
struct BestResponseConfig {
  bool use_suit_isomorphism = true;
  nodes::ActionNode::TrainablePrecision precision = nodes::ActionNode::TrainablePrecision::kFloat;
  int num_threads = 1;
  bool debug_log = false;
  core::GameRound parallel_split_round = core::GameRound::kFlop;
};


// Calculates the best response strategy and exploitability against a fixed strategy
// profile stored within a game tree.
class BestResponse {
 public:
  using Config = BestResponseConfig;

  explicit BestResponse(const Config& config = Config{});
  ~BestResponse() = default;

  double CalculateExploitability(
      const std::shared_ptr<core::GameTreeNode>& root_node,
      const std::vector<std::vector<core::PrivateCards>>& player_ranges,
      const ranges::PrivateCardsManager& pcm,
      ranges::RiverRangeManager& rrm,
      const core::Deck& deck,
      uint64_t initial_board_mask,
      double initial_pot) const;

  double CalculateBestResponseEv(
      const std::shared_ptr<core::GameTreeNode>& root_node,
      size_t best_response_player_index,
      const std::vector<std::vector<core::PrivateCards>>& player_ranges,
      const ranges::PrivateCardsManager& pcm,
      ranges::RiverRangeManager& rrm,
      const core::Deck& deck,
      uint64_t initial_board_mask) const;

 private:
  void CalculateNodeValue(
      const std::shared_ptr<core::GameTreeNode>& node,
      size_t best_response_player_index,
      const std::vector<std::vector<double>>& reach_probs,
      uint64_t current_board_mask,
      size_t deal_abstraction_index,
      const std::vector<std::vector<core::PrivateCards>>& player_ranges,
      const ranges::PrivateCardsManager& pcm,
      ranges::RiverRangeManager& rrm,
      const core::Deck& deck,
      std::vector<double>& out_evs) const;

  void HandleActionNode(
      const std::shared_ptr<nodes::ActionNode>& node,
      size_t best_response_player_index,
      const std::vector<std::vector<double>>& reach_probs,
      uint64_t current_board_mask,
      size_t deal_abstraction_index,
      const std::vector<std::vector<core::PrivateCards>>& player_ranges,
      const ranges::PrivateCardsManager& pcm,
      ranges::RiverRangeManager& rrm,
      const core::Deck& deck,
      std::vector<double>& out_evs) const;

  void HandleChanceNode(
       const std::shared_ptr<nodes::ChanceNode>& node,
       size_t best_response_player_index,
       const std::vector<std::vector<double>>& reach_probs,
       uint64_t current_board_mask,
       size_t deal_abstraction_index,
       const std::vector<std::vector<core::PrivateCards>>& player_ranges,
       const ranges::PrivateCardsManager& pcm,
       ranges::RiverRangeManager& rrm,
       const core::Deck& deck,
       std::vector<double>& out_evs) const;

  // *** UPDATED HandleTerminalNode Signature ***
  void HandleTerminalNode(
       const std::shared_ptr<nodes::TerminalNode>& node,
       size_t best_response_player_index,
       const std::vector<std::vector<double>>& reach_probs, // Keep for consistency?
       uint64_t current_board_mask,                         // Keep for consistency?
       // Added dependencies needed for precise weighting:
       const std::vector<std::vector<core::PrivateCards>>& player_ranges,
       const ranges::PrivateCardsManager& pcm,
       // Output parameter:
       std::vector<double>& out_evs) const;

  void HandleShowdownNode(
       const std::shared_ptr<nodes::ShowdownNode>& node,
       size_t best_response_player_index,
       const std::vector<std::vector<double>>& reach_probs,
       uint64_t current_board_mask,
       const std::vector<std::vector<core::PrivateCards>>& player_ranges,
       ranges::RiverRangeManager& rrm,
       std::vector<double>& out_evs) const;


  int CalculatePossibleDeals(uint64_t current_board_mask, const core::Deck& deck) const;
  size_t GetNextDealAbstractionIndex(size_t current_deal_index, int card_index) const;
  void InitializeIsomorphism();

  // --- Member Variables ---
  Config config_;
  mutable std::vector<size_t> player_hand_counts_;
  static constexpr size_t kMaxIsoIndex = core::kNumCardsInDeck * core::kNumCardsInDeck;
  std::vector<std::array<int, core::kNumSuits>> suit_iso_offset_;

  BestResponse(const BestResponse&) = delete;
  BestResponse& operator=(const BestResponse&) = delete;
  BestResponse(BestResponse&&) = delete;
  BestResponse& operator=(BestResponse&&) = delete;
};

} // namespace solver
} // namespace poker_solver

#endif // POKER_SOLVER_SOLVER_BEST_RESPONSE_H_
