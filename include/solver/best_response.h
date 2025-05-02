///////////////////////////////////////////////////////////////////////////////
// include/solver/best_response.h (Corrected)
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
#include <stdexcept>                // Added for exception types potentially used

// Forward declarations
namespace poker_solver { namespace core { class Deck; }}
namespace poker_solver { namespace nodes { class ChanceNode; class ShowdownNode; class TerminalNode; }}
namespace poker_solver { namespace ranges { class PrivateCardsManager; class RiverRangeManager; }}

namespace poker_solver {
namespace solver {

// *** MOVED CONFIG STRUCT OUTSIDE AND BEFORE BestResponse ***
// Configuration struct for BestResponse behavior
struct BestResponseConfig { // Renamed slightly for clarity (optional)
  bool use_suit_isomorphism = true;
  // Use fully qualified name for nested enum
  nodes::ActionNode::TrainablePrecision precision = nodes::ActionNode::TrainablePrecision::kFloat;
  int num_threads = 1;
  bool debug_log = false;
  // Round at which to potentially parallelize chance node calculations
  core::GameRound parallel_split_round = core::GameRound::kFlop;
};


// Calculates the best response strategy and exploitability against a fixed strategy
// profile stored within a game tree. Designed to be thread-safe for calculations
// if configured with multiple threads (using OpenMP in the implementation).
class BestResponse {
 public:
  // Use the Config struct defined outside
  using Config = BestResponseConfig; // Optional: create an alias inside if preferred

  // Constructor - now simpler, only takes configuration.
  // Default argument uses the externally defined Config struct.
  explicit BestResponse(const Config& config = Config{});

  // Default destructor is sufficient.
  ~BestResponse() = default;

  // Calculates the overall exploitability (average EV gain for both players playing BR).
  // Dependencies are passed in. Marked const.
  double CalculateExploitability(
      const std::shared_ptr<core::GameTreeNode>& root_node,
      const std::vector<std::vector<core::PrivateCards>>& player_ranges,
      const ranges::PrivateCardsManager& pcm,
      ranges::RiverRangeManager& rrm, // RRM might be modified (cache)
      const core::Deck& deck,
      uint64_t initial_board_mask,
      double initial_pot) const;

  // Calculates the expected value (EV) for a specific player playing a best
  // response strategy against the fixed strategy in the tree.
  // Dependencies are passed in. Marked const.
  double CalculateBestResponseEv(
      const std::shared_ptr<core::GameTreeNode>& root_node,
      size_t best_response_player_index,
      const std::vector<std::vector<core::PrivateCards>>& player_ranges,
      const ranges::PrivateCardsManager& pcm,
      ranges::RiverRangeManager& rrm, // RRM might be modified (cache)
      const core::Deck& deck,
      uint64_t initial_board_mask) const;

 private:
  // Recursive function to calculate node values (per hand) for the best responder.
  // Takes output vector by reference. Passes dependencies down. Marked const.
  void CalculateNodeValue(
      const std::shared_ptr<core::GameTreeNode>& node,
      size_t best_response_player_index,
      const std::vector<std::vector<double>>& reach_probs,
      uint64_t current_board_mask,
      size_t deal_abstraction_index,
      // Dependencies needed by helpers:
      const std::vector<std::vector<core::PrivateCards>>& player_ranges,
      const ranges::PrivateCardsManager& pcm,
      ranges::RiverRangeManager& rrm,
      const core::Deck& deck,
      // Output parameter:
      std::vector<double>& out_evs) const;

  // Specific handlers for different node types. Take output vector by reference. Marked const.
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

  void HandleTerminalNode(
       const std::shared_ptr<nodes::TerminalNode>& node,
       size_t best_response_player_index,
       // Unused parameters marked maybe_unused if compiler supports C++17
       [[maybe_unused]] const std::vector<std::vector<double>>& reach_probs,
       [[maybe_unused]] uint64_t current_board_mask,
       std::vector<double>& out_evs) const; // Output parameter

  void HandleShowdownNode(
       const std::shared_ptr<nodes::ShowdownNode>& node,
       size_t best_response_player_index,
       const std::vector<std::vector<double>>& reach_probs,
       uint64_t current_board_mask,
       // Dependencies needed for RRM call:
       const std::vector<std::vector<core::PrivateCards>>& player_ranges,
       ranges::RiverRangeManager& rrm,
       std::vector<double>& out_evs) const; // Output parameter


  // Helper to calculate the number of possible deals for a chance node. Marked const.
  int CalculatePossibleDeals(uint64_t current_board_mask, const core::Deck& deck) const;

  // Helper to get the next deal abstraction index (used for isomorphism). Marked const.
  // Placeholder implementation.
  size_t GetNextDealAbstractionIndex(size_t current_deal_index, int card_index) const;

  // Precompute suit isomorphism mappings if enabled.
  // Placeholder implementation.
  void InitializeIsomorphism();

  // --- Member Variables ---
  Config config_; // Store configuration (uses the struct defined outside)

  // Store range sizes temporarily during calculation (set in public methods)
  // Marked mutable to allow modification in const public methods.
  mutable std::vector<size_t> player_hand_counts_;

  // Use std::vector<std::array> for isomorphism offsets. Correct size needed.
  // Determine max index based on how deal_abstraction_index is calculated.
  // Using 52*52 for now, assuming max 2 cards dealt abstractly? Needs clarification.
  static constexpr size_t kMaxIsoIndex = core::kNumCardsInDeck * core::kNumCardsInDeck; // Adjust if needed
  std::vector<std::array<int, core::kNumSuits>> suit_iso_offset_;

  // Deleted copy/move operations. BestResponse is configured once and used.
  BestResponse(const BestResponse&) = delete;
  BestResponse& operator=(const BestResponse&) = delete;
  BestResponse(BestResponse&&) = delete;
  BestResponse& operator=(BestResponse&&) = delete;
};

} // namespace solver
} // namespace poker_solver

#endif // POKER_SOLVER_SOLVER_BEST_RESPONSE_H_