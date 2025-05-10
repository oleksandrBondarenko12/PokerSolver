#include "solver/PCfrSolver.h"
#include "nodes/ActionNode.h"
#include "nodes/ChanceNode.h"
#include "nodes/ShowdownNode.h"
#include "nodes/TerminalNode.h"
#include "trainable/Trainable.h"
#include "trainable/DiscountedCfrTrainable.h"
#include "Library.h"
#include "Card.h"
#include "tools/Rule.h"

#include <stdexcept>
#include <vector>
#include <numeric>
#include <cmath>
#include <iostream>
#include <future>
#include <thread>
#include <algorithm>
#include <map>
#include <iomanip>
#include <utility> // For std::move

// Use aliases for namespaces (optional, but can make definitions cleaner)
// namespace core = poker_solver::core;
// namespace nodes = poker_solver::nodes;
// namespace ranges = poker_solver::ranges;
// namespace tree_ns = poker_solver::tree; // Alias for tree if 'tree' is ambiguous
// namespace config_ns = poker_solver::config; // Alias for config
// namespace utils_ns = poker_solver::utils; // Alias for utils

// Ensure ALL definitions are within these namespaces
namespace poker_solver {
namespace solver {

// --- Constructor ---
PCfrSolver::PCfrSolver(std::shared_ptr<tree::GameTree> game_tree, // Use tree::GameTree
                       std::shared_ptr<ranges::PrivateCardsManager> pcm,
                       std::shared_ptr<ranges::RiverRangeManager> rrm,
                       const config::Rule& rule,
                       Config solver_config)
    : Solver(std::move(game_tree)),
      pcm_(std::move(pcm)),
      rrm_(std::move(rrm)),
      deck_(rule.GetDeck()),
      config_(std::move(solver_config)),
      initial_board_mask_(core::Card::CardIntsToUint64(rule.GetInitialBoardCardsInt()))
{
    if (!game_tree_) {
        throw std::invalid_argument("PCfrSolver: GameTree cannot be null.");
    }
    if (!pcm_) {
        throw std::invalid_argument("PCfrSolver: PrivateCardsManager cannot be null.");
    }
     if (pcm_->GetNumPlayers() != num_players_) {
        throw std::invalid_argument("PCfrSolver: PrivateCardsManager must be for 2 players.");
     }
    if (!rrm_) {
        throw std::invalid_argument("PCfrSolver: RiverRangeManager cannot be null.");
    }

     std::vector<std::shared_ptr<core::GameTreeNode>> node_stack;
     if (game_tree_->GetRoot()) { // Use game_tree_ member
        node_stack.push_back(game_tree_->GetRoot());
     }
     int associated_nodes = 0;
     while (!node_stack.empty()) {
         std::shared_ptr<core::GameTreeNode> current = node_stack.back();
         node_stack.pop_back();

         if (!current) continue;

         if (auto action_node = std::dynamic_pointer_cast<nodes::ActionNode>(current)) {
             size_t player_idx = action_node->GetPlayerIndex();
             action_node->SetPlayerRange(&(pcm_->GetPlayerRange(player_idx))); // Use pcm_ member
             associated_nodes++;
             for(const auto& child : action_node->GetChildren()) {
                 if (child) node_stack.push_back(child);
             }
         } else if (auto chance_node = std::dynamic_pointer_cast<nodes::ChanceNode>(current)) {
             auto chance_child = chance_node->GetChild();
             if (chance_child) {
                 node_stack.push_back(chance_child);
             }
         }
     }
     std::cout << "[INFO] Pre-associated player ranges with " << associated_nodes << " action nodes." << std::endl;
}

// --- Solver Interface Implementation ---

void PCfrSolver::Train() {
    stop_signal_ = false;
    evs_calculated_ = false;

    std::cout << "[INFO] Starting PCFR training for " << config_.iteration_limit << " iterations..." << std::endl;
    std::cout << "[INFO] Initial Board Mask: 0x" << std::hex << initial_board_mask_ << std::dec << std::endl;
    std::cout << "[INFO] Threads: " << config_.num_threads << std::endl;

    std::vector<std::vector<double>> initial_reach_probs(num_players_);
    bool possible_to_train = true;
    for (size_t p = 0; p < num_players_; ++p) {
        const auto& reach_doubles = pcm_->GetInitialReachProbs(p);
        initial_reach_probs[p] = reach_doubles;
        if (pcm_->GetPlayerRange(p).empty() || initial_reach_probs[p].empty() ||
            std::accumulate(initial_reach_probs[p].begin(), initial_reach_probs[p].end(), 0.0) < 1e-12) {
             std::cerr << "[ERROR] Player " << p << " has an empty initial range or zero total reach probability. Cannot train." << std::endl;
             possible_to_train = false;
        }
    }

    if (!possible_to_train) {
        std::cerr << "[ERROR] Training aborted due to empty range or zero reach probability for a player." << std::endl;
        return;
    }

    uint64_t start_time = utils::TimeSinceEpochMillisec();
    int completed_iterations = 0; // Variable to track iterations run

    if (config_.iteration_limit <= 0) {
        std::cout << "[INFO] Iteration limit is " << config_.iteration_limit << ". No training will occur." << std::endl;
    } else {
        for (int i = 1; i <= config_.iteration_limit; ++i) {
            completed_iterations = i; // Update completed iterations
            if (stop_signal_) {
                std::cout << "[INFO] Training stopped prematurely during iteration " << i << "." << std::endl;
                // If stopped, completed_iterations will hold the iteration number it was stopped *during*.
                // If you want strictly completed iterations, you might use `completed_iterations = i - 1;` here
                // before breaking, but `i` is usually what's reported.
                break;
            }

            for (int traverser = 0; traverser < static_cast<int>(num_players_); ++traverser) {
                 try {
                     cfr_utility(game_tree_->GetRoot(), initial_reach_probs, traverser, i, this->initial_board_mask_, 1.0);
                 } catch (const std::exception& e) {
                     std::cerr << "[FATAL ERROR] Exception during CFR iteration " << i
                               << " for traverser " << traverser << ": " << e.what() << std::endl;
                     throw; // Re-throw to stop execution
                 }
            }

            if (i % 100 == 0 || i == config_.iteration_limit) {
                 uint64_t current_time = utils::TimeSinceEpochMillisec();
                 double elapsed_sec = static_cast<double>(current_time - start_time) / 1000.0;
                 // std::cout << "[INFO] Iteration " << i << "/" << config_.iteration_limit
                    //       << " completed. Time elapsed: " << std::fixed << std::setprecision(2) << elapsed_sec << "s." << std::endl;
            }
        }
    }


     uint64_t end_time = utils::TimeSinceEpochMillisec();
     double total_sec = static_cast<double>(end_time - start_time) / 1000.0;

     // Use completed_iterations for the log message
     //std::cout << "[INFO] Training finished after "
       //        << completed_iterations // Use the tracked variable
         //      << " iterations. Total time: " << std::fixed << std::setprecision(2) << total_sec << "s." << std::endl;
}

void PCfrSolver::Stop() {
    stop_signal_ = true;
}

json PCfrSolver::DumpStrategy(bool dump_evs, int max_depth) const {
    json result;
    if (!game_tree_ || !game_tree_->GetRoot()) {
        result["error"] = "Game tree is empty or not initialized.";
        return result;
    }
    std::cout << "[INFO] Dumping strategy... (EVs: " << dump_evs << ", MaxDepth: " << max_depth << ")" << std::endl;
    result = dump_strategy_recursive(game_tree_->GetRoot(), dump_evs, 0, max_depth);
    // Ensure metadata is added to the result from dump_strategy_recursive, not overwriting it
    if (result.is_null()) result = json::object(); // Ensure result is an object if tree was empty/pruned
    result["metadata"]["dump_evs"] = dump_evs;
    result["metadata"]["max_depth"] = max_depth == -1 ? "unlimited" : std::to_string(max_depth);
    return result;
}

// --- Private Recursive CFR Function ---
std::vector<double> PCfrSolver::cfr_utility(
    std::shared_ptr<core::GameTreeNode> node,
    const std::vector<std::vector<double>>& reach_probs,
    int traverser,
    int iteration,
    uint64_t current_board_mask,
    double chance_reach)
{
    if (!node) {
        throw std::logic_error("cfr_utility called with null node.");
    }

    // --- DEBUG PRINT ---
    //std::cout << "[DEBUG CFRUtil] Iter: " << iteration << ", Trav: " << traverser
      //        << ", NodeDepth: " << node->GetDepth() // Assuming depth is set
        //      << ", NodeType: ";
    switch(node->GetNodeType()) {
        case core::GameTreeNodeType::kAction: /*std::cout << "Action (P" << std::dynamic_pointer_cast<nodes::ActionNode>(node)->GetPlayerIndex() << ")";*/ break;
        case core::GameTreeNodeType::kChance: /*std::cout << "Chance";*/ break;
        case core::GameTreeNodeType::kShowdown: /*std::cout << "Showdown";*/ break;
        case core::GameTreeNodeType::kTerminal: /*std::cout << "Terminal";*/ break;
        default: std::cout << "Unknown";
    }
    //std::cout << ", Round: " << core::GameTreeNode::GameRoundToString(node->GetRound())
      //        << ", Board: 0x" << std::hex << current_board_mask << std::dec
        //      << " (Cards: " << core::Card::Uint64ToCards(current_board_mask).size() << ")"
          //    << ", ChanceReach: " << chance_reach
            //  << std::endl;

    bool is_terminal = false;
    core::GameTreeNodeType node_type = node->GetNodeType();
    if (node_type == core::GameTreeNodeType::kTerminal ||
        node_type == core::GameTreeNodeType::kShowdown) {
        is_terminal = true;
    }

    if (traverser < 0 || traverser >= static_cast<int>(reach_probs.size())) {
         throw std::out_of_range("Invalid traverser index in cfr_utility.");
    }
    if (!is_terminal && std::accumulate(reach_probs[traverser].begin(), reach_probs[traverser].end(), 0.0) < 1e-12) {
         return std::vector<double>(pcm_->GetPlayerRange(traverser).size(), 0.0);
    }

    switch (node_type) {
        case core::GameTreeNodeType::kTerminal:
            return cfr_terminal_node(std::dynamic_pointer_cast<nodes::TerminalNode>(node), reach_probs, traverser, chance_reach);
        case core::GameTreeNodeType::kShowdown:
            return cfr_showdown_node(std::dynamic_pointer_cast<nodes::ShowdownNode>(node), reach_probs, traverser, current_board_mask, chance_reach);
        case core::GameTreeNodeType::kChance:
            return cfr_chance_node(std::dynamic_pointer_cast<nodes::ChanceNode>(node), reach_probs, traverser, iteration, current_board_mask, chance_reach);
        case core::GameTreeNodeType::kAction:
            return cfr_action_node(std::dynamic_pointer_cast<nodes::ActionNode>(node), reach_probs, traverser, iteration, current_board_mask, chance_reach);
        default:
            throw std::logic_error("cfr_utility encountered unknown node type.");
    }
}


// --- Action Node Helper ---
std::vector<double> PCfrSolver::cfr_action_node(
    std::shared_ptr<nodes::ActionNode> node,
    const std::vector<std::vector<double>>& reach_probs,
    int traverser,
    int iteration,
    uint64_t current_board_mask,
    double chance_reach)
{
    size_t acting_player = node->GetPlayerIndex();
    size_t opponent_player = 1 - acting_player;
    const auto& actions = node->GetActions();
    const auto& children = node->GetChildren();
    size_t num_actions = actions.size();

    size_t traverser_num_hands = pcm_->GetPlayerRange(traverser).size();
    std::vector<double> node_utility(traverser_num_hands, 0.0);

    const auto* player_range_ptr = node->GetPlayerRangeRaw();
    if (!player_range_ptr) throw std::runtime_error("Player range not set on ActionNode.");
    size_t acting_player_num_hands = player_range_ptr->size();

    if (num_actions == 0 || acting_player_num_hands == 0) {
        return node_utility;
    }

    auto trainable = node->GetTrainable(0);
    if (!trainable) throw std::runtime_error("Failed to get Trainable object.");

    std::vector<double> current_strategy_local;
    const std::vector<double>& trainable_strategy = trainable->GetCurrentStrategy();

    if (trainable_strategy.size() == (num_actions * acting_player_num_hands)) {
        current_strategy_local = trainable_strategy;
    } else {
        //std::cerr << "[ERROR] Strategy size mismatch (" << trainable_strategy.size()
          //        << ") from GetCurrentStrategy() in ActionNode (expected "
            //      << (num_actions * acting_player_num_hands) << "). Using uniform." << std::endl;
        double uniform_prob = (num_actions > 0) ? 1.0 / static_cast<double>(num_actions) : 0.0;
        current_strategy_local.assign(num_actions * acting_player_num_hands, uniform_prob);
    }

    std::vector<std::vector<double>> child_utilities(num_actions, std::vector<double>(traverser_num_hands, 0.0));
    for (size_t a = 0; a < num_actions; ++a) {
        std::vector<std::vector<double>> next_reach_probs = reach_probs;
        for (size_t h = 0; h < acting_player_num_hands; ++h) {
            size_t strat_idx = h * num_actions + a; // Corrected to Hand-Major for current_strategy_local access
             if (acting_player < next_reach_probs.size() && h < next_reach_probs[acting_player].size() && strat_idx < current_strategy_local.size()){
                next_reach_probs[acting_player][h] *= current_strategy_local[strat_idx];
            } else {
                 std::cerr << "[ERROR] Index out of bounds during reach prob update in cfr_action_node. "
                           << "acting_player=" << acting_player << ", next_reach_probs.size()=" << next_reach_probs.size()
                           << ", h=" << h 
                           << ", strat_idx=" << strat_idx << ", current_strategy_local.size()=" << current_strategy_local.size()
                           << std::endl;
                 if(acting_player < next_reach_probs.size() && h < next_reach_probs[acting_player].size()) {
                    next_reach_probs[acting_player][h] = 0.0;
                 }
            }
        }
        if (a < children.size() && children[a]) {
            child_utilities[a] = cfr_utility(children[a], next_reach_probs, traverser, iteration, current_board_mask, chance_reach);
        } else {
             std::cerr << "[ERROR] Missing or null child node for action index " << a << " in cfr_action_node." << std::endl;
             child_utilities[a].assign(traverser_num_hands, 0.0);
        }
    }

    if (acting_player == traverser) {
        for (size_t h = 0; h < traverser_num_hands; ++h) {
            node_utility[h] = 0.0;
            for (size_t a = 0; a < num_actions; ++a) {
                size_t strat_idx = h * num_actions + a; // Corrected to Hand-Major
                 if (strat_idx < current_strategy_local.size() && a < child_utilities.size() && h < child_utilities[a].size()) {
                    node_utility[h] += current_strategy_local[strat_idx] * child_utilities[a][h];
                 }
            }
        }
    } else {
        for (size_t h = 0; h < traverser_num_hands; ++h) {
            node_utility[h] = 0.0;
            for (size_t a = 0; a < num_actions; ++a) {
                 if (a < child_utilities.size() && h < child_utilities[a].size()) {
                    node_utility[h] += child_utilities[a][h];
                 }
            }
        }
    }

    if (acting_player == traverser) {
        std::vector<double> weighted_regrets(num_actions * acting_player_num_hands);
        double opponent_reach_sum = 0.0;
        if (opponent_player < reach_probs.size()) { // Bounds check for opponent_player
             opponent_reach_sum = std::accumulate(reach_probs[opponent_player].begin(), reach_probs[opponent_player].end(), 0.0);
        }
        double scalar_weight_for_regret_update = opponent_reach_sum * chance_reach;

        std::vector<double> player_reach_weights_vec(acting_player_num_hands);
        for (size_t h = 0; h < acting_player_num_hands; ++h) {
             if (acting_player < reach_probs.size() && h < reach_probs[acting_player].size()){ // Bounds check
                player_reach_weights_vec[h] = reach_probs[acting_player][h] * chance_reach;
             } else {
                player_reach_weights_vec[h] = 0.0;
             }
            for (size_t a = 0; a < num_actions; ++a) {
                 size_t idx = h * num_actions + a; // Corrected to Hand-Major
                 if (idx < weighted_regrets.size() && a < child_utilities.size() && h < child_utilities[a].size() && h < node_utility.size()) {
                     weighted_regrets[idx] = child_utilities[a][h] - node_utility[h];
                 } else {
                      if (idx < weighted_regrets.size()) {
                          weighted_regrets[idx] = 0.0;
                      }
                 }
            }
        }
        trainable->UpdateRegrets(weighted_regrets, iteration, scalar_weight_for_regret_update);
        trainable->AccumulateAverageStrategy(current_strategy_local, iteration, player_reach_weights_vec);
    }
    return node_utility;
}


// --- Chance Node Helper ---
std::vector<double> PCfrSolver::cfr_chance_node(
    std::shared_ptr<nodes::ChanceNode> node,
    const std::vector<std::vector<double>>& reach_probs,
    int traverser,
    int iteration,
    uint64_t current_board_mask,
    double parent_chance_reach)
{

    std::shared_ptr<core::GameTreeNode> child = node->GetChild();
    if (!child) throw std::logic_error("Chance node has no child.");

    core::GameRound round_after_chance = node->GetRound();
    int num_cards_to_deal = 0;
    if (round_after_chance == core::GameRound::kFlop) num_cards_to_deal = 3;
    else if (round_after_chance == core::GameRound::kTurn) num_cards_to_deal = 1;
    else if (round_after_chance == core::GameRound::kRiver) num_cards_to_deal = 1;
    else { // Should not happen if tree is built correctly
        std::cerr << "[ERROR] cfr_chance_node: Invalid round after chance: " << core::GameTreeNode::GameRoundToString(round_after_chance) << std::endl;
        throw std::logic_error("Invalid round after ChanceNode in cfr_chance_node.");
    }


    uint64_t unavailable_mask = current_board_mask; // Start with existing board
    // Add cards from players' hands that can actually reach this node
    for(size_t p=0; p < num_players_; ++p) {
        if (p >= reach_probs.size()) continue; // Safety check
        const auto& range = pcm_->GetPlayerRange(p);
        if (range.size() != reach_probs[p].size()) {
            //std::cerr << "[WARN] cfr_chance_node: Mismatch between range size (" << range.size()
              //        << ") and reach_probs size (" << reach_probs[p].size() << ") for player " << p << std::endl;
            // Decide how to handle: skip player, throw, or continue with caution
            // For now, let's be cautious and potentially skip this player's blockers if sizes mismatch significantly
            if (std::abs(static_cast<long>(range.size()) - static_cast<long>(reach_probs[p].size())) > 5) { // Arbitrary threshold
                continue;
            }
        }
        for(size_t h=0; h < std::min(range.size(), reach_probs[p].size()); ++h) {
            if (reach_probs[p][h] > 1e-12) { // Only consider hands that can reach
                unavailable_mask |= range[h].GetBoardMask();
            }
        }
    }


    std::vector<int> available_card_indices;
    const auto& all_deck_cards = deck_.GetCards();
    available_card_indices.reserve(core::kNumCardsInDeck);
    for(int i=0; i < core::kNumCardsInDeck; ++i) {
        if (!core::Card::DoBoardsOverlap(1ULL << i, unavailable_mask)) {
            available_card_indices.push_back(i);
        }
    }

    if (static_cast<int>(available_card_indices.size()) < num_cards_to_deal) {
        return std::vector<double>(pcm_->GetPlayerRange(traverser).size(), 0.0);
    }

    utils::Combinations<int> combinations(available_card_indices, num_cards_to_deal);
    const auto& outcomes = combinations.GetCombinations();
    double num_possible_outcomes = static_cast<double>(outcomes.size());

    if (num_possible_outcomes < 1.0) {
        return std::vector<double>(pcm_->GetPlayerRange(traverser).size(), 0.0);
    }

    double outcome_probability = 1.0 / num_possible_outcomes;
    double next_node_chance_reach = parent_chance_reach * outcome_probability;

    size_t traverser_num_hands = pcm_->GetPlayerRange(traverser).size();
    std::vector<double> total_expected_utility(traverser_num_hands, 0.0);


    // --- DEBUG PRINT ---
    // std::cout << "  [DEBUG ChanceEntry] Board BEFORE deal: 0x" << std::hex << current_board_mask << std::dec
      //         << " (Cards: " << core::Card::Uint64ToCards(current_board_mask).size() << ")" << std::endl;
    // --- END DEBUG PRINT ---

    for (const auto& outcome_cards : outcomes) {
        uint64_t outcome_board_mask = core::Card::CardIntsToUint64(outcome_cards);
        uint64_t next_board_mask = current_board_mask | outcome_board_mask;

        // --- DEBUG PRINT ---
        // std::cout << "    [DEBUG ChanceDeal] Dealt: ";
        // for(int ci : outcome_cards) { std::cout << core::Card::IntToString(ci) << " "; }
        // std::cout << "(Mask: 0x" << std::hex << outcome_board_mask << std::dec << ")"
          //         << " -> Next Board: 0x" << std::hex << next_board_mask << std::dec
           //        << " (Cards: " << core::Card::Uint64ToCards(next_board_mask).size() << ")"
             //      << std::endl;
        // --- END DEBUG PRINT ---

        std::vector<std::vector<double>> next_reach_probs = reach_probs;
        bool traverser_can_reach_this_outcome = false;
        bool opponent_can_reach_this_outcome = false;
        int opponent_player = 1 - traverser;

        for (size_t p = 0; p < num_players_; ++p) {
             if (p >= next_reach_probs.size()) continue;
             const auto& range = pcm_->GetPlayerRange(p);
             double current_reach_sum_for_outcome = 0.0;
             for (size_t h = 0; h < std::min(range.size(), next_reach_probs[p].size()); ++h) {
                  if (core::Card::DoBoardsOverlap(range[h].GetBoardMask(), outcome_board_mask)) {
                      next_reach_probs[p][h] = 0.0;
                  }
                  current_reach_sum_for_outcome += next_reach_probs[p][h];
             }
             if (p == static_cast<size_t>(traverser) && current_reach_sum_for_outcome > 1e-12) traverser_can_reach_this_outcome = true;
             if (p == static_cast<size_t>(opponent_player) && current_reach_sum_for_outcome > 1e-12) opponent_can_reach_this_outcome = true;
        }

        if (traverser_can_reach_this_outcome || opponent_can_reach_this_outcome) {
             std::vector<double> child_utility = cfr_utility(child, next_reach_probs, traverser, iteration, next_board_mask, next_node_chance_reach);
             if (total_expected_utility.size() == child_utility.size()) {
                 for (size_t i = 0; i < total_expected_utility.size(); ++i) {
                     total_expected_utility[i] += child_utility[i];
                 }
             } else if (!child_utility.empty()){
                  std::cerr << "[ERROR] Utility vector size mismatch from chance node child." << std::endl;
                  throw std::runtime_error("Utility vector size mismatch in chance node.");
             }
        }
    }
    return total_expected_utility;
}


// --- Showdown Node Helper ---
std::vector<double> PCfrSolver::cfr_showdown_node(
    std::shared_ptr<nodes::ShowdownNode> node,
    const std::vector<std::vector<double>>& reach_probs,
    int traverser,
    uint64_t final_board_mask, double chance_reach)
{
    /*
    // --- DEBUG PRINT ---
    std::cout << "  [DEBUG ShowdownNode] Iter: " << "N/A" // Iteration not directly passed here, but known from caller
              << ", Trav: " << traverser
              << ", FINAL Board for RRM: 0x" << std::hex << final_board_mask << std::dec
              << " (Cards: " << core::Card::Uint64ToCards(final_board_mask).size() << ")"
              << ", ChanceReach: " << chance_reach
              << std::endl;
    // --- END DEBUG PRINT ---
    */

    int opponent_player = 1 - traverser;
    const auto& traverser_range = pcm_->GetPlayerRange(traverser);
    const auto& opponent_range = pcm_->GetPlayerRange(opponent_player);
    size_t traverser_hands = traverser_range.size();
    size_t opponent_hands = opponent_range.size();

    std::vector<double> traverser_utility(traverser_hands, 0.0);

    if (opponent_player < 0 || opponent_player >= static_cast<int>(reach_probs.size()) || (opponent_hands > 0 && opponent_hands != reach_probs[opponent_player].size())) {
        //std::cerr << "[ERROR] Opponent range/reach_probs mismatch in cfr_showdown_node. Opponent: " << opponent_player
          //        << ", Opponent Hands: " << opponent_hands
            //      << ", Reach Probs size for opp: " << (opponent_player < static_cast<int>(reach_probs.size()) ? reach_probs[opponent_player].size() : -1)
              //    << std::endl;
        return traverser_utility;
    }

    const auto& traverser_combos = rrm_->GetRiverCombos(traverser, traverser_range, final_board_mask);
    const auto& opponent_combos = rrm_->GetRiverCombos(opponent_player, opponent_range, final_board_mask);

    const auto& p0_wins_payoffs = node->GetPayoffs(core::ComparisonResult::kPlayer1Wins);
    const auto& p1_wins_payoffs = node->GetPayoffs(core::ComparisonResult::kPlayer2Wins);
    const auto& tie_payoffs     = node->GetPayoffs(core::ComparisonResult::kTie);

    std::unordered_map<size_t, size_t> oppo_orig_idx_to_river_idx;
    for(size_t rc_idx = 0; rc_idx < opponent_combos.size(); ++rc_idx) {
        oppo_orig_idx_to_river_idx[opponent_combos[rc_idx].original_range_index] = rc_idx;
    }

    for (const auto& trav_c : traverser_combos) {
        size_t trav_orig_idx = trav_c.original_range_index;
        if (trav_orig_idx >= traverser_utility.size() || (static_cast<size_t>(traverser) < reach_probs.size() && trav_orig_idx >= reach_probs[traverser].size())) continue;
        if (static_cast<size_t>(traverser) >= reach_probs.size() || reach_probs[traverser][trav_orig_idx] < 1e-12) continue;

        uint64_t traverser_private_mask = trav_c.private_cards.GetBoardMask();
        double expected_value_for_hand = 0.0;

        for (size_t oppo_orig_idx = 0; oppo_orig_idx < opponent_hands; ++oppo_orig_idx) {
            if (reach_probs[opponent_player][oppo_orig_idx] < 1e-12) continue;

            const core::PrivateCards& oppo_hand_orig = opponent_range[oppo_orig_idx];
            uint64_t opponent_private_mask = oppo_hand_orig.GetBoardMask();

            if (core::Card::DoBoardsOverlap(traverser_private_mask, opponent_private_mask)) {
                continue;
            }

            auto map_it = oppo_orig_idx_to_river_idx.find(oppo_orig_idx);
            if (map_it == oppo_orig_idx_to_river_idx.end()) {
                continue;
            }
            const auto& oppo_c = opponent_combos[map_it->second];

            double payoff_for_traverser = 0.0;
            if (trav_c.rank < oppo_c.rank) {
                payoff_for_traverser = (traverser == 0) ? p0_wins_payoffs[0] : p1_wins_payoffs[1];
            } else if (oppo_c.rank < trav_c.rank) {
                payoff_for_traverser = (traverser == 0) ? p1_wins_payoffs[0] : p0_wins_payoffs[1];
            } else {
                payoff_for_traverser = tie_payoffs[traverser];
            }
            expected_value_for_hand += reach_probs[opponent_player][oppo_orig_idx] * payoff_for_traverser;
        }
        traverser_utility[trav_orig_idx] = expected_value_for_hand * chance_reach;
    }
    return traverser_utility;
}


// --- Terminal Node Helper (Corrected) ---
std::vector<double> PCfrSolver::cfr_terminal_node(
    std::shared_ptr<nodes::TerminalNode> node,
    const std::vector<std::vector<double>>& reach_probs,
    int traverser,
    double chance_reach)
{
    const auto& payoffs = node->GetPayoffs();
    if (traverser < 0 || traverser >= static_cast<int>(payoffs.size())) {
         throw std::out_of_range("Traverser index out of bounds for payoffs vector in terminal node.");
    }
    const double payoff_for_traverser = payoffs[traverser];

    int opponent_player = 1 - traverser;
    const auto& traverser_range = pcm_->GetPlayerRange(traverser);
    const auto& opponent_range = pcm_->GetPlayerRange(opponent_player);
    size_t traverser_hands = traverser_range.size();
    size_t opponent_hands = opponent_range.size();

    std::vector<double> node_utility(traverser_hands, 0.0);

    if (opponent_player < 0 || opponent_player >= static_cast<int>(reach_probs.size()) || (opponent_hands > 0 && opponent_hands != reach_probs[opponent_player].size()) ) {
        // std::cerr << "[ERROR] Opponent range/reach_probs mismatch in cfr_terminal_node. Opponent: " << opponent_player
          //        << ", Opponent Hands: " << opponent_hands
            //      << ", Reach Probs size for opp: " << (opponent_player < static_cast<int>(reach_probs.size()) ? reach_probs[opponent_player].size() : -1)
              //    << std::endl;
         return node_utility;
    }

    for (size_t h_i = 0; h_i < traverser_hands; ++h_i) {
        if (static_cast<size_t>(traverser) >= reach_probs.size() || h_i >= reach_probs[traverser].size() || reach_probs[traverser][h_i] < 1e-12) {
            continue;
        }
        uint64_t traverser_mask = traverser_range[h_i].GetBoardMask();
        double compatible_opponent_reach_sum = 0.0;
        for (size_t h_j = 0; h_j < opponent_hands; ++h_j) {
            uint64_t opponent_mask = opponent_range[h_j].GetBoardMask();
            if (!core::Card::DoBoardsOverlap(traverser_mask, opponent_mask)) {
                if (static_cast<size_t>(opponent_player) < reach_probs.size() && h_j < reach_probs[opponent_player].size()) { // Bounds check
                   compatible_opponent_reach_sum += reach_probs[opponent_player][h_j];
                }
            }
        }
        node_utility[h_i] = payoff_for_traverser * compatible_opponent_reach_sum * chance_reach;
    }
    return node_utility;
}


// --- Dump Strategy Helper ---
json PCfrSolver::dump_strategy_recursive(
        const std::shared_ptr<core::GameTreeNode>& node,
        bool dump_evs,
        int current_depth,
        int max_depth) const {
    json result;
    if (!node || (max_depth >= 0 && current_depth > max_depth)) {
        return nullptr;
    }

    result["round"] = core::GameTreeNode::GameRoundToString(node->GetRound());
    result["pot"] = node->GetPot();
    result["depth"] = current_depth;

    if (auto action_node = std::dynamic_pointer_cast<const nodes::ActionNode>(node)) {
        result["node_type"] = "Action";
        result["player"] = action_node->GetPlayerIndex();
        auto trainable = action_node->GetTrainableIfExists(0);
        if (trainable) {
            result["strategy_data"] = trainable->DumpStrategy(dump_evs);
        } else {
            result["strategy_data"] = "Not trained";
        }
        json children_json = json::object();
        const auto& actions = action_node->GetActions();
        const auto& children = action_node->GetChildren();
        if (actions.size() == children.size()) {
             for (size_t i = 0; i < actions.size(); ++i) {
                 if (children[i]) {
                     json child_dump = dump_strategy_recursive(children[i], dump_evs, current_depth + 1, max_depth);
                     if (!child_dump.is_null()) {
                         children_json[actions[i].ToString()] = child_dump;
                     }
                 }
             }
        } else {
             std::cerr << "[WARN] DumpStrategy: Action/Children size mismatch at depth " << current_depth << std::endl;
        }
        if (!children_json.empty()) result["children"] = children_json;

    } else if (auto chance_node = std::dynamic_pointer_cast<const nodes::ChanceNode>(node)) {
        result["node_type"] = "Chance";
        json dealt_cards_json = json::array();
        for(const auto& card : chance_node->GetDealtCards()) {
            if (!card.IsEmpty()) {
                dealt_cards_json.push_back(card.ToString());
            } else {
                 dealt_cards_json.push_back("InvalidCard");
            }
        }
        result["dealt_cards"] = dealt_cards_json;
        // Removed IsDonkOpportunity as it was removed from ChanceNode definition
        if (chance_node->GetChild()) {
            json child_dump = dump_strategy_recursive(chance_node->GetChild(), dump_evs, current_depth + 1, max_depth);
            if (!child_dump.is_null()) result["child"] = child_dump;
        }

    } else if (auto showdown_node = std::dynamic_pointer_cast<const nodes::ShowdownNode>(node)) {
        result["node_type"] = "Showdown";
    } else if (auto terminal_node = std::dynamic_pointer_cast<const nodes::TerminalNode>(node)) {
        result["node_type"] = "Terminal";
        result["payoffs"] = terminal_node->GetPayoffs();
    } else {
        result["node_type"] = "Unknown";
    }

    bool is_effectively_empty = true;
    if (result.contains("children") && !result["children"].empty()) is_effectively_empty = false;
    if (result.contains("child") && !result["child"].is_null()) is_effectively_empty = false;
    if (result["node_type"] == "Terminal" || result["node_type"] == "Showdown") is_effectively_empty = false;
    if (result["node_type"] == "Action" && result.contains("strategy_data") && result["strategy_data"] != "Not trained" && !result["strategy_data"].empty() ) is_effectively_empty = false;


    if (is_effectively_empty && (result["node_type"] != "Terminal" && result["node_type"] != "Showdown")) {
         return nullptr;
    }
    return result;
}

} // namespace solver
} // namespace poker_solver