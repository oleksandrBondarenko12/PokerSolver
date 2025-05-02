///////////////////////////////////////////////////////////////////////////////
// src/solver/best_response.cpp (Re-implemented)
///////////////////////////////////////////////////////////////////////////////
#include "solver/best_response.h"
#include "nodes/ActionNode.h"
#include "nodes/ChanceNode.h"
#include "nodes/ShowdownNode.h"
#include "nodes/TerminalNode.h"
#include "nodes/GameActions.h"
#include "Card.h"
#include "trainable/Trainable.h"
#include "compairer/Compairer.h"
#include "compairer/Dic5Compairer.h" // For kInvalidRank - TODO: move constant?
#include "ranges/PrivateCardsManager.h"
#include "ranges/RiverRangeManager.h"
#include "Deck.h"
#include "tools/utils.h" // For ExchangeColorIsomorphism

#include <vector>
#include <numeric>   // For std::accumulate
#include <cmath>     // For std::abs, std::max, std::isnan, std::pow
#include <limits>    // For std::numeric_limits
#include <stdexcept> // For exceptions
#include <sstream>   // For error messages
#include <iostream>  // For debug/warning logging
#include <iomanip>   // For hex output in logging
#include <utility>   // For std::move
#include <algorithm> // For std::fill, std::max, std::sort
#include <bit>       // For std::popcount (C++20) / builtins

#ifdef _OPENMP // Only include omp.h if OpenMP is enabled
#include <omp.h>
#endif

// Use aliases
namespace core = poker_solver::core;
namespace nodes = poker_solver::nodes;
namespace ranges = poker_solver::ranges;
namespace utils = poker_solver::utils;
namespace eval = poker_solver::eval;

