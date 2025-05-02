#include "solver/best_response.h"
#include "nodes/ActionNode.h"
#include "nodes/ChanceNode.h"
#include "nodes/ShowdownNode.h"
#include "nodes/TerminalNode.h"
#include "Card.h"
#include "trainable/Trainable.h" // Need definition for GetAverageStrategy
#include "compairer/Dic5Compairer.h"    // For kInvalidRank
#include "tools/utils.h"

#include <vector>
#include <numeric>   // For std::accumulate
#include <cmath>     // For std::abs, std::max
#include <limits>    // For std::numeric_limits
#include <stdexcept> // For exceptions
#include <sstream>   // For error messages
#include <iostream>  // For debug logging
#include <iomanip>   // For debug logging formatting
#include <utility>   // For std::move
#include <bit>       // For std::popcount (optional)
// #include <omp.h>     // For OpenMP pragmas - COMMENTED OUT

// Use aliases
namespace core = poker_solver::core;
namespace nodes = poker_solver::nodes;
namespace ranges = poker_solver::ranges;
namespace utils = poker_solver::utils;
// Use eval namespace specifically for kInvalidRank constant
namespace eval = poker_solver::eval;


namespace poker_solver {
namespace solver {

// --- Constructor ---

BestResponse::BestResponse(
    const std::vector<std::vector<core::PrivateCards>>& player_ranges,
    const ranges::PrivateCardsManager& pcm,
    ranges::RiverRangeManager& rrm,
    const core::Deck& deck,
    const Config& config)
    : player_ranges_(player_ranges),
      pcm_(pcm),
      rrm_(rrm),
      deck_(deck),
      config_(config) {

    if (player_ranges.size() != 2) {
        throw std::invalid_argument("BestResponse currently only supports 2 players.");
    }
    player_hand_counts_.push_back(player_ranges[0].size());
    player_hand_counts_.push_back(player_ranges[1].size());

    // Initialize isomorphism table if needed
    if (config_.use_suit_isomorphism) {
        InitializeIsomorphism();
    } else {
        // Set all offsets to 0 if isomorphism is disabled
        for (size_t i = 0; i < kMaxDealAbstractionIndex; ++i) {
            for (int j = 0; j < core::kNumSuits; ++j) {
                suit_iso_offset_[i][j] = 0;
            }
        }
    }

    // Set number of threads for OpenMP - COMMENTED OUT
    // if (config_.num_threads > 0) {
    //     omp_set_num_threads(config_.num_threads);
    // } else {
    //     omp_set_num_threads(1); // Default to 1 thread if invalid config
    // }
}

// --- Public Methods ---

double BestResponse::CalculateExploitability(
    std::shared_ptr<core::GameTreeNode> root_node,
    uint64_t initial_board_mask,
    double initial_pot) const {

    if (!root_node) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (initial_pot <= 0) {
         std::cerr << "[WARNING] Initial pot is non-positive (" << initial_pot
                   << ") in CalculateExploitability. Result might be misleading." << std::endl;
    }

    double total_ev_sum = 0.0;

    // Calculate best response EV for Player 0
    double ev_p0 = CalculateBestResponseEv(root_node, 0, initial_board_mask);
    if (config_.debug_log) {
        std::cout << "[DEBUG BR] Best Response EV for Player 0: " << ev_p0 << std::endl;
    }
    // Check for NaN before adding
    if (!std::isnan(ev_p0)) {
        total_ev_sum += ev_p0;
    } else {
         std::cerr << "[WARNING BR] NaN EV detected for Player 0." << std::endl;
    }


    // Calculate best response EV for Player 1
    double ev_p1 = CalculateBestResponseEv(root_node, 1, initial_board_mask);
     if (config_.debug_log) {
        std::cout << "[DEBUG BR] Best Response EV for Player 1: " << ev_p1 << std::endl;
    }
    // Check for NaN before adding
     if (!std::isnan(ev_p1)) {
        total_ev_sum += ev_p1;
    } else {
         std::cerr << "[WARNING BR] NaN EV detected for Player 1." << std::endl;
    }


    // Exploitability is the average value gained by the best responder.
    double exploitability = total_ev_sum / 2.0;

     if (config_.debug_log) {
        std::cout << "[DEBUG BR] Total Exploitability Value: " << exploitability << std::endl;
    }

    return exploitability;
}


double BestResponse::CalculateBestResponseEv(
    std::shared_ptr<core::GameTreeNode> root_node,
    size_t best_response_player_index,
    uint64_t initial_board_mask) const {

    if (!root_node) return 0.0;
    if (best_response_player_index >= 2) {
        throw std::out_of_range("Invalid best_response_player_index.");
    }

    // Get initial reach probabilities considering board and opponent ranges
    std::vector<std::vector<double>> initial_reach_probs = GetInitialReachProbs(initial_board_mask);

    // Calculate node values recursively starting from the root
    std::vector<double> node_values = CalculateNodeValue(
        root_node,
        best_response_player_index,
        initial_reach_probs,
        initial_board_mask,
        0 // Initial deal abstraction index
    );

    // The total EV is the sum of node values weighted by the initial *absolute*
    // probability of each hand (considering blockers).
    const auto& final_reach_probs = pcm_.GetInitialReachProbs(best_response_player_index);

    if (node_values.size() != final_reach_probs.size()) {
         std::cerr << "[ERROR BR] Node value vector size (" << node_values.size()
                   << ") does not match initial reach prob size (" << final_reach_probs.size()
                   << ") for player " << best_response_player_index << std::endl;
         return std::numeric_limits<double>::quiet_NaN(); // Indicate error
    }

    double total_ev = 0.0;
    for (size_t i = 0; i < node_values.size(); ++i) {
        // Check for NaN in node_values or final_reach_probs before multiplying
        if (!std::isnan(node_values[i]) && !std::isnan(final_reach_probs[i])) {
            total_ev += node_values[i] * final_reach_probs[i];
        } else {
            // Log or handle NaN case appropriately
             if (config_.debug_log) {
                 std::cerr << "[WARNING BR] NaN detected during EV calculation. Index: " << i
                           << ", NodeValue: " << node_values[i]
                           << ", ReachProb: " << final_reach_probs[i] << std::endl;
             }
        }
    }

    return total_ev;
}


// --- Recursive Calculation Helpers ---

std::vector<double> BestResponse::CalculateNodeValue(
    std::shared_ptr<core::GameTreeNode> node,
    size_t best_response_player_index,
    const std::vector<std::vector<double>>& reach_probs,
    uint64_t current_board_mask,
    size_t deal_abstraction_index) const {

    if (!node) {
        std::cerr << "[ERROR BR] CalculateNodeValue called with null node!" << std::endl;
        throw std::logic_error("Encountered null node during BestResponse traversal.");
    }

    // --- Add Debug ---
    // std::cout << "[DEBUG BR Enter] Node Type: " << static_cast<int>(node->GetNodeType())
    //           << ", BR Player: " << best_response_player_index
    //           << ", Node Ptr: " << node.get() << std::endl;
    // --- End Debug ---

    std::vector<double> result;
    try {
        switch (node->GetNodeType()) {
            case core::GameTreeNodeType::kAction:
                result = HandleActionNode(
                    std::dynamic_pointer_cast<nodes::ActionNode>(node),
                    best_response_player_index, reach_probs, current_board_mask,
                    deal_abstraction_index);
                break;
            case core::GameTreeNodeType::kChance:
                result = HandleChanceNode(
                    std::dynamic_pointer_cast<nodes::ChanceNode>(node),
                    best_response_player_index, reach_probs, current_board_mask,
                    deal_abstraction_index);
                break;
            case core::GameTreeNodeType::kTerminal:
                result = HandleTerminalNode(
                    std::dynamic_pointer_cast<nodes::TerminalNode>(node),
                    best_response_player_index, reach_probs, current_board_mask);
                break;
            case core::GameTreeNodeType::kShowdown:
                result = HandleShowdownNode(
                    std::dynamic_pointer_cast<nodes::ShowdownNode>(node),
                    best_response_player_index, reach_probs, current_board_mask);
                break;
            default:
                 throw std::logic_error("Unknown node type encountered in CalculateNodeValue.");
        }
    } catch (const std::exception& e) {
         std::cerr << "[ERROR BR] Exception during CalculateNodeValue for node type "
                   << static_cast<int>(node->GetNodeType()) << ": " << e.what() << std::endl;
         // Re-throw or return default error value? Re-throwing is often better for debugging.
         throw;
    }

    // --- Add Debug ---
    // std::cout << "[DEBUG BR Exit] Node Type: " << static_cast<int>(node->GetNodeType())
    //           << ", Node Ptr: " << node.get() << std::endl;
    // --- End Debug ---
    return result;
}

std::vector<double> BestResponse::HandleActionNode(
    std::shared_ptr<nodes::ActionNode> node,
    size_t best_response_player_index,
    const std::vector<std::vector<double>>& reach_probs,
    uint64_t current_board_mask,
    size_t deal_abstraction_index) const {

    // --- Add Debug ---
    // std::cout << "[DEBUG BR Enter] HandleActionNode Ptr: " << node.get()
    //           << ", Acting Player: " << node->GetPlayerIndex()
    //           << ", BR Player: " << best_response_player_index << std::endl;
    // --- End Debug ---

    if (!node) { throw std::logic_error("HandleActionNode called with null node ptr."); }

    size_t acting_player = node->GetPlayerIndex();
    size_t opponent_player = 1 - acting_player;
    size_t num_hands_acting = player_hand_counts_[acting_player];
    size_t num_hands_br = player_hand_counts_[best_response_player_index]; // Size for the return vector
    size_t num_actions = node->GetActions().size();

    if (num_actions == 0) { return std::vector<double>(num_hands_br, 0.0); }

    std::vector<std::vector<double>> child_node_values(num_actions);
    for (size_t a = 0; a < num_actions; ++a) {
        std::shared_ptr<core::GameTreeNode> child_node = node->GetChildren()[a];
        if (!child_node) {
             throw std::logic_error("ActionNode has null child pointer.");
        }
        std::vector<std::vector<double>> next_reach_probs = reach_probs; // Copy current probs

        if (acting_player != best_response_player_index) {
            auto trainable = node->GetTrainableIfExists(deal_abstraction_index);
            std::vector<float> fixed_strategy;
            if (trainable) {
                fixed_strategy = trainable->GetAverageStrategy(); // Use average strategy
            } else {
                 fixed_strategy.assign(num_actions * num_hands_acting,
                                       (num_actions > 0) ? 1.0f / static_cast<float>(num_actions) : 0.0f);
                 // std::cerr << "[WARNING BR] Trainable not found for node, assuming uniform strategy." << std::endl;
            }
            if (fixed_strategy.size() != num_actions * num_hands_acting) { throw std::logic_error("Strategy size mismatch in HandleActionNode."); }

            next_reach_probs[acting_player].assign(num_hands_acting, 0.0); // Reset next probs
            double next_reach_sum = 0.0; // Sum of reach after applying strategy
            for (size_t h = 0; h < num_hands_acting; ++h) {
                size_t strat_idx = a * num_hands_acting + h;
                if (strat_idx < fixed_strategy.size() && h < reach_probs[acting_player].size()) { // Bounds check
                    next_reach_probs[acting_player][h] = reach_probs[acting_player][h] * static_cast<double>(fixed_strategy[strat_idx]);
                    next_reach_sum += next_reach_probs[acting_player][h];
                }
            }
            // Normalize next reach probabilities if the sum is valid
            if (next_reach_sum > 1e-12) {
                for(double& p : next_reach_probs[acting_player]) {
                    p /= next_reach_sum;
                }
            } else {
                 // If this action has zero probability for all hands, set all to zero
                 next_reach_probs[acting_player].assign(num_hands_acting, 0.0);
            }
        }
        // Opponent's reach probs remain unchanged relative to each other for this step

        // --- Add Debug ---
        // std::cout << "[DEBUG BR Action] Recursing on child " << a << " (" << child_node.get() << ")" << std::endl;
        // --- End Debug ---
        child_node_values[a] = CalculateNodeValue( child_node, best_response_player_index, next_reach_probs, current_board_mask, deal_abstraction_index);
        // --- Add Debug ---
        // std::cout << "[DEBUG BR Action] Returned from child " << a << std::endl;
        // --- End Debug ---
    }


    std::vector<double> current_node_value(num_hands_br, 0.0);

    if (acting_player == best_response_player_index) {
        // Best responder chooses the action maximizing their EV for each hand
        for (size_t h = 0; h < num_hands_br; ++h) {
            double max_ev = -std::numeric_limits<double>::infinity();
            for (size_t a = 0; a < num_actions; ++a) {
                 if (h < child_node_values[a].size()) { // Bounds check
                    max_ev = std::max(max_ev, child_node_values[a][h]);
                 }
            }
            current_node_value[h] = (max_ev == -std::numeric_limits<double>::infinity()) ? 0.0 : max_ev; // Default to 0 if no actions possible?
        }
    } else {
        // Opponent acts according to their fixed strategy.
        // The EV for the BR player is the weighted average over opponent's actions.
        // The weighting was already applied when calculating next_reach_probs for the recursive call.
        // So, we just sum the results returned from the recursive calls.
        for (size_t a = 0; a < num_actions; ++a) {
             for (size_t h = 0; h < current_node_value.size(); ++h) {
                  if (h < child_node_values[a].size()) { // Bounds check
                    current_node_value[h] += child_node_values[a][h];
                  }
             }
        }
    }

    return current_node_value;
}


// --- Refined HandleChanceNode ---
std::vector<double> BestResponse::HandleChanceNode(
    std::shared_ptr<nodes::ChanceNode> node,
    size_t best_response_player_index,
    const std::vector<std::vector<double>>& reach_probs,
    uint64_t current_board_mask, // Board *before* this chance event
    size_t deal_abstraction_index) const {

   if (!node) { throw std::logic_error("HandleChanceNode called with null node ptr."); }
   std::shared_ptr<core::GameTreeNode> child_node = node->GetChild();
   if (!child_node) throw std::logic_error("ChanceNode has no child during BestResponse traversal.");

   size_t num_hands_p0 = player_hand_counts_[0];
   size_t num_hands_p1 = player_hand_counts_[1];
   std::vector<double> total_child_values(player_hand_counts_[best_response_player_index], 0.0);
   double total_deal_weight_sum = 0.0;

   const auto& deck_cards = deck_.GetCards();
   int num_deck_cards = deck_cards.size();

   int cards_to_deal = 0;
   core::GameRound round_before_deal = node->GetRound();

   if (round_before_deal == core::GameRound::kPreflop) {
       cards_to_deal = 3;
       throw std::logic_error("BestResponse ChanceNode logic does not support dealing Flop (3 cards).");
   } else if (round_before_deal == core::GameRound::kFlop || round_before_deal == core::GameRound::kTurn) {
       cards_to_deal = 1;
   } else if (round_before_deal == core::GameRound::kRiver) {
       // Pass current board mask unchanged to child
       return CalculateNodeValue(child_node, best_response_player_index, reach_probs,
                                 current_board_mask, deal_abstraction_index);
   } else {
       throw std::logic_error("Invalid GameRound encountered in HandleChanceNode.");
   }

   // --- Logic for dealing 1 card (Turn or River) ---
   int possible_deals = CalculatePossibleDeals(current_board_mask);
   if (possible_deals <= 0) {
       std::cerr << "[WARNING BR] No possible deals found in HandleChanceNode. Board: 0x"
                 << std::hex << current_board_mask << std::dec << std::endl;
       return total_child_values;
   }
   double deal_probability = 1.0 / static_cast<double>(possible_deals);


   // Iterate through each potential card from the deck
   for (int i = 0; i < num_deck_cards; ++i) {
       const core::Card& next_card = deck_cards[i];
       if (next_card.IsEmpty()) continue;
       int card_int = next_card.card_int().value();
       uint64_t card_mask = core::Card::CardIntToUint64(card_int);
       if (core::Card::DoBoardsOverlap(card_mask, current_board_mask)) continue;

       int iso_offset = config_.use_suit_isomorphism ? suit_iso_offset_[deal_abstraction_index][card_int % core::kNumSuits] : 0;
       if (iso_offset < 0) continue;

       std::vector<std::vector<double>> next_reach_probs(2);
       next_reach_probs[0].resize(num_hands_p0);
       next_reach_probs[1].resize(num_hands_p1);
       double p0_reach_sum = 0.0; double p1_reach_sum = 0.0;
       bool p0_possible = false; bool p1_possible = false;

       for (size_t h = 0; h < num_hands_p0; ++h) {
           if (core::Card::DoBoardsOverlap(player_ranges_[0][h].GetBoardMask(), card_mask)) next_reach_probs[0][h] = 0.0;
           else { next_reach_probs[0][h] = reach_probs[0][h]; if (next_reach_probs[0][h] > 1e-12) p0_possible = true; p0_reach_sum += next_reach_probs[0][h]; }
       }
       for (size_t h = 0; h < num_hands_p1; ++h) {
            if (core::Card::DoBoardsOverlap(player_ranges_[1][h].GetBoardMask(), card_mask)) next_reach_probs[1][h] = 0.0;
            else { next_reach_probs[1][h] = reach_probs[1][h]; if (next_reach_probs[1][h] > 1e-12) p1_possible = true; p1_reach_sum += next_reach_probs[1][h]; }
       }
       if (!p0_possible || !p1_possible) continue;

       if (p0_reach_sum > 1e-12) { for(double& p : next_reach_probs[0]) p /= p0_reach_sum; }
       else { next_reach_probs[0].assign(num_hands_p0, 0.0); }
       if (p1_reach_sum > 1e-12) { for(double& p : next_reach_probs[1]) p /= p1_reach_sum; }
       else { next_reach_probs[1].assign(num_hands_p1, 0.0); }

       // *** FIX: Calculate the board mask for the *next* state ***
       uint64_t next_board_mask = current_board_mask | card_mask; // Combine current board with new card
       size_t next_deal_abs_index = GetNextDealAbstractionIndex(deal_abstraction_index, i);

       // *** FIX: Pass the next_board_mask to the recursive call ***
       std::vector<double> child_values = CalculateNodeValue(
           child_node, best_response_player_index, next_reach_probs,
           next_board_mask, next_deal_abs_index); // Pass updated board mask

       if (iso_offset > 0 && config_.use_suit_isomorphism) { /* TODO: Isomorphism */ }

       for (size_t h = 0; h < total_child_values.size(); ++h) {
           if (h < child_values.size()) {
               total_child_values[h] += child_values[h] * deal_probability;
           }
       }
       total_deal_weight_sum += deal_probability;
   }

   // Normalize final values if needed
   if (std::abs(total_deal_weight_sum - 1.0) > 1e-6 && total_deal_weight_sum > 1e-12) {
       if (config_.debug_log) { /* Log warning */ }
       for (double& val : total_child_values) { val /= total_deal_weight_sum; }
   } else if (total_deal_weight_sum <= 1e-12 && !total_child_values.empty()) {
        std::fill(total_child_values.begin(), total_child_values.end(), 0.0);
   }

   return total_child_values;
}


std::vector<double> BestResponse::HandleTerminalNode(
     std::shared_ptr<nodes::TerminalNode> node,
     size_t best_response_player_index,
     const std::vector<std::vector<double>>& reach_probs,
     uint64_t current_board_mask) const {
    // --- Add Debug ---
    // std::cout << "[DEBUG BR Enter] HandleTerminalNode Ptr: " << node.get() << std::endl;
    // --- End Debug ---
    if (!node) { throw std::logic_error("HandleTerminalNode called with null node ptr."); }
    size_t num_hands = player_hand_counts_[best_response_player_index];
    std::vector<double> payoffs(num_hands, 0.0);
    const std::vector<double>& node_payoffs = node->GetPayoffs();
    if (node_payoffs.size() != 2) throw std::logic_error("TerminalNode payoff vector size not 2.");
    double payoff_for_br_player = node_payoffs[best_response_player_index];
    for (size_t h = 0; h < num_hands; ++h) payoffs[h] = payoff_for_br_player;
    return payoffs;
}


std::vector<double> BestResponse::HandleShowdownNode(
     std::shared_ptr<nodes::ShowdownNode> node,
     size_t best_response_player_index,
     const std::vector<std::vector<double>>& reach_probs,
     uint64_t current_board_mask) const {
    // --- Add Debug ---
    // std::cout << "[DEBUG BR Enter] HandleShowdownNode Ptr: " << node.get() << std::endl;
    // --- End Debug ---
     if (!node) { throw std::logic_error("HandleShowdownNode called with null node ptr."); }

    // *** ADD CHECK: Ensure board mask represents 5 cards ***
    int pop_count = 0;
    #if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L // Check C++20 feature
        pop_count = std::popcount(current_board_mask);
    #elif defined(__GNUC__) || defined(__clang__)
        pop_count = __builtin_popcountll(current_board_mask);
    #else
        uint64_t count_mask = current_board_mask;
        while(count_mask > 0) { count_mask &= (count_mask - 1); pop_count++; }
    #endif

    size_t num_hands_br = player_hand_counts_[best_response_player_index];
    if (pop_count != 5) {
        std::cerr << "[WARNING BR] HandleShowdownNode called with invalid board mask (Cards: "
                  << pop_count << "). Returning 0 EV for all hands." << std::endl;
        return std::vector<double>(num_hands_br, 0.0); // Return default value
    }
    // *** END CHECK ***


    size_t opponent_player_index = 1 - best_response_player_index;
    // size_t num_hands_br = player_hand_counts_[best_response_player_index]; // Defined above
    size_t num_hands_opp = player_hand_counts_[opponent_player_index];
    std::vector<double> expected_payoffs(num_hands_br, 0.0);

    // Check if reach probabilities are valid sizes
    if (reach_probs.size() != 2 ||
        reach_probs[best_response_player_index].size() != num_hands_br ||
        reach_probs[opponent_player_index].size() != num_hands_opp) {
         throw std::logic_error("Reach probability vector size mismatch in HandleShowdownNode.");
    }

    const auto& br_player_initial_range = player_ranges_[best_response_player_index];
    const auto& opp_player_initial_range = player_ranges_[opponent_player_index];

    // GetRiverCombos should now receive a valid 5-card mask
    const auto& br_river_combos = rrm_.GetRiverCombos(best_response_player_index, br_player_initial_range, current_board_mask);
    const auto& opp_river_combos = rrm_.GetRiverCombos(opponent_player_index, opp_player_initial_range, current_board_mask);

    const auto& payoffs_br_wins = node->GetPayoffs(best_response_player_index == 0 ? core::ComparisonResult::kPlayer1Wins : core::ComparisonResult::kPlayer2Wins);
    const auto& payoffs_opp_wins = node->GetPayoffs(opponent_player_index == 0 ? core::ComparisonResult::kPlayer1Wins : core::ComparisonResult::kPlayer2Wins);
    const auto& payoffs_tie = node->GetPayoffs(core::ComparisonResult::kTie);
    if (payoffs_br_wins.size() != 2 || payoffs_opp_wins.size() != 2 || payoffs_tie.size() != 2) {
        throw std::logic_error("ShowdownNode payoff vector size not 2.");
    }
    double payoff_on_win = payoffs_br_wins[best_response_player_index];
    double payoff_on_loss = payoffs_opp_wins[best_response_player_index];
    double payoff_on_tie = payoffs_tie[best_response_player_index];

    for (const auto& br_combo : br_river_combos) {
        size_t br_hand_idx = br_combo.original_range_index;
        if (br_hand_idx >= num_hands_br) continue;
        int br_rank = br_combo.rank;
        // Use fully qualified name for constant
        if (br_rank == eval::Dic5Compairer::kInvalidRank || br_rank == -1) {
             expected_payoffs[br_hand_idx] = 0.0; continue;
        }
        const auto& br_private_cards = br_combo.private_cards;
        double win_prob_final = 0, loss_prob_final = 0, tie_prob_final = 0;

        for(const auto& opp_combo : opp_river_combos) {
             size_t opp_hand_idx = opp_combo.original_range_index;
             if (opp_hand_idx >= reach_probs[opponent_player_index].size()) continue;
             if (core::Card::DoBoardsOverlap(br_private_cards.GetBoardMask(), opp_combo.private_cards.GetBoardMask())) continue;
             // Use fully qualified name for constant
             if (opp_combo.rank == eval::Dic5Compairer::kInvalidRank || opp_combo.rank == -1) continue;

             double opp_prob = reach_probs[opponent_player_index][opp_hand_idx];
             if (br_rank < opp_combo.rank) win_prob_final += opp_prob;
             else if (br_rank > opp_combo.rank) loss_prob_final += opp_prob;
             else tie_prob_final += opp_prob;
        }
        expected_payoffs[br_hand_idx] = (win_prob_final * payoff_on_win) + (loss_prob_final * payoff_on_loss) + (tie_prob_final * payoff_on_tie);
    }
    return expected_payoffs;
}

// --- Other Private Helpers ---
std::vector<std::vector<double>> BestResponse::GetInitialReachProbs(uint64_t initial_board_mask) const {
    std::vector<std::vector<double>> initial_probs(2);
    // Ensure PCM is valid before calling
    // Note: pcm_ is const&, GetInitialReachProbs should be const
    initial_probs[0] = pcm_.GetInitialReachProbs(0);
    initial_probs[1] = pcm_.GetInitialReachProbs(1);
    return initial_probs; // Return the calculated probabilities
}
int BestResponse::CalculatePossibleDeals(uint64_t current_board_mask) const {
     int board_card_count = 0;
     #if defined(__cpp_lib_bitops) && __cpp_lib_bitops >= 201907L // Check C++20 feature
         board_card_count = std::popcount(current_board_mask);
     #elif defined(__GNUC__) || defined(__clang__)
         board_card_count = __builtin_popcountll(current_board_mask);
     #else
         uint64_t count_mask = current_board_mask;
         while(count_mask > 0) { count_mask &= (count_mask - 1); board_card_count++; }
     #endif
     // Assumes 2 players (4 private cards removed)
     int remaining_cards = core::kNumCardsInDeck - board_card_count - 4;
     return std::max(0, remaining_cards); // Return 0 if negative
}
size_t BestResponse::GetNextDealAbstractionIndex(size_t current_deal_index, int card_index) const {
    return 0; // Perfect recall placeholder
}
void BestResponse::InitializeIsomorphism() {
     for (size_t i = 0; i < kMaxDealAbstractionIndex; ++i) {
         for (int j = 0; j < core::kNumSuits; ++j) {
             suit_iso_offset_[i][j] = 0;
         }
     }
     // std::cerr << "[WARNING BR] Suit isomorphism calculation not implemented in BestResponse::InitializeIsomorphism." << std::endl;
}


} // namespace solver
} // namespace poker_solver
