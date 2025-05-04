#include "solver/PCfrSolver.h"
#include "nodes/ActionNode.h"
#include "nodes/ChanceNode.h"
#include "nodes/ShowdownNode.h"
#include "nodes/TerminalNode.h"
#include "trainable/Trainable.h"
#include "trainable/DiscountedCfrTrainable.h" // Assuming DCFR for now
#include "Library.h"         // For utils::Combinations
#include "Card.h"            // For Card utilities

#include <stdexcept>
#include <vector>
#include <numeric>   // For std::accumulate
#include <cmath>     // For std::abs, std::max, std::pow
#include <iostream>  // For logging/debugging
#include <future>    // For potential multi-threading later
#include <thread>    // For potential multi-threading later
#include <algorithm> // For std::fill, std::max, std::copy
#include <map>       // Used in DumpStrategy
#include <iomanip>   // For std::hex/dec in logging

// Use aliases for namespaces
namespace core = poker_solver::core;
namespace nodes = poker_solver::nodes;
namespace ranges = poker_solver::ranges;
namespace solver = poker_solver::solver;
namespace config = poker_solver::config;
namespace utils = poker_solver::utils;

namespace poker_solver {
namespace solver {

// --- Constructor ---
PCfrSolver::PCfrSolver(std::shared_ptr<tree::GameTree> game_tree,
                       std::shared_ptr<ranges::PrivateCardsManager> pcm,
                       std::shared_ptr<ranges::RiverRangeManager> rrm,
                       const config::Rule& rule,
                       Config solver_config)
    : Solver(std::move(game_tree)), // Initialize base class
      pcm_(std::move(pcm)),
      rrm_(std::move(rrm)),
      deck_(rule.GetDeck()), // Copy deck from rule
      config_(std::move(solver_config))
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

    // Extract initial board mask from the rule if possible, or pass separately
    std::vector<int> initial_board_ints; // Example: Get from Rule if it exists
    initial_board_mask_ = core::Card::CardIntsToUint64(initial_board_ints);