namespace poker_solver {
namespace solver {

// --- Constructor ---

BestResponse::BestResponse(const Config& config)
    : config_(config),
      suit_iso_offset_(kMaxIsoIndex) // Initialize vector size
{
    // Initialize isomorphism table if needed
    if (config_.use_suit_isomorphism) {
        InitializeIsomorphism(); // Needs actual implementation
    } else {
        // Set all offsets to 0 if isomorphism is disabled
        for (auto& offsets : suit_iso_offset_) {
            offsets.fill(0);
        }
    }

    // Set number of threads for OpenMP if enabled
    #ifdef _OPENMP
    if (config_.num_threads > 0) {
        omp_set_num_threads(config_.num_threads);
    } else {
        omp_set_num_threads(1); // Default to 1 thread if invalid config
    }
    #else
    if (config_.num_threads > 1) {
        std::cerr << "[WARNING BR] OpenMP not available/enabled, running single-threaded." << std::endl;
    }
    #endif
}

// --- Public Methods ---

double BestResponse::CalculateExploitability(
    const std::shared_ptr<core::GameTreeNode>& root_node,
    const std::vector<std::vector<core::PrivateCards>>& player_ranges,
    const ranges::PrivateCardsManager& pcm,
    ranges::RiverRangeManager& rrm,
    const core::Deck& deck,
    uint64_t initial_board_mask,
    double initial_pot) const { // Marked const

    if (!root_node) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (initial_pot <= 0) {
         std::cerr << "[WARNING BR] Initial pot is non-positive (" << initial_pot
                   << ") in CalculateExploitability. Result might be misleading." << std::endl;
    }
     if (player_ranges.size() != 2) {
        throw std::invalid_argument("CalculateExploitability currently only supports 2 players.");
    }

    // Store hand counts locally for this calculation
    player_hand_counts_.clear(); // Use mutable member
    player_hand_counts_.push_back(player_ranges[0].size());
    player_hand_counts_.push_back(player_ranges[1].size());


    double total_ev_sum = 0.0;

    // Calculate best response EV for Player 0
    double ev_p0 = CalculateBestResponseEv(root_node, 0, player_ranges, pcm, rrm, deck, initial_board_mask);
    if (config_.debug_log) {
        std::cout << "[DEBUG BR] Best Response EV for Player 0: " << ev_p0 << std::endl;
    }
    if (!std::isnan(ev_p0)) { total_ev_sum += ev_p0; }
    else { std::cerr << "[WARNING BR] NaN EV detected for Player 0." << std::endl; }

    // Calculate best response EV for Player 1
    double ev_p1 = CalculateBestResponseEv(root_node, 1, player_ranges, pcm, rrm, deck, initial_board_mask);
    if (config_.debug_log) {
        std::cout << "[DEBUG BR] Best Response EV for Player 1: " << ev_p1 << std::endl;
    }
    if (!std::isnan(ev_p1)) { total_ev_sum += ev_p1; }
    else { std::cerr << "[WARNING BR] NaN EV detected for Player 1." << std::endl; }

    // Exploitability is the average value gained by the best responder.
    double exploitability = total_ev_sum / 2.0;

    if (config_.debug_log) {
        std::cout << "[DEBUG BR] Total Exploitability Value: " << exploitability << std::endl;
        // Convert to mbb/hand if pot represents total money exchanged (e.g., 2*SB for limp pot)
        // This depends on how initial_pot is defined. Assuming it's total BBs in pot:
        // std::cout << "[DEBUG BR] Exploitability (mbb/hand): " << (exploitability * 1000.0) << std::endl;
    }

    // Reset temporary hand counts
    player_hand_counts_.clear(); // Use mutable member

    return exploitability; // Return raw EV exploitability
}


double BestResponse::CalculateBestResponseEv(
    const std::shared_ptr<core::GameTreeNode>& root_node,
    size_t best_response_player_index,
    const std::vector<std::vector<core::PrivateCards>>& player_ranges, // Passed in
    const ranges::PrivateCardsManager& pcm, // Passed in
    ranges::RiverRangeManager& rrm, // Passed in
    const core::Deck& deck, // Passed in
    uint64_t initial_board_mask) const { // Marked const

    if (!root_node) return 0.0;
    if (best_response_player_index >= 2 || player_ranges.size() != 2) {
        throw std::out_of_range("Invalid player index or ranges size in CalculateBestResponseEv.");
    }

    // Use mutable member for hand counts if needed by recursive calls
    if (player_hand_counts_.empty()) {
        player_hand_counts_.push_back(player_ranges[0].size());
        player_hand_counts_.push_back(player_ranges[1].size());
    } else if (player_hand_counts_.size() != 2 ||
               player_hand_counts_[0] != player_ranges[0].size() ||
               player_hand_counts_[1] != player_ranges[1].size())
    {
         throw std::logic_error("Inconsistent player range sizes during BR calculation.");
    }

    // --- Calculate initial reach probabilities considering initial board and opponent blockers ---
    // This calculation is complex and was likely intended to be done by PCM.
    // For now, let's get the *normalized* probabilities from PCM, which already
    // account for blockers relative to the opponent range *before* the board.
    // We still need to zero out probabilities for hands blocked by the initial_board_mask.
    std::vector<std::vector<double>> initial_reach_probs(2);
    initial_reach_probs[0] = pcm.GetInitialReachProbs(0); // Normalized P(H_p0)
    initial_reach_probs[1] = pcm.GetInitialReachProbs(1); // Normalized P(H_p1)

    // Zero out probabilities conflicting with the initial board and re-normalize
    double p0_sum = 0.0;
    double p1_sum = 0.0;
    for(size_t h = 0; h < initial_reach_probs[0].size(); ++h) {
        if (core::Card::DoBoardsOverlap(player_ranges[0][h].GetBoardMask(), initial_board_mask)) {
            initial_reach_probs[0][h] = 0.0;
        }
        p0_sum += initial_reach_probs[0][h];
    }
     for(size_t h = 0; h < initial_reach_probs[1].size(); ++h) {
        if (core::Card::DoBoardsOverlap(player_ranges[1][h].GetBoardMask(), initial_board_mask)) {
            initial_reach_probs[1][h] = 0.0;
        }
        p1_sum += initial_reach_probs[1][h];
    }
    // Re-normalize after removing board-blocked hands
    if (p0_sum > 1e-12) { for(double& p : initial_reach_probs[0]) p /= p0_sum; }
    if (p1_sum > 1e-12) { for(double& p : initial_reach_probs[1]) p /= p1_sum; }

    // --- This 'initial_reach_probs' now represents P(Hand | Board) ---


    // Allocate output vector for the result of the recursive calculation
    std::vector<double> node_values(player_hand_counts_[best_response_player_index]);

    // Calculate node values recursively starting from the root, filling node_values
    CalculateNodeValue(
        root_node,
        best_response_player_index,
        initial_reach_probs, // Pass the board-adjusted reach probs
        initial_board_mask,
        0, // Initial deal abstraction index
        player_ranges, // Pass dependencies down
        pcm,
        rrm,
        deck,
        node_values // Pass output vector by reference
    );

    // *** Use the CORRECT initial reach probabilities for weighting ***
    // The final EV is the sum of per-hand EVs weighted by the probability
    // of having that hand *given the initial board*. This is what we
    // calculated and stored in initial_reach_probs[best_response_player_index].
    const auto& final_reach_probs = initial_reach_probs[best_response_player_index];

    if (node_values.size() != final_reach_probs.size()) {
         std::cerr << "[ERROR BR] Node value vector size (" << node_values.size()
                   << ") does not match final reach prob size (" << final_reach_probs.size()
                   << ") for player " << best_response_player_index << std::endl;
         if (player_hand_counts_.size() == 2) player_hand_counts_.clear(); // Cleanup
         return std::numeric_limits<double>::quiet_NaN();
    }

    // Log the final reach probs sum being used
    if (config_.debug_log) {
        double final_reach_sum_check = std::accumulate(final_reach_probs.begin(), final_reach_probs.end(), 0.0);
        std::cout << "[DEBUG BR Final EV] Using final reach prob sum for player " << best_response_player_index << ": " << final_reach_sum_check << std::endl;
    }


    double total_ev = 0.0;
    for (size_t i = 0; i < node_values.size(); ++i) {
        if (!std::isnan(node_values[i]) && !std::isnan(final_reach_probs[i])) {
            // Log values being summed
            if (config_.debug_log && (node_values[i] != 0.0 || final_reach_probs[i] != 0.0)) {
                 std::cout << "[DEBUG BR Final Sum] HandIdx=" << i
                           << ", NodeValue=" << node_values[i]
                           << ", ReachProb=" << final_reach_probs[i]
                           << ", Contribution=" << node_values[i] * final_reach_probs[i] << std::endl;
            }
            total_ev += node_values[i] * final_reach_probs[i]; // EV = Sum[ EV(hand_i | board) * P(hand_i | board) ]
        } else {
             if (config_.debug_log) {
                 std::cerr << "[WARNING BR] NaN detected during EV calculation. Index: " << i
                           << ", NodeValue: " << node_values[i]
                           << ", ReachProb: " << final_reach_probs[i] << std::endl;
             }
        }
    }


    // Reset temporary hand counts if they were set locally
    if (player_hand_counts_.size() == 2) player_hand_counts_.clear();

    return total_ev;
}


// --- Recursive Calculation Helpers ---

// Takes output vector by reference, marked const
void BestResponse::CalculateNodeValue(
    const std::shared_ptr<core::GameTreeNode>& node, // Use const& for shared_ptr
    size_t best_response_player_index,
    const std::vector<std::vector<double>>& reach_probs,
    uint64_t current_board_mask,
    size_t deal_abstraction_index,
    const std::vector<std::vector<core::PrivateCards>>& player_ranges, // Pass dependencies
    const ranges::PrivateCardsManager& pcm,
    ranges::RiverRangeManager& rrm,
    const core::Deck& deck,
    std::vector<double>& out_evs) const { // Output parameter

    if (!node) {
        std::cerr << "[ERROR BR] CalculateNodeValue called with null node!" << std::endl;
        throw std::logic_error("Encountered null node during BestResponse traversal.");
    }

    // Ensure output vector has the correct size for this player
    if (out_evs.size() != player_hand_counts_[best_response_player_index]) {
        out_evs.resize(player_hand_counts_[best_response_player_index]);
    }

    if (config_.debug_log) {
        std::cout << "[DEBUG BR Enter] Node Type: " << static_cast<int>(node->GetNodeType())
                  << ", BR Player: " << best_response_player_index
                  << ", Board: 0x" << std::hex << current_board_mask << std::dec
                  << ", Node Ptr: " << node.get() << std::endl;
        std::cout << "[DEBUG BR CalculateNodeValue] Processing Node Type (Before Switch): "
                  << static_cast<int>(node->GetNodeType()) << std::endl << std::flush;
    }


    try {
        switch (node->GetNodeType()) {
            case core::GameTreeNodeType::kAction:
                HandleActionNode(
                    std::dynamic_pointer_cast<nodes::ActionNode>(node),
                    best_response_player_index, reach_probs, current_board_mask,
                    deal_abstraction_index, player_ranges, pcm, rrm, deck, out_evs);
                break;
            case core::GameTreeNodeType::kChance:
                HandleChanceNode(
                    std::dynamic_pointer_cast<nodes::ChanceNode>(node),
                    best_response_player_index, reach_probs, current_board_mask,
                    deal_abstraction_index, player_ranges, pcm, rrm, deck, out_evs);
                break;
            case core::GameTreeNodeType::kTerminal:
                // *** Pass player_ranges and pcm to HandleTerminalNode ***
                HandleTerminalNode(
                    std::dynamic_pointer_cast<nodes::TerminalNode>(node),
                    best_response_player_index, reach_probs, current_board_mask,
                    player_ranges, pcm, out_evs); // Pass dependencies + out_evs
                break;
            case core::GameTreeNodeType::kShowdown:
                HandleShowdownNode(
                    std::dynamic_pointer_cast<nodes::ShowdownNode>(node),
                    best_response_player_index, reach_probs, current_board_mask,
                    player_ranges, rrm, out_evs); // Pass dependencies + out_evs
                break;
            default:
                 std::cerr << "[ERROR BR] Unknown node type encountered in CalculateNodeValue: "
                           << static_cast<int>(node->GetNodeType()) << std::endl;
                 throw std::logic_error("Unknown node type encountered in CalculateNodeValue.");
        }
    } catch (const std::exception& e) {
         std::cerr << "[ERROR BR] Exception during CalculateNodeValue for node type "
                   << static_cast<int>(node->GetNodeType()) << ": " << e.what() << std::endl;
         std::fill(out_evs.begin(), out_evs.end(), std::numeric_limits<double>::quiet_NaN());
         throw; // Re-throw
    }
}

// Takes output vector by reference, marked const
void BestResponse::HandleActionNode(
    const std::shared_ptr<nodes::ActionNode>& node,
    size_t best_response_player_index,
    const std::vector<std::vector<double>>& reach_probs,
    uint64_t current_board_mask,
    size_t deal_abstraction_index,
    const std::vector<std::vector<core::PrivateCards>>& player_ranges,
    const ranges::PrivateCardsManager& pcm,
    ranges::RiverRangeManager& rrm,
    const core::Deck& deck,
    std::vector<double>& out_evs) const { // Output parameter

    if (!node) { throw std::logic_error("HandleActionNode called with null node ptr."); }

    size_t acting_player = node->GetPlayerIndex();
    size_t opponent_player = 1 - acting_player;
    size_t num_hands_acting = player_hand_counts_[acting_player];
    size_t num_hands_br = player_hand_counts_[best_response_player_index];
    size_t num_actions = node->GetActions().size();

    // Resize and initialize output EV vector
    if (out_evs.size() != num_hands_br) out_evs.resize(num_hands_br);
    std::fill(out_evs.begin(), out_evs.end(),
              (acting_player == best_response_player_index) ? -std::numeric_limits<double>::infinity() : 0.0);

    if (num_actions == 0) { return; } // No actions, return initialized EV vector

    // --- Allocate temporary vector for child results ---
    std::vector<double> child_evs(num_hands_br);

    for (size_t a = 0; a < num_actions; ++a) {
        const std::shared_ptr<core::GameTreeNode>& child_node = node->GetChildren()[a];
        if (!child_node) { throw std::logic_error("ActionNode has null child pointer."); }

        std::vector<std::vector<double>> next_reach_probs = reach_probs; // Copy current probs

        if (acting_player != best_response_player_index) {
            // --- Get Opponent Strategy & Calculate Next Reach Probs ---
            auto trainable = node->GetTrainableIfExists(deal_abstraction_index);
            // Use const& for strategy vector if GetAverageStrategy returns const&
            const std::vector<float>* fixed_strategy_ptr = nullptr;
            std::vector<float> uniform_strategy; // Only used if trainable is null

            if (trainable) {
                fixed_strategy_ptr = &(trainable->GetAverageStrategy());
            } else {
                 uniform_strategy.assign(num_actions * num_hands_acting,
                                        (num_actions > 0) ? 1.0f / static_cast<float>(num_actions) : 0.0f);
                 fixed_strategy_ptr = &uniform_strategy;
            }
            const std::vector<float>& fixed_strategy = *fixed_strategy_ptr; // Use reference

            if (fixed_strategy.size() != num_actions * num_hands_acting) { throw std::logic_error("Strategy size mismatch."); }

            next_reach_probs[acting_player].assign(num_hands_acting, 0.0);
            double next_reach_sum = 0.0;
            for (size_t h = 0; h < num_hands_acting; ++h) {
                size_t strat_idx = a * num_hands_acting + h;
                if (strat_idx < fixed_strategy.size() && h < reach_probs[acting_player].size()) {
                    next_reach_probs[acting_player][h] = reach_probs[acting_player][h] * static_cast<double>(fixed_strategy[strat_idx]);
                    next_reach_sum += next_reach_probs[acting_player][h];
                }
            }
            if (next_reach_sum > 1e-12) {
                for(double& p : next_reach_probs[acting_player]) { p /= next_reach_sum; }
            } else {
                 next_reach_probs[acting_player].assign(num_hands_acting, 0.0);
            }
            // ---------------------------------------------------------
        }

        // --- Recursive Call (Fills child_evs) ---
        CalculateNodeValue(
            child_node, best_response_player_index, next_reach_probs,
            current_board_mask, deal_abstraction_index,
            player_ranges, pcm, rrm, deck,
            child_evs); // Pass output vector

        // --- Aggregate Results into out_evs ---
        if (acting_player == best_response_player_index) {
            // Best responder maximizes EV over actions
            for (size_t h = 0; h < num_hands_br; ++h) {
                 out_evs[h] = std::max(out_evs[h], child_evs[h]);
            }
        } else {
            // Opponent plays fixed strategy - EV is weighted sum (weighting done via reach probs)
            for (size_t h = 0; h < num_hands_br; ++h) {
                 out_evs[h] += child_evs[h];
            }
        }
    } // End loop over actions
}


// Takes output vector by reference, marked const
// Takes output vector by reference, marked const
// --- Corrected HandleChanceNode ---
void BestResponse::HandleChanceNode(
    const std::shared_ptr<nodes::ChanceNode>& node, // Use const&
    size_t best_response_player_index,
    const std::vector<std::vector<double>>& reach_probs,
    uint64_t current_board_mask, // Board *before* this chance event
    size_t deal_abstraction_index,
    const std::vector<std::vector<core::PrivateCards>>& player_ranges, // Pass dependencies
    const ranges::PrivateCardsManager& pcm,
    ranges::RiverRangeManager& rrm,
    const core::Deck& deck,
    std::vector<double>& out_evs) const { // Output parameter

   if (!node) { throw std::logic_error("HandleChanceNode called with null node ptr."); }
   const std::shared_ptr<core::GameTreeNode>& child_node = node->GetChild(); // Use const&
   if (!child_node) throw std::logic_error("ChanceNode has no child.");

   // Get the card(s) dealt at THIS chance node
   const auto& dealt_cards = node->GetDealtCards();
   if (dealt_cards.empty()) {
       // If no cards dealt, pass state directly.
       if (config_.debug_log) {
            std::cerr << "[WARNING BR] ChanceNode (Round " << static_cast<int>(node->GetRound())
                      << ") has no dealt cards. Passing state to child." << std::endl;
       }
       CalculateNodeValue(child_node, best_response_player_index, reach_probs,
                          current_board_mask, deal_abstraction_index,
                          player_ranges, pcm, rrm, deck, out_evs);
       return;
   }
   // *** Assuming Turn/Flop/River deals only 1 card per node in this tree structure ***
   if (dealt_cards.size() != 1) {
        throw std::logic_error("HandleChanceNode currently only supports 1 card dealt per chance node.");
   }

   const core::Card& next_card = dealt_cards[0];
   if (next_card.IsEmpty()) {
       throw std::logic_error("ChanceNode dealt an empty card.");
   }
   int card_int = next_card.card_int().value();
   uint64_t card_mask = core::Card::CardIntToUint64(card_int);

   // Check for impossible states
   if (core::Card::DoBoardsOverlap(card_mask, current_board_mask)) {
        std::ostringstream oss;
        oss << "[ERROR BR] ChanceNode dealt card (Idx " << card_int
            << ") already present on board mask 0x" << std::hex << current_board_mask;
        throw std::logic_error(oss.str());
   }

   size_t num_hands_p0 = player_hand_counts_[0];
   size_t num_hands_p1 = player_hand_counts_[1];
   size_t num_hands_br = player_hand_counts_[best_response_player_index];

   // Ensure output vector is correct size (will be filled by recursive call)
   if (out_evs.size() != num_hands_br) out_evs.resize(num_hands_br);
   // No need to initialize here, recursive call will overwrite

   // --- Calculate next reach probabilities considering card removal ---
   std::vector<std::vector<double>> next_reach_probs(2);
   next_reach_probs[0].resize(num_hands_p0);
   next_reach_probs[1].resize(num_hands_p1);
   double p0_reach_sum = 0.0; double p1_reach_sum = 0.0;
   bool p0_possible = false; bool p1_possible = false;

   for (size_t h = 0; h < num_hands_p0; ++h) {
       if (core::Card::DoBoardsOverlap(player_ranges[0][h].GetBoardMask(), card_mask)) {
           next_reach_probs[0][h] = 0.0;
       } else {
           next_reach_probs[0][h] = reach_probs[0][h];
           if (next_reach_probs[0][h] > 1e-12) p0_possible = true;
           p0_reach_sum += next_reach_probs[0][h];
       }
   }
   for (size_t h = 0; h < num_hands_p1; ++h) {
        if (core::Card::DoBoardsOverlap(player_ranges[1][h].GetBoardMask(), card_mask)) {
            next_reach_probs[1][h] = 0.0;
        } else {
            next_reach_probs[1][h] = reach_probs[1][h];
            if (next_reach_probs[1][h] > 1e-12) p1_possible = true;
            p1_reach_sum += next_reach_probs[1][h];
        }
   }
   if (!p0_possible || !p1_possible) {
       // If dealing this card makes a range impossible, the EV from this path is 0.
       std::fill(out_evs.begin(), out_evs.end(), 0.0); // Fill output with zeros
       return;
   }
   // Normalize probabilities (important!)
   if (p0_reach_sum > 1e-12) { for(double& p : next_reach_probs[0]) p /= p0_reach_sum; }
   else { next_reach_probs[0].assign(num_hands_p0, 0.0); }
   if (p1_reach_sum > 1e-12) { for(double& p : next_reach_probs[1]) p /= p1_reach_sum; }
   else { next_reach_probs[1].assign(num_hands_p1, 0.0); }
   // ------------------------------------

   uint64_t next_board_mask = current_board_mask | card_mask; // Combine masks
   // TODO: Fix GetNextDealAbstractionIndex if isomorphism is used
   size_t next_deal_abs_index = 0; // Assuming perfect recall for now

   // --- Logging before recursive call ---
   if (config_.debug_log) {
       std::cout << "[DEBUG BR Chance] Curr=0x" << std::hex << current_board_mask
                 << ", Dealt Card=0x" << card_mask << " (Idx " << std::dec << card_int << ")"
                 << ", Next Board=0x" << std::hex << next_board_mask << std::dec << std::endl;
   }

   // --- Recursive Call (fills out_evs directly) ---
   // Pass the *updated* mask (next_board_mask) to the child.
   CalculateNodeValue(
       child_node, best_response_player_index, next_reach_probs,
       next_board_mask, // Pass the combined mask
       next_deal_abs_index,
       player_ranges, pcm, rrm, deck,
       out_evs); // Pass output vector directly

   // --- Isomorphism Handling (If Enabled) ---
   // TODO: If isomorphism is used, the result in out_evs might need to be
   // transformed back using exchange_color based on iso_offset here.

   // No averaging needed as we only processed the single card dealt by this node.
   // No return needed (fills out_evs)
}// Takes output vector by reference, marked const


void BestResponse::HandleTerminalNode(
    const std::shared_ptr<nodes::TerminalNode>& node, // Use const&
    size_t best_response_player_index,
    const std::vector<std::vector<double>>& reach_probs,
    uint64_t current_board_mask, // Now used for blocker checks
    // --- Added Dependencies ---
    const std::vector<std::vector<core::PrivateCards>>& player_ranges,
    const ranges::PrivateCardsManager& pcm,
    // ------------------------
    std::vector<double>& out_evs) const { // Output parameter

   if (!node) { throw std::logic_error("HandleTerminalNode called with null node ptr."); }
   size_t num_hands_br = player_hand_counts_[best_response_player_index];
   size_t opponent_player_index = 1 - best_response_player_index;

   // Ensure output vector is correct size
   if (out_evs.size() != num_hands_br) out_evs.resize(num_hands_br);
   // Initialize later based on calculation

   const std::vector<double>& node_payoffs = node->GetPayoffs(); // Returns const&
   if (node_payoffs.size() != 2) throw std::logic_error("TerminalNode payoff vector size not 2.");
   double payoff_for_br_player = node_payoffs[best_response_player_index];

   // --- Calculate Opponent Reach Sum and Blocker Sums ---
   const auto& oppo_range = player_ranges[opponent_player_index];
   const auto& oppo_reach_prob = reach_probs[opponent_player_index];
   if (oppo_range.size() != oppo_reach_prob.size()) {
       throw std::logic_error("Opponent range size and reach prob size mismatch in HandleTerminalNode.");
   }

   double oppo_total_reach_sum = 0.0;
   std::vector<double> oppo_card_reach_sum(core::kNumCardsInDeck, 0.0);

   for(size_t opp_hand_idx = 0; opp_hand_idx < oppo_range.size(); ++opp_hand_idx) {
       const auto& opp_hand = oppo_range[opp_hand_idx];
       uint64_t opp_mask = opp_hand.GetBoardMask();

       // Skip opponent hands blocked by the current board
       if (core::Card::DoBoardsOverlap(opp_mask, current_board_mask)) {
           continue;
       }

       double current_opp_reach = oppo_reach_prob[opp_hand_idx];
       oppo_total_reach_sum += current_opp_reach;
       oppo_card_reach_sum[opp_hand.Card1Int()] += current_opp_reach;
       oppo_card_reach_sum[opp_hand.Card2Int()] += current_opp_reach;
   }
   // --------------------------------------------------

   // --- Calculate Payoff for Each BR Hand ---
   const auto& br_range = player_ranges[best_response_player_index];
   for(size_t br_hand_idx = 0; br_hand_idx < num_hands_br; ++br_hand_idx) {
       const auto& br_hand = br_range[br_hand_idx];
       uint64_t br_mask = br_hand.GetBoardMask();

       // If BR hand is blocked by board, EV is 0
       if (core::Card::DoBoardsOverlap(br_mask, current_board_mask)) {
           out_evs[br_hand_idx] = 0.0;
           continue;
       }

       // Calculate non-blocking opponent reach probability
       double non_blocking_opp_prob = oppo_total_reach_sum
                                    - oppo_card_reach_sum[br_hand.Card1Int()]
                                    - oppo_card_reach_sum[br_hand.Card2Int()];

       // Add back reach prob if opponent could hold the *same* hand (using PCM lookup)
       // This handles the P(Blocker1 AND Blocker2) term correction implicitly
       std::optional<size_t> opp_equiv_idx = pcm.GetOpponentHandIndex(
                                                   best_response_player_index,
                                                   opponent_player_index,
                                                   br_hand_idx);
       if (opp_equiv_idx.has_value()) {
            // Check if the equivalent opponent hand was itself blocked by the board
            if (!core::Card::DoBoardsOverlap(br_mask, current_board_mask)) { // Use br_mask as it's the same hand
                 if (opp_equiv_idx.value() < oppo_reach_prob.size()) { // Bounds check
                   non_blocking_opp_prob += oppo_reach_prob[opp_equiv_idx.value()];
                 }
            }
       }

       // Ensure probability is not negative due to floating point errors
       non_blocking_opp_prob = std::max(0.0, non_blocking_opp_prob);

       // Final EV for this hand = Payoff * Prob(Opponent doesn't block)
       out_evs[br_hand_idx] = payoff_for_br_player * non_blocking_opp_prob;

       if (config_.debug_log && br_hand_idx == 0) { // Log first hand only
           std::cout << "[DEBUG BR Terminal Calc] HandIdx=" << br_hand_idx
                     << ", Payoff=" << payoff_for_br_player
                     << ", OppSum=" << oppo_total_reach_sum
                     << ", Blocker1Sum=" << oppo_card_reach_sum[br_hand.Card1Int()]
                     << ", Blocker2Sum=" << oppo_card_reach_sum[br_hand.Card2Int()]
                     << ", OppEquivIdx=" << (opp_equiv_idx.has_value() ? std::to_string(opp_equiv_idx.value()) : "N/A")
                     << ", NonBlockProb=" << non_blocking_opp_prob
                     << ", EV=" << out_evs[br_hand_idx] << std::endl;
       }
   }
   // ---------------------------------------
}


// Takes output vector by reference, marked const
// Takes output vector by reference, marked const
// Takes output vector by reference, marked const
void BestResponse::HandleShowdownNode(
    const std::shared_ptr<nodes::ShowdownNode>& node, // Use const&
    size_t best_response_player_index,
    const std::vector<std::vector<double>>& reach_probs,
    uint64_t current_board_mask,
    const std::vector<std::vector<core::PrivateCards>>& player_ranges, // Pass needed data
    ranges::RiverRangeManager& rrm,                                   // Pass needed data
    std::vector<double>& out_evs) const { // Output parameter

    if (!node) { throw std::logic_error("HandleShowdownNode called with null node ptr."); }

   // Ensure output vector is correct size
   size_t num_hands_br = player_hand_counts_[best_response_player_index];
   if (out_evs.size() != num_hands_br) out_evs.resize(num_hands_br);
   std::fill(out_evs.begin(), out_evs.end(), 0.0); // Initialize to zero

   // *** Board Validation Check (Keep this) ***
   int pop_count = 0;
   #if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
        pop_count = std::popcount(current_board_mask);
   #elif defined(__GNUC__) || defined(__clang__)
        pop_count = __builtin_popcountll(current_board_mask);
   #else
        uint64_t count_mask = current_board_mask;
        while(count_mask > 0) { count_mask &= (count_mask - 1); pop_count++; }
   #endif

   if (config_.debug_log) {
       std::cout << "[DEBUG BR] HandleShowdownNode received board mask: 0x"
                 << std::hex << current_board_mask << std::dec << std::endl;
       std::cout << "[DEBUG BR] Calculated popcount: " << pop_count << std::endl;
   }

   if (pop_count != 5) {
       std::cerr << "[WARNING BR] HandleShowdownNode called with invalid board mask (Cards: "
                 << pop_count << "). Returning 0 EV for all hands." << std::endl;
       return;
   }
   // *** END CHECK ***


   size_t opponent_player_index = 1 - best_response_player_index;
   size_t num_hands_opp = player_hand_counts_[opponent_player_index];

   if (reach_probs.size() != 2 || reach_probs[0].size() != player_hand_counts_[0] || reach_probs[1].size() != player_hand_counts_[1]) {
        throw std::logic_error("Reach probability vector size mismatch in HandleShowdownNode.");
   }

   // Log opponent reach probability sum
   double opp_reach_sum_check = std::accumulate(reach_probs[opponent_player_index].begin(),
                                                reach_probs[opponent_player_index].end(), 0.0);
   if (config_.debug_log) {
       std::cout << "[DEBUG BR Showdown] Received Opponent Reach Sum: "
                 << std::fixed << std::setprecision(8) << opp_reach_sum_check << std::endl;
       // Add check if sum is not close to 1.0
       if (std::abs(opp_reach_sum_check - 1.0) > 1e-6 && opp_reach_sum_check > 1e-12) {
            std::cerr << "[WARNING BR Showdown] Opponent reach sum is not 1.0!" << std::endl;
       }
   }


   const auto& br_player_initial_range = player_ranges[best_response_player_index];
   const auto& opp_player_initial_range = player_ranges[opponent_player_index];

   const auto& br_river_combos = rrm.GetRiverCombos(best_response_player_index, br_player_initial_range, current_board_mask);
   const auto& opp_river_combos = rrm.GetRiverCombos(opponent_player_index, opp_player_initial_range, current_board_mask);

   // *** ADDED: Log combo vector sizes ***
   if (config_.debug_log) {
       std::cout << "[DEBUG BR Showdown] BR Combo Size: " << br_river_combos.size()
                 << ", Opp Combo Size: " << opp_river_combos.size() << std::endl;
   }
   // ************************************


   const auto& payoffs_br_wins = node->GetPayoffs(best_response_player_index == 0 ? core::ComparisonResult::kPlayer1Wins : core::ComparisonResult::kPlayer2Wins);
   const auto& payoffs_opp_wins = node->GetPayoffs(opponent_player_index == 0 ? core::ComparisonResult::kPlayer1Wins : core::ComparisonResult::kPlayer2Wins);
   const auto& payoffs_tie = node->GetPayoffs(core::ComparisonResult::kTie);
   if (payoffs_br_wins.size() != 2 || payoffs_opp_wins.size() != 2 || payoffs_tie.size() != 2) {
       throw std::logic_error("ShowdownNode payoff vector size not 2.");
   }
   double payoff_on_win = payoffs_br_wins[best_response_player_index];
   double payoff_on_loss = payoffs_opp_wins[best_response_player_index];
   double payoff_on_tie = payoffs_tie[best_response_player_index];

   if (config_.debug_log) {
       std::cout << "[DEBUG BR Showdown Payoffs] P" << best_response_player_index
                 << ": Win=" << payoff_on_win << ", Loss=" << payoff_on_loss << ", Tie=" << payoff_on_tie << std::endl;
   }

   // --- Sweep-line algorithm for EV calculation ---
   double current_win_prob_sum = 0.0;
   std::vector<double> current_card_win_sum(core::kNumCardsInDeck, 0.0);
   size_t opp_idx_win = 0;

   for (size_t br_idx = 0; br_idx < br_river_combos.size(); ++br_idx) {
       const auto& br_combo = br_river_combos[br_idx];
       size_t br_hand_idx = br_combo.original_range_index;
       if (br_hand_idx >= num_hands_br) continue;
       int br_rank = br_combo.rank;
       if (br_rank == eval::Dic5Compairer::kInvalidRank || br_rank == -1) {
            out_evs[br_hand_idx] = 0.0; continue;
       }

       // *** ADDED: Log entry into win sweep for hand 0 ***
       if (config_.debug_log && br_hand_idx == 0) {
            std::cout << "[DEBUG BR Win Sweep] Processing HandIdx=" << br_hand_idx << ", Rank=" << br_rank << std::endl;
       }
       // *************************************************

       while (opp_idx_win < opp_river_combos.size() && br_rank < opp_river_combos[opp_idx_win].rank) {
           const auto& opp_combo = opp_river_combos[opp_idx_win];
           if (opp_combo.original_range_index < reach_probs[opponent_player_index].size()) {
               double opp_prob = reach_probs[opponent_player_index][opp_combo.original_range_index];
               current_win_prob_sum += opp_prob;
               current_card_win_sum[opp_combo.private_cards.Card1Int()] += opp_prob;
               current_card_win_sum[opp_combo.private_cards.Card2Int()] += opp_prob;
               // *** ADDED: Log accumulation in win sweep for hand 0 ***
               if (config_.debug_log && br_hand_idx == 0) {
                    std::cout << "[DEBUG BR Win Sweep Accum] OppIdx=" << opp_idx_win << ", OppRank=" << opp_combo.rank
                              << ", OppProb=" << opp_prob << ", NewWinSum=" << current_win_prob_sum << std::endl;
               }
               // ******************************************************
           }
           opp_idx_win++;
       }

       double win_reach_blockers = current_win_prob_sum
                                 - current_card_win_sum[br_combo.private_cards.Card1Int()]
                                 - current_card_win_sum[br_combo.private_cards.Card2Int()];

       out_evs[br_hand_idx] = std::max(0.0, win_reach_blockers) * payoff_on_win;

       if (config_.debug_log && br_hand_idx == 0) {
            std::cout << "[DEBUG BR Showdown Win Calc] HandIdx=" << br_hand_idx << ", Rank=" << br_rank
                      << ", WinSum=" << current_win_prob_sum
                      << ", Blocker1=" << current_card_win_sum[br_combo.private_cards.Card1Int()]
                      << ", Blocker2=" << current_card_win_sum[br_combo.private_cards.Card2Int()]
                      << ", WinReach=" << std::max(0.0, win_reach_blockers)
                      << ", EV(Win)=" << out_evs[br_hand_idx] << std::endl;
       }
   }

   // Calculate loss payoff (sweep from the other end)
   double current_loss_prob_sum = 0.0;
   std::vector<double> current_card_loss_sum(core::kNumCardsInDeck, 0.0);
   size_t opp_idx_loss = opp_river_combos.size();

   for (int br_idx = static_cast<int>(br_river_combos.size()) - 1; br_idx >= 0; --br_idx) {
       const auto& br_combo = br_river_combos[br_idx];
       size_t br_hand_idx = br_combo.original_range_index;
       if (br_hand_idx >= num_hands_br) continue;
       int br_rank = br_combo.rank;
        if (br_rank == eval::Dic5Compairer::kInvalidRank || br_rank == -1) continue;

       // *** ADDED: Log entry into loss sweep for hand 0 ***
       if (config_.debug_log && br_hand_idx == 0) {
            std::cout << "[DEBUG BR Loss Sweep] Processing HandIdx=" << br_hand_idx << ", Rank=" << br_rank << std::endl;
       }
       // **************************************************

       while (opp_idx_loss > 0 && br_rank > opp_river_combos[opp_idx_loss - 1].rank) {
           opp_idx_loss--;
           const auto& opp_combo = opp_river_combos[opp_idx_loss];
            if (opp_combo.original_range_index < reach_probs[opponent_player_index].size()) {
               double opp_prob = reach_probs[opponent_player_index][opp_combo.original_range_index];
               current_loss_prob_sum += opp_prob;
               current_card_loss_sum[opp_combo.private_cards.Card1Int()] += opp_prob;
               current_card_loss_sum[opp_combo.private_cards.Card2Int()] += opp_prob;
               // *** ADDED: Log accumulation in loss sweep for hand 0 ***
               if (config_.debug_log && br_hand_idx == 0) {
                    std::cout << "[DEBUG BR Loss Sweep Accum] OppIdx=" << opp_idx_loss << ", OppRank=" << opp_combo.rank
                              << ", OppProb=" << opp_prob << ", NewLossSum=" << current_loss_prob_sum << std::endl;
               }
               // *******************************************************
           }
       }

       double loss_reach_blockers = current_loss_prob_sum
                                  - current_card_loss_sum[br_combo.private_cards.Card1Int()]
                                  - current_card_loss_sum[br_combo.private_cards.Card2Int()];

       double loss_ev_component = std::max(0.0, loss_reach_blockers) * payoff_on_loss;
       out_evs[br_hand_idx] += loss_ev_component;

       if (config_.debug_log && br_hand_idx == 0) {
            std::cout << "[DEBUG BR Showdown Loss Calc] HandIdx=" << br_hand_idx << ", Rank=" << br_rank
                      << ", LossSum=" << current_loss_prob_sum
                      << ", Blocker1=" << current_card_loss_sum[br_combo.private_cards.Card1Int()]
                      << ", Blocker2=" << current_card_loss_sum[br_combo.private_cards.Card2Int()]
                      << ", LossReach=" << std::max(0.0, loss_reach_blockers)
                      << ", EV(Loss)=" << loss_ev_component
                      << ", EV(AfterLoss)=" << out_evs[br_hand_idx] << std::endl; // Log EV after adding loss
       }


       // --- Simple Tie Calculation (O(N*M)) ---
       double current_tie_prob_sum = 0.0;
       for(const auto& opp_combo : opp_river_combos) {
            if (opp_combo.rank == br_rank) {
                 if (opp_combo.original_range_index < reach_probs[opponent_player_index].size()) {
                   if (!core::Card::DoBoardsOverlap(br_combo.private_cards.GetBoardMask(), opp_combo.private_cards.GetBoardMask())) {
                        double opp_prob = reach_probs[opponent_player_index][opp_combo.original_range_index];
                        current_tie_prob_sum += opp_prob;
                   }
                 }
            }
       }
       double tie_reach_blockers = current_tie_prob_sum; // Simplified blocker adjustment for ties
       double tie_ev_component = std::max(0.0, tie_reach_blockers) * payoff_on_tie;
       out_evs[br_hand_idx] += tie_ev_component;

       if (config_.debug_log && br_hand_idx == 0) {
            std::cout << "[DEBUG BR Showdown Tie Calc] HandIdx=" << br_hand_idx << ", Rank=" << br_rank
                      << ", TieSum=" << current_tie_prob_sum
                      << ", TieReach=" << std::max(0.0, tie_reach_blockers)
                      << ", EV(Tie)=" << tie_ev_component
                      << ", EV(Final)=" << out_evs[br_hand_idx] << std::endl;
       }
       // ---------------------------------
   }

   // Log final EVs
   if (config_.debug_log) {
       std::cout << "[DEBUG BR Showdown EVs] Player " << best_response_player_index
                 << " Board: 0x" << std::hex << current_board_mask << std::dec
                 << " EVs: {";
       bool first = true;
       for(size_t h = 0; h < out_evs.size(); ++h) {
           if (!first) std::cout << ", ";
           std::cout << std::fixed << std::setprecision(4) << out_evs[h];
           first = false;
       }
       std::cout << "}" << std::endl << std::flush;
   }

   // No return needed (fills out_evs)
}

// --- Other Private Helpers ---

// Marked const, pass deck by const&
int BestResponse::CalculatePossibleDeals(uint64_t current_board_mask, const core::Deck& deck) const {
     int board_card_count = 0;
     #if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L
         board_card_count = std::popcount(current_board_mask);
     #elif defined(__GNUC__) || defined(__clang__)
         board_card_count = __builtin_popcountll(current_board_mask);
     #else
         uint64_t count_mask = current_board_mask;
         while(count_mask > 0) { count_mask &= (count_mask - 1); board_card_count++; }
     #endif
     // Assumes 2 players (4 private cards removed)
     int remaining_cards = deck.GetCards().size() - board_card_count - 4;
     return std::max(0, remaining_cards);
}

// Marked const
size_t BestResponse::GetNextDealAbstractionIndex(size_t /* current_deal_index */, int /* card_index */) const {
    // Placeholder - needs real implementation based on how card indices map to abstract deals
    return 0; // Assuming perfect recall for now
}

void BestResponse::InitializeIsomorphism() {
    // Placeholder - needs real implementation
    // This should populate the suit_iso_offset_ member based on initial board state if applicable
    // Example: Calculate color hash based on initial board if any, then find symmetries.
     for (auto& offsets : suit_iso_offset_) {
         offsets.fill(0); // Default to 0 (no offset/canonical)
     }
     if (config_.debug_log) {
        std::cout << "[DEBUG BR] Suit isomorphism calculation not implemented in InitializeIsomorphism." << std::endl;
     }
}


} // namespace solver
} // namespace poker_solver
