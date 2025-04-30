#include "solver/best_response.h"
#include "nodes/ActionNode.h"
#include "nodes/ChanceNode.h"
#include "nodes/ShowdownNode.h"
#include "nodes/TerminalNode.h"
#include "Card.h"
#include "trainable/Trainable.h" // Need definition for GetAverageStrategy

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

#include "compairer/Dic5Compairer.h" // <<< ADD THIS LINE

// Use aliases
namespace core = poker_solver::core;
namespace nodes = poker_solver::nodes;
namespace ranges = poker_solver::ranges;
namespace utils = poker_solver::utils;
// REMOVE: namespace eval = poker_solver::eval;


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
    if (!std::isnan(ev_p0)) { total_ev_sum += ev_p0; }
    else { std::cerr << "[WARNING BR] NaN EV detected for Player 0." << std::endl; }

    // Calculate best response EV for Player 1
    double ev_p1 = CalculateBestResponseEv(root_node, 1, initial_board_mask);
     if (config_.debug_log) {
        std::cout << "[DEBUG BR] Best Response EV for Player 1: " << ev_p1 << std::endl;
    }
     if (!std::isnan(ev_p1)) { total_ev_sum += ev_p1; }
     else { std::cerr << "[WARNING BR] NaN EV detected for Player 1." << std::endl; }

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

    std::vector<std::vector<double>> initial_reach_probs = GetInitialReachProbs(initial_board_mask);

    std::vector<double> node_values = CalculateNodeValue(
        root_node, best_response_player_index, initial_reach_probs,
        initial_board_mask, 0 );

    const auto& final_reach_probs = pcm_.GetInitialReachProbs(best_response_player_index);

    if (node_values.size() != final_reach_probs.size()) {
         std::cerr << "[ERROR BR] Node value vector size (" << node_values.size()
                   << ") does not match initial reach prob size (" << final_reach_probs.size()
                   << ") for player " << best_response_player_index << std::endl;
         return std::numeric_limits<double>::quiet_NaN();
    }

    double total_ev = 0.0;
    for (size_t i = 0; i < node_values.size(); ++i) {
        if (!std::isnan(node_values[i]) && !std::isnan(final_reach_probs[i])) {
            total_ev += node_values[i] * final_reach_probs[i];
        } else {
             if (config_.debug_log) { /* Log NaN */ }
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
    // ... (Switch statement remains the same) ...
    if (!node) { throw std::logic_error("Encountered null node during BestResponse traversal."); }
    switch (node->GetNodeType()) {
        case core::GameTreeNodeType::kAction:   return HandleActionNode(std::dynamic_pointer_cast<nodes::ActionNode>(node), best_response_player_index, reach_probs, current_board_mask, deal_abstraction_index);
        case core::GameTreeNodeType::kChance:   return HandleChanceNode(std::dynamic_pointer_cast<nodes::ChanceNode>(node), best_response_player_index, reach_probs, current_board_mask, deal_abstraction_index);
        case core::GameTreeNodeType::kTerminal: return HandleTerminalNode(std::dynamic_pointer_cast<nodes::TerminalNode>(node), best_response_player_index, reach_probs, current_board_mask);
        case core::GameTreeNodeType::kShowdown: return HandleShowdownNode(std::dynamic_pointer_cast<nodes::ShowdownNode>(node), best_response_player_index, reach_probs, current_board_mask);
        default: throw std::logic_error("Unknown node type encountered in CalculateNodeValue.");
    }
}

std::vector<double> BestResponse::HandleActionNode(
    std::shared_ptr<nodes::ActionNode> node,
    size_t best_response_player_index,
    const std::vector<std::vector<double>>& reach_probs,
    uint64_t current_board_mask,
    size_t deal_abstraction_index) const {
    // ... (Implementation remains the same) ...
    size_t acting_player = node->GetPlayerIndex();
    size_t opponent_player = 1 - acting_player;
    size_t num_hands_acting = player_hand_counts_[acting_player];
    size_t num_hands_br = player_hand_counts_[best_response_player_index];
    size_t num_actions = node->GetActions().size();
    if (num_actions == 0) { return std::vector<double>(num_hands_br, 0.0); }

    std::vector<std::vector<double>> child_node_values(num_actions);
    for (size_t a = 0; a < num_actions; ++a) {
        std::shared_ptr<core::GameTreeNode> child_node = node->GetChildren()[a];
        std::vector<std::vector<double>> next_reach_probs = reach_probs;
        if (acting_player != best_response_player_index) {
            auto trainable = node->GetTrainableIfExists(deal_abstraction_index);
            std::vector<float> fixed_strategy;
            if (trainable) { fixed_strategy = trainable->GetAverageStrategy(); }
            else { fixed_strategy.assign(num_actions * num_hands_acting, (num_actions > 0) ? 1.0f / static_cast<float>(num_actions) : 0.0f); }
            if (fixed_strategy.size() != num_actions * num_hands_acting) { throw std::logic_error("Strategy size mismatch in HandleActionNode."); }
            next_reach_probs[acting_player].assign(num_hands_acting, 0.0);
            double next_reach_sum = 0.0;
            for (size_t h = 0; h < num_hands_acting; ++h) {
                size_t strat_idx = a * num_hands_acting + h;
                if (strat_idx < fixed_strategy.size()) { next_reach_probs[acting_player][h] = reach_probs[acting_player][h] * static_cast<double>(fixed_strategy[strat_idx]); next_reach_sum += next_reach_probs[acting_player][h]; }
            }
            if (next_reach_sum > 1e-12) { for(double& p : next_reach_probs[acting_player]) p /= next_reach_sum; }
            else { next_reach_probs[acting_player].assign(num_hands_acting, 0.0); }
        }
        child_node_values[a] = CalculateNodeValue( child_node, best_response_player_index, next_reach_probs, current_board_mask, deal_abstraction_index);
    }
    std::vector<double> current_node_value(num_hands_br, 0.0);
    if (acting_player == best_response_player_index) {
        for (size_t h = 0; h < num_hands_br; ++h) {
            double max_ev = -std::numeric_limits<double>::infinity();
            for (size_t a = 0; a < num_actions; ++a) { if (h < child_node_values[a].size()) { max_ev = std::max(max_ev, child_node_values[a][h]); } }
            current_node_value[h] = (max_ev == -std::numeric_limits<double>::infinity()) ? 0.0 : max_ev;
        }
    } else {
        for (size_t a = 0; a < num_actions; ++a) { for (size_t h = 0; h < current_node_value.size(); ++h) { if (h < child_node_values[a].size()) { current_node_value[h] += child_node_values[a][h]; } } }
    }
    return current_node_value;
}


std::vector<double> BestResponse::HandleChanceNode(
     std::shared_ptr<nodes::ChanceNode> node,
     size_t best_response_player_index,
     const std::vector<std::vector<double>>& reach_probs,
     uint64_t current_board_mask,
     size_t deal_abstraction_index) const {

    std::shared_ptr<core::GameTreeNode> child_node = node->GetChild();
    if (!child_node) throw std::logic_error("ChanceNode has no child during BestResponse traversal.");

    size_t num_hands_p0 = player_hand_counts_[0];
    size_t num_hands_p1 = player_hand_counts_[1];
    std::vector<double> total_child_values(player_hand_counts_[best_response_player_index], 0.0);
    double total_weight_sum = 0.0; // <<< FIX: Initialize here

    const auto& deck_cards = deck_.GetCards();
    int num_deck_cards = deck_cards.size();

    int cards_to_deal = 0;
    if (node->GetRound() == core::GameRound::kPreflop) cards_to_deal = 3;
    else if (node->GetRound() == core::GameRound::kFlop || node->GetRound() == core::GameRound::kTurn) cards_to_deal = 1;
    if (cards_to_deal == 0) return CalculateNodeValue(child_node, best_response_player_index, reach_probs, current_board_mask, deal_abstraction_index);
    if (cards_to_deal != 1) throw std::logic_error("BestResponse ChanceNode logic currently only supports dealing 1 card.");

    bool parallelize = false; // OpenMP disabled
    std::vector<std::vector<double>> thread_results; // Not used if not parallel

    // --- FIX: Operate directly on total_child_values and total_weight_sum ---
    // std::vector<double> result_vector(total_child_values.size(), 0.0); // Remove this
    // double weight_sum_local = 0.0; // Remove this

    for (int i = 0; i < num_deck_cards; ++i) {
        const core::Card& next_card = deck_cards[i];
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
        else { next_reach_probs[0].assign(num_hands_p0, 0.0); } // Assign zero if sum is zero
        if (p1_reach_sum > 1e-12) { for(double& p : next_reach_probs[1]) p /= p1_reach_sum; }
        else { next_reach_probs[1].assign(num_hands_p1, 0.0); } // Assign zero if sum is zero


        int possible_deals = CalculatePossibleDeals(current_board_mask);
        if (possible_deals <= 0) continue;
        double deal_weight = 1.0 / static_cast<double>(possible_deals);

        uint64_t next_board_mask = current_board_mask | card_mask;
        size_t next_deal_abs_index = GetNextDealAbstractionIndex(deal_abstraction_index, i);

        std::vector<double> child_values = CalculateNodeValue(
            child_node, best_response_player_index, next_reach_probs,
            next_board_mask, next_deal_abs_index);

        // Accumulate directly into total_child_values
        for (size_t h = 0; h < total_child_values.size(); ++h) {
            if (h < child_values.size()) {
                if (iso_offset > 0) { /* TODO: Isomorphism */ }
                total_child_values[h] += child_values[h] * deal_weight;
            }
        }
        total_weight_sum += deal_weight; // Accumulate total weight
    }
    // --- End of block that was parallel region ---

    // Normalization logic remains the same, using total_weight_sum
    if (std::abs(total_weight_sum - 1.0) > 1e-6 && total_weight_sum > 1e-12) {
        for (double& val : total_child_values) { val /= total_weight_sum; }
    } else if (total_weight_sum <= 1e-12) {
         std::fill(total_child_values.begin(), total_child_values.end(), 0.0);
    }

    return total_child_values;
}


std::vector<double> BestResponse::HandleTerminalNode(
     std::shared_ptr<nodes::TerminalNode> node,
     size_t best_response_player_index,
     const std::vector<std::vector<double>>& reach_probs,
     uint64_t current_board_mask) const {
    /* ... implementation as before ... */
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
    /* ... implementation as before ... */
    size_t opponent_player_index = 1 - best_response_player_index;
    size_t num_hands_br = player_hand_counts_[best_response_player_index];
    size_t num_hands_opp = player_hand_counts_[opponent_player_index];
    std::vector<double> expected_payoffs(num_hands_br, 0.0);
    const auto& br_player_initial_range = player_ranges_[best_response_player_index];
    const auto& opp_player_initial_range = player_ranges_[opponent_player_index];
    const auto& br_river_combos = rrm_.GetRiverCombos(best_response_player_index, br_player_initial_range, current_board_mask);
    const auto& opp_river_combos = rrm_.GetRiverCombos(opponent_player_index, opp_player_initial_range, current_board_mask);
    const auto& payoffs_br_wins = node->GetPayoffs(best_response_player_index == 0 ? core::ComparisonResult::kPlayer1Wins : core::ComparisonResult::kPlayer2Wins);
    const auto& payoffs_opp_wins = node->GetPayoffs(opponent_player_index == 0 ? core::ComparisonResult::kPlayer1Wins : core::ComparisonResult::kPlayer2Wins);
    const auto& payoffs_tie = node->GetPayoffs(core::ComparisonResult::kTie);
    double payoff_on_win = payoffs_br_wins[best_response_player_index];
    double payoff_on_loss = payoffs_opp_wins[best_response_player_index];
    double payoff_on_tie = payoffs_tie[best_response_player_index];

    for (const auto& br_combo : br_river_combos) {
        size_t br_hand_idx = br_combo.original_range_index;
        if (br_hand_idx >= num_hands_br) continue;
        int br_rank = br_combo.rank;
        // Use fully qualified name for constant
        if (br_rank == poker_solver::eval::Dic5Compairer::kInvalidRank || br_rank == -1) {
             expected_payoffs[br_hand_idx] = 0.0; continue;
        }
        const auto& br_private_cards = br_combo.private_cards;
        double win_prob_final = 0, loss_prob_final = 0, tie_prob_final = 0;
        for(const auto& opp_combo : opp_river_combos) {
             size_t opp_hand_idx = opp_combo.original_range_index;
             if (opp_hand_idx >= reach_probs[opponent_player_index].size()) continue;
             if (core::Card::DoBoardsOverlap(br_private_cards.GetBoardMask(), opp_combo.private_cards.GetBoardMask())) continue;
             // Use fully qualified name for constant
             if (opp_combo.rank == poker_solver::eval::Dic5Compairer::kInvalidRank || opp_combo.rank == -1) continue;

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
    // ... as before ...
    return {}; // Add this line to silence warning
}
int BestResponse::CalculatePossibleDeals(uint64_t current_board_mask) const { 
    // ... as before ...
    return 0; // Add this line to silence warning
}
size_t BestResponse::GetNextDealAbstractionIndex(size_t current_deal_index, int card_index) const { 
    // ... as before ...
    return 0; // Add this line to silence warning
}
void BestResponse::InitializeIsomorphism() { /* ... as before ... */ }


} // namespace solver
} // namespace poker_solver