     // Pre-associate ranges with ActionNodes
     std::vector<std::shared_ptr<core::GameTreeNode>> node_stack;
     if (game_tree_->GetRoot()) {
        node_stack.push_back(game_tree_->GetRoot());
     }
     int associated_nodes = 0;
     while (!node_stack.empty()) {
         std::shared_ptr<core::GameTreeNode> current = node_stack.back();
         node_stack.pop_back();

         if (!current) continue; // Skip null nodes if any

         if (auto action_node = std::dynamic_pointer_cast<nodes::ActionNode>(current)) {
             size_t player_idx = action_node->GetPlayerIndex();
             action_node->SetPlayerRange(&(pcm_->GetPlayerRange(player_idx)));
             associated_nodes++;
             for(const auto& child : action_node->GetChildren()) {
                 if (child) node_stack.push_back(child);
             }
         } else if (auto chance_node = std::dynamic_pointer_cast<nodes::ChanceNode>(current)) {
             if(chance_node->GetChild()) node_stack.push_back(chance_node->GetChild());
         }
         // Terminal/Showdown nodes have no children to add
     }
     std::cout << "[INFO] Pre-associated player ranges with " << associated_nodes << " action nodes." << std::endl;
}

// --- Solver Interface Implementation ---

void PCfrSolver::Train() {
    stop_signal_ = false;
    evs_calculated_ = false;

    std::cout << "[INFO] Starting PCFR training for " << config_.iteration_limit << " iterations..." << std::endl;
    std::cout << "[INFO] Initial Board Mask: 0x" << std::hex << initial_board_mask_ << std::dec << std::endl;
    std::cout << "[INFO] Threads: " << config_.num_threads << std::endl; // Still single-threaded

    std::vector<std::vector<double>> initial_reach_probs(num_players_);
    bool possible_to_train = true;
    for (size_t p = 0; p < num_players_; ++p) {
        const auto& reach_floats = pcm_->GetInitialReachProbs(p);
        initial_reach_probs[p].assign(reach_floats.begin(), reach_floats.end());
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

    for (int i = 1; i <= config_.iteration_limit; ++i) {
        if (stop_signal_) {
            std::cout << "[INFO] Training stopped prematurely at iteration " << i << "." << std::endl;
            break;
        }

        for (int traverser = 0; traverser < static_cast<int>(num_players_); ++traverser) {
             try {
                 cfr_utility(game_tree_->GetRoot(), initial_reach_probs, traverser, i, initial_board_mask_, 1.0);
             } catch (const std::exception& e) {
                 std::cerr << "[FATAL ERROR] Exception during CFR iteration " << i
                           << " for traverser " << traverser << ": " << e.what() << std::endl;
                 throw;
             }
        }

        if (i % 100 == 0 || i == config_.iteration_limit) {
             uint64_t current_time = utils::TimeSinceEpochMillisec();
             double elapsed_sec = static_cast<double>(current_time - start_time) / 1000.0;
             std::cout << "[INFO] Iteration " << i << "/" << config_.iteration_limit
                       << " completed. Time elapsed: " << elapsed_sec << "s." << std::endl;
             // TODO: Add exploitability calculation here if desired later.
        }
    }

     uint64_t end_time = utils::TimeSinceEpochMillisec();
     double total_sec = static_cast<double>(end_time - start_time) / 1000.0;
     int actual_iterations = stop_signal_ ? -1 : config_.iteration_limit;
     std::cout << "[INFO] Training finished after " << (stop_signal_ ? "early stop" : std::to_string(actual_iterations))
               << " iterations. Total time: " << total_sec << "s." << std::endl;
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
    result["metadata"]["dump_evs"] = dump_evs;
    result["metadata"]["max_depth"] = max_depth == -1 ? "unlimited" : std::to_string(max_depth);
    return result;
}

// --- Private Recursive CFR Function ---
std::vector<double> PCfrSolver::cfr_utility(
    std::shared_ptr<core::GameTreeNode> node,
    const std::vector<std::vector<double>>& reach_probs, // pi_i(h), pi_{-i}(h)
    int traverser,
    int iteration,
    uint64_t current_board_mask,
    double chance_reach) // pi_c(history)
{
    if (!node) {
        throw std::logic_error("cfr_utility called with null node.");
    }

    bool is_terminal = false;
    core::GameTreeNodeType node_type = node->GetNodeType();
    if (node_type == core::GameTreeNodeType::kTerminal ||
        node_type == core::GameTreeNodeType::kShowdown) {
        is_terminal = true;
    }

    // Optimization: Prune if traverser cannot reach this node
    if (!is_terminal && std::accumulate(reach_probs[traverser].begin(), reach_probs[traverser].end(), 0.0) < 1e-12) {
         return std::vector<double>(pcm_->GetPlayerRange(traverser).size(), 0.0);
    }

    // Dispatch based on node type
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
    std::vector<double> node_utility(traverser_num_hands, 0.0); // Utility FOR TRAVERSER

    const auto* player_range_ptr = node->GetPlayerRangeIfExists();
    if (!player_range_ptr) throw std::runtime_error("Player range not set on ActionNode.");
    size_t acting_player_num_hands = player_range_ptr->size();

    if (num_actions == 0 || acting_player_num_hands == 0) {
        return node_utility;
    }

    auto trainable = node->GetTrainable(0); // Assume perfect recall
    if (!trainable) throw std::runtime_error("Failed to get Trainable object.");

    // Get Current Strategy (for acting player)
    const std::vector<float>& current_strategy_float = trainable->GetCurrentStrategy(); // This ensures it's calculated if needed
    std::vector<double> current_strategy(num_actions * acting_player_num_hands);
    if (current_strategy_float.size() == current_strategy.size()) {
        std::copy(current_strategy_float.begin(), current_strategy_float.end(), current_strategy.begin());
    } else {
         double uniform_prob = (num_actions > 0) ? 1.0 / static_cast<double>(num_actions) : 0.0;
         current_strategy.assign(current_strategy.size(), uniform_prob);
         std::cerr << "[WARN] Strategy size mismatch in ActionNode, using uniform." << std::endl;
    }

    // Calculate Child Utilities (from traverser's perspective)
    std::vector<std::vector<double>> child_utilities(num_actions, std::vector<double>(traverser_num_hands, 0.0));
    for (size_t a = 0; a < num_actions; ++a) {
        std::vector<std::vector<double>> next_reach_probs = reach_probs;
        for (size_t h = 0; h < acting_player_num_hands; ++h) {
            size_t strat_idx = a * acting_player_num_hands + h;
            next_reach_probs[acting_player][h] *= current_strategy[strat_idx];
        }
        child_utilities[a] = cfr_utility(children[a], next_reach_probs, traverser, iteration, current_board_mask, chance_reach);
    }

    // Calculate Node Utility (from traverser's perspective)
    if (acting_player == traverser) {
        for (size_t h = 0; h < traverser_num_hands; ++h) {
            node_utility[h] = 0.0;
            for (size_t a = 0; a < num_actions; ++a) {
                size_t strat_idx = a * acting_player_num_hands + h;
                node_utility[h] += current_strategy[strat_idx] * child_utilities[a][h];
            }
        }
    } else {
        for (size_t h = 0; h < traverser_num_hands; ++h) {
            node_utility[h] = 0.0;
            for (size_t a = 0; a < num_actions; ++a) {
                node_utility[h] += child_utilities[a][h];
            }
        }
    }

    // Update Regrets and Average Strategy only if traverser is acting
    if (acting_player == traverser) {
        std::vector<float> regrets_float(num_actions * acting_player_num_hands);
        // Calculate SCALAR counterfactual weights
        double opponent_reach_sum = std::accumulate(reach_probs[opponent_player].begin(), reach_probs[opponent_player].end(), 0.0);
        double player_reach_sum   = std::accumulate(reach_probs[acting_player].begin(), reach_probs[acting_player].end(), 0.0); // acting_player == traverser

        double w_regret = opponent_reach_sum * chance_reach;
        double w_avg    = player_reach_sum   * chance_reach;

        // Calculate regrets (per-hand)
        for (size_t h = 0; h < acting_player_num_hands; ++h) {
             for (size_t a = 0; a < num_actions; ++a) {
                 size_t idx = a * acting_player_num_hands + h;
                 regrets_float[idx] = static_cast<float>(child_utilities[a][h] - node_utility[h]);
             }
        }

        // Pass the SCALAR weights to the trainable methods
        trainable->UpdateRegrets(regrets_float, iteration, w_regret);

        // Pass current strategy and SCALAR weight for average strategy update
        std::vector<float> current_strategy_for_update(current_strategy.begin(), current_strategy.end());
        trainable->AccumulateAverageStrategy(current_strategy_for_update, iteration, w_avg);
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
    else throw std::logic_error("Invalid round after ChanceNode.");

    // TODO: Improve blocker model here if needed
    uint64_t unavailable_mask = current_board_mask;
    std::vector<int> available_card_indices;
    const auto& all_deck_cards = deck_.GetCards();
    available_card_indices.reserve(core::kNumCardsInDeck);
    for(int i=0; i < core::kNumCardsInDeck; ++i) {
        if (!core::Card::DoBoardsOverlap(1ULL << i, unavailable_mask)) {
            available_card_indices.push_back(i);
        }
    }

    if (static_cast<int>(available_card_indices.size()) < num_cards_to_deal) {
        std::cerr << "[WARN] Not enough cards left (" << available_card_indices.size()
                  << ") for " << core::GameTreeNode::GameRoundToString(round_after_chance) << std::endl;
        return std::vector<double>(pcm_->GetPlayerRange(traverser).size(), 0.0);
    }

    utils::Combinations<int> combinations(available_card_indices, num_cards_to_deal);
    const auto& outcomes = combinations.GetCombinations();
    double num_possible_outcomes = static_cast<double>(outcomes.size());

    if (num_possible_outcomes < 1.0) {
         std::cerr << "[WARN] No valid card combinations found for chance node." << std::endl;
        return std::vector<double>(pcm_->GetPlayerRange(traverser).size(), 0.0);
    }

    double outcome_probability = 1.0 / num_possible_outcomes;
    double next_node_chance_reach = parent_chance_reach * outcome_probability;

    size_t traverser_num_hands = pcm_->GetPlayerRange(traverser).size();
    std::vector<double> total_expected_utility(traverser_num_hands, 0.0);

    for (const auto& outcome_cards : outcomes) {
        uint64_t outcome_board_mask = core::Card::CardIntsToUint64(outcome_cards);
        uint64_t next_board_mask = current_board_mask | outcome_board_mask;

        std::vector<std::vector<double>> next_reach_probs = reach_probs;
        bool traverser_can_reach = false;
        bool opponent_can_reach = false;
        int opponent_player = 1 - traverser;

        // Filter reach probs based on *new* blockers
        for (size_t p = 0; p < num_players_; ++p) {
             const auto& range = pcm_->GetPlayerRange(p);
             double next_reach_sum = 0.0;
             for (size_t h = 0; h < range.size(); ++h) {
                  if (core::Card::DoBoardsOverlap(range[h].GetBoardMask(), outcome_board_mask)) {
                      next_reach_probs[p][h] = 0.0;
                  }
                  next_reach_sum += next_reach_probs[p][h];
             }
              if (p == traverser && next_reach_sum > 1e-12) traverser_can_reach = true;
              if (p == opponent_player && next_reach_sum > 1e-12) opponent_can_reach = true;
        }

        if (traverser_can_reach || opponent_can_reach) {
             std::vector<double> child_utility = cfr_utility(child, next_reach_probs, traverser, iteration, next_board_mask, next_node_chance_reach);

            // Accumulate utility directly (weighting is implicit in reach probs)
             if (total_expected_utility.size() == child_utility.size()) {
                 for (size_t i = 0; i < total_expected_utility.size(); ++i) {
                     total_expected_utility[i] += child_utility[i]; // FIX 2: Removed outcome_probability multiplier
                 }
             } else if (!child_utility.empty()){
                  std::cerr << "[ERROR] Utility vector size mismatch from chance node child." << std::endl;
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
    int opponent_player = 1 - traverser;
    const auto& traverser_range = pcm_->GetPlayerRange(traverser);
    const auto& opponent_range = pcm_->GetPlayerRange(opponent_player);
    size_t traverser_hands = traverser_range.size();

    std::vector<double> traverser_utility(traverser_hands, 0.0);

    const auto& traverser_combos = rrm_->GetRiverCombos(traverser, traverser_range, final_board_mask);
    const auto& opponent_combos = rrm_->GetRiverCombos(opponent_player, opponent_range, final_board_mask);

    const auto& p0_wins_payoffs = node->GetPayoffs(core::ComparisonResult::kPlayer1Wins);
    const auto& p1_wins_payoffs = node->GetPayoffs(core::ComparisonResult::kPlayer2Wins);
    const auto& tie_payoffs     = node->GetPayoffs(core::ComparisonResult::kTie);

    for (const auto& trav_c : traverser_combos) {
        size_t trav_orig_idx = trav_c.original_range_index;
        if (trav_orig_idx >= traverser_utility.size()) continue;

        for (const auto& oppo_c : opponent_combos) {
            size_t oppo_orig_idx = oppo_c.original_range_index;
            if (oppo_orig_idx >= reach_probs[opponent_player].size()) continue;

            double payoff_for_traverser = 0.0;
            // *** VERIFY RANK DIRECTION HERE *** Assuming lower rank is better
            if (trav_c.rank < oppo_c.rank) { // Traverser wins
                payoff_for_traverser = (traverser == 0) ? p0_wins_payoffs[0] : p1_wins_payoffs[1];
            } else if (oppo_c.rank < trav_c.rank) { // Opponent wins
                payoff_for_traverser = (traverser == 0) ? p1_wins_payoffs[0] : p0_wins_payoffs[1];
            } else { // Tie
                payoff_for_traverser = tie_payoffs[traverser];
            }
            // If HIGHER rank number is BETTER, swap the '<' signs.

            traverser_utility[trav_orig_idx] += reach_probs[opponent_player][oppo_orig_idx] * payoff_for_traverser;
        }
    }
    for (double & util : traverser_utility) {
        util *= chance_reach; // Apply chance reach probability
    }
    return traverser_utility;
}


// --- Terminal Node Helper ---
std::vector<double> PCfrSolver::cfr_terminal_node(
    std::shared_ptr<nodes::TerminalNode> node,
    const std::vector<std::vector<double>>& reach_probs,
    int traverser,
    double chance_reach)
{
    const auto& payoffs = node->GetPayoffs();
    int opponent_player = 1 - traverser;
    double payoff_for_traverser = payoffs[traverser];

    double opponent_total_reach = 0.0;
     if (opponent_player >= 0 && opponent_player < static_cast<int>(reach_probs.size())) {
        opponent_total_reach = std::accumulate(reach_probs[opponent_player].begin(),
                                                reach_probs[opponent_player].end(), 0.0);
     }

    size_t traverser_hands = pcm_->GetPlayerRange(traverser).size();
    std::vector<double> node_utility(traverser_hands);
    double final_weight = opponent_total_reach * chance_reach;

    for(size_t h = 0; h < traverser_hands; ++h) {
        node_utility[h] = payoff_for_traverser * final_weight;
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
        return nullptr; // Return null for pruned nodes
    }

    result["round"] = core::GameTreeNode::GameRoundToString(node->GetRound());
    result["pot"] = node->GetPot();
    result["depth"] = current_depth;

    if (auto action_node = std::dynamic_pointer_cast<const nodes::ActionNode>(node)) {
        result["node_type"] = "Action";
        result["player"] = action_node->GetPlayerIndex();
        auto trainable = action_node->GetTrainableIfExists(0);
        if (trainable) {
            // DumpStrategy should use average strategy by default
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
        // Only add children object if it's not empty
        if (!children_json.empty()) result["children"] = children_json;

    } else if (auto chance_node = std::dynamic_pointer_cast<const nodes::ChanceNode>(node)) {
        result["node_type"] = "Chance";
        json dealt_cards_json = json::array();
        for(const auto& card : chance_node->GetDealtCards()) {
            dealt_cards_json.push_back(card.ToString());
        }
        result["dealt_cards"] = dealt_cards_json;
        result["donk_opportunity"] = chance_node->IsDonkOpportunity();
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

    // Check if the node became effectively empty after processing children (due to max_depth)
    bool is_effectively_empty = true;
    for(const auto& item : result.items()) {
        if (item.key() != "round" && item.key() != "pot" && item.key() != "depth" && item.key() != "node_type") {
            is_effectively_empty = false;
            break;
        }
    }
    if (is_effectively_empty && result["node_type"] != "Terminal" && result["node_type"] != "Showdown") {
         return nullptr; // Return null if only basic info remains and not intrinsically terminal
    }


    return result;
}

} // namespace solver
} // namespace poker_solver