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
             // Make sure ChanceNode has its child set before adding to stack
             auto chance_child = chance_node->GetChild();
             if (chance_child) {
                 node_stack.push_back(chance_child);
             }
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
        const auto& reach_doubles = pcm_->GetInitialReachProbs(p); // Already double
        initial_reach_probs[p] = reach_doubles; // Assign directly
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
                       << " completed. Time elapsed: " << std::fixed << std::setprecision(2) << elapsed_sec << "s." << std::endl;
             // TODO: Add exploitability calculation here if desired later.
        }
    }

     uint64_t end_time = utils::TimeSinceEpochMillisec();
     double total_sec = static_cast<double>(end_time - start_time) / 1000.0;
     int actual_iterations = stop_signal_ ? (config_.iteration_limit > 0 ? std::max(1, std::min(config_.iteration_limit, static_cast<int>(end_time - start_time) + 1)) : 0) : config_.iteration_limit; // Approximate if stopped early
     std::cout << "[INFO] Training finished after " << (stop_signal_ ? "early stop near iteration " + std::to_string(actual_iterations) : std::to_string(actual_iterations))
               << " iterations. Total time: " << std::fixed << std::setprecision(2) << total_sec << "s." << std::endl;
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
    // Check if traverser index is valid before accessing reach_probs
    if (traverser < 0 || traverser >= static_cast<int>(reach_probs.size())) {
         throw std::out_of_range("Invalid traverser index in cfr_utility.");
    }
    if (!is_terminal && std::accumulate(reach_probs[traverser].begin(), reach_probs[traverser].end(), 0.0) < 1e-12) {
         // Return vector of zeros with the correct size for the traverser
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

    const auto* player_range_ptr = node->GetPlayerRangeRaw(); // Use the corrected getter
    if (!player_range_ptr) throw std::runtime_error("Player range not set on ActionNode.");
    size_t acting_player_num_hands = player_range_ptr->size();

    // Handle case where node has no actions or player has no hands
    if (num_actions == 0 || acting_player_num_hands == 0) {
        return node_utility; // Return vector of zeros
    }

    auto trainable = node->GetTrainable(0); // Assume perfect recall
    if (!trainable) throw std::runtime_error("Failed to get Trainable object.");

    // --- FIX for Point 2: Use local strategy copy if needed ---
    std::vector<double> current_strategy_local; // Local vector
    const std::vector<double>& trainable_strategy = trainable->GetCurrentStrategy(); // Get const ref

    if (trainable_strategy.size() == (num_actions * acting_player_num_hands)) {
        current_strategy_local = trainable_strategy; // Copy if size is correct
    } else {
        // Handle potential size mismatch if Trainable returned wrong size
        std::cerr << "[ERROR] Strategy size mismatch (" << trainable_strategy.size()
                  << ") from GetCurrentStrategy() in ActionNode (expected "
                  << (num_actions * acting_player_num_hands) << "). Using uniform." << std::endl;
        double uniform_prob = (num_actions > 0) ? 1.0 / static_cast<double>(num_actions) : 0.0;
        current_strategy_local.assign(num_actions * acting_player_num_hands, uniform_prob); // Assign to local
    }
    // --- End FIX for Point 2 ---


    // Calculate Child Utilities (from traverser's perspective)
    std::vector<std::vector<double>> child_utilities(num_actions, std::vector<double>(traverser_num_hands, 0.0));
    for (size_t a = 0; a < num_actions; ++a) {
        // Create next_reach_probs for this action branch
        std::vector<std::vector<double>> next_reach_probs = reach_probs; // Copy current reach probs
        // Update the acting player's reach probabilities based on the strategy
        for (size_t h = 0; h < acting_player_num_hands; ++h) {
            size_t strat_idx = a * acting_player_num_hands + h;
            // Ensure strat_idx is within bounds of the LOCAL strategy copy
            if (strat_idx < current_strategy_local.size() && h < next_reach_probs[acting_player].size()) {
                 // *** Use current_strategy_local here ***
                 next_reach_probs[acting_player][h] *= current_strategy_local[strat_idx];
            } else {
                 // This case should ideally not happen if sizes match earlier
                 std::cerr << "[ERROR] Index out of bounds during reach prob update. strat_idx=" << strat_idx
                           << ", current_strategy_local.size()=" << current_strategy_local.size()
                           << ", h=" << h << ", next_reach_probs[" << acting_player << "].size()=" << next_reach_probs[acting_player].size()
                           << std::endl;
                 // Set reach to 0 to prevent further issues, though this indicates a problem
                 if (h < next_reach_probs[acting_player].size()) {
                    next_reach_probs[acting_player][h] = 0.0;
                 }
            }
        }
        // Recursively call cfr_utility for the child corresponding to action 'a'
        if (a < children.size() && children[a]) { // Add check for valid child
           child_utilities[a] = cfr_utility(children[a], next_reach_probs, traverser, iteration, current_board_mask, chance_reach);
        } else {
             std::cerr << "[ERROR] Missing or null child node for action index " << a << std::endl;
             // Assign zeros or handle error appropriately
             child_utilities[a].assign(traverser_num_hands, 0.0);
        }
    }

    // Calculate Node Utility (expected utility from traverser's perspective)
    if (acting_player == traverser) {
        // If traverser is acting, utility is weighted average over actions
        for (size_t h = 0; h < traverser_num_hands; ++h) {
            node_utility[h] = 0.0;
            for (size_t a = 0; a < num_actions; ++a) {
                size_t strat_idx = a * acting_player_num_hands + h;
                 // *** Use current_strategy_local here ***
                 if (strat_idx < current_strategy_local.size() && a < child_utilities.size() && h < child_utilities[a].size()) {
                     node_utility[h] += current_strategy_local[strat_idx] * child_utilities[a][h];
                 } else {
                     // Handle index out of bounds if sizes mismatch
                 }
            }
        }
    } else {
        // If opponent is acting, utility is the sum over actions (already weighted by opponent strategy in child calls)
        for (size_t h = 0; h < traverser_num_hands; ++h) {
            node_utility[h] = 0.0;
            for (size_t a = 0; a < num_actions; ++a) {
                 if (a < child_utilities.size() && h < child_utilities[a].size()) {
                     node_utility[h] += child_utilities[a][h];
                 }
            }
        }
    }

    // Update Regrets and Average Strategy only if traverser is acting
    if (acting_player == traverser) {
        // Calculate immediate counterfactual regrets (weighted by opponent reach)
        std::vector<double> weighted_regrets(num_actions * acting_player_num_hands);
        double opponent_reach_sum = std::accumulate(reach_probs[opponent_player].begin(), reach_probs[opponent_player].end(), 0.0);
        double scalar_weight_for_regret_update = opponent_reach_sum * chance_reach; // Scalar weight used in DCFR update

        // Calculate per-hand weights for average strategy update
        std::vector<double> player_reach_weights_vec(acting_player_num_hands);

        for (size_t h = 0; h < acting_player_num_hands; ++h) {
            // Calculate the per-hand weight for average strategy accumulation
            if (h < reach_probs[acting_player].size()) { // Bounds check
                player_reach_weights_vec[h] = reach_probs[acting_player][h] * chance_reach;
            } else {
                player_reach_weights_vec[h] = 0.0; // Should not happen if sizes match
            }


            // Calculate weighted regrets for Trainable->UpdateRegrets
            for (size_t a = 0; a < num_actions; ++a) {
                 size_t idx = a * acting_player_num_hands + h;
                 if (idx < weighted_regrets.size() && a < child_utilities.size() && h < child_utilities[a].size() && h < node_utility.size()) {
                     weighted_regrets[idx] = child_utilities[a][h] - node_utility[h];
                     // NOTE: weighted_regrets contains pi_{-i}(h)pi_c(h)[v(ha) - v(h)]
                     // The Trainable::UpdateRegrets MUST NOT multiply by the scalar weight again.
                 } else {
                      // Handle index out of bounds
                      if (idx < weighted_regrets.size()) {
                          weighted_regrets[idx] = 0.0;
                      }
                 }
            }
        }

        // Pass the weighted regrets and the *scalar* weight (for DCFR discounting)
        trainable->UpdateRegrets(weighted_regrets, iteration, scalar_weight_for_regret_update);

        // Pass current strategy (local copy) and the *vector* of per-hand weights for average strategy
        // *** Use current_strategy_local here ***
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
    else throw std::logic_error("Invalid round after ChanceNode.");

    // Determine unavailable cards: board + player hands from reach_probs
    uint64_t unavailable_mask = current_board_mask;
    for(size_t p=0; p < num_players_; ++p) {
        const auto& range = pcm_->GetPlayerRange(p);
        for(size_t h=0; h < range.size(); ++h) {
            // Only include blockers from hands that can actually reach this point
            if (reach_probs[p][h] > 1e-12) {
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
        // This can happen if ranges block all possible remaining cards
        // std::cerr << "[WARN] Not enough cards left (" << available_card_indices.size()
        //           << ") for " << core::GameTreeNode::GameRoundToString(round_after_chance)
        //           << " (Board: 0x" << std::hex << current_board_mask << std::dec << ")" << std::endl;
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
    // The chance_reach passed to the recursive call should incorporate the outcome probability
    double next_node_chance_reach = parent_chance_reach * outcome_probability;

    size_t traverser_num_hands = pcm_->GetPlayerRange(traverser).size();
    std::vector<double> total_expected_utility(traverser_num_hands, 0.0);

    for (const auto& outcome_cards : outcomes) {
        uint64_t outcome_board_mask = core::Card::CardIntsToUint64(outcome_cards);
        uint64_t next_board_mask = current_board_mask | outcome_board_mask;

        std::vector<std::vector<double>> next_reach_probs = reach_probs;
        bool traverser_can_reach_this_outcome = false;
        bool opponent_can_reach_this_outcome = false;
        int opponent_player = 1 - traverser;

        // Filter reach probs based on the *newly dealt cards* blocking existing hands
        for (size_t p = 0; p < num_players_; ++p) {
             const auto& range = pcm_->GetPlayerRange(p);
             double current_reach_sum_for_outcome = 0.0; // Sum for THIS outcome only
             for (size_t h = 0; h < range.size(); ++h) {
                  // Check if player's hand 'h' is blocked by the newly dealt cards
                  if (core::Card::DoBoardsOverlap(range[h].GetBoardMask(), outcome_board_mask)) {
                      next_reach_probs[p][h] = 0.0; // This hand is impossible with this chance outcome
                  }
                  // Accumulate the sum of reach probs compatible with this outcome
                  current_reach_sum_for_outcome += next_reach_probs[p][h];
             }
             // Update flags based on whether any probability remains for this specific outcome
             if (p == traverser && current_reach_sum_for_outcome > 1e-12) traverser_can_reach_this_outcome = true;
             if (p == opponent_player && current_reach_sum_for_outcome > 1e-12) opponent_can_reach_this_outcome = true;
        }

        // Only recurse if at least one player can reach this state with this chance outcome
        if (traverser_can_reach_this_outcome || opponent_can_reach_this_outcome) {
             // Pass the reach probabilities *after* filtering for this specific outcome
             std::vector<double> child_utility = cfr_utility(child, next_reach_probs, traverser, iteration, next_board_mask, next_node_chance_reach);

             // Accumulate utility. The child_utility is already weighted by pi_c(outcome)*pi_{-i}(h'),
             // so we just sum them up. The different reach_probs passed down handle the weighting implicitly.
             if (total_expected_utility.size() == child_utility.size()) {
                 for (size_t i = 0; i < total_expected_utility.size(); ++i) {
                     total_expected_utility[i] += child_utility[i];
                 }
             } else if (!child_utility.empty()){
                  std::cerr << "[ERROR] Utility vector size mismatch from chance node child." << std::endl;
                  // Consider how to handle this error - maybe return zeros or throw?
                  // Returning zeros might hide issues. Throwing might be better.
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
    int opponent_player = 1 - traverser;
    const auto& traverser_range = pcm_->GetPlayerRange(traverser);
    const auto& opponent_range = pcm_->GetPlayerRange(opponent_player);
    size_t traverser_hands = traverser_range.size();
    size_t opponent_hands = opponent_range.size(); // Store opponent hand count

    std::vector<double> traverser_utility(traverser_hands, 0.0);

    // Check if opponent range/reach_probs are valid before accessing
    if (opponent_player < 0 || opponent_player >= static_cast<int>(reach_probs.size()) || opponent_hands != reach_probs[opponent_player].size()) {
        std::cerr << "[ERROR] Opponent range/reach_probs mismatch in cfr_showdown_node." << std::endl;
        return traverser_utility; // Return zeros
    }

    // Pre-fetch river combos to avoid repeated lookups inside the loop
    const auto& traverser_combos = rrm_->GetRiverCombos(traverser, traverser_range, final_board_mask);
    const auto& opponent_combos = rrm_->GetRiverCombos(opponent_player, opponent_range, final_board_mask);

    const auto& p0_wins_payoffs = node->GetPayoffs(core::ComparisonResult::kPlayer1Wins);
    const auto& p1_wins_payoffs = node->GetPayoffs(core::ComparisonResult::kPlayer2Wins);
    const auto& tie_payoffs     = node->GetPayoffs(core::ComparisonResult::kTie);

    // --- Build a map from original opponent index to their RiverCombs index ---
    // This avoids searching the opponent_combos vector repeatedly.
    std::unordered_map<size_t, size_t> oppo_orig_idx_to_river_idx;
    for(size_t rc_idx = 0; rc_idx < opponent_combos.size(); ++rc_idx) {
        oppo_orig_idx_to_river_idx[opponent_combos[rc_idx].original_range_index] = rc_idx;
    }
    // -------------------------------------------------------------------------


    for (const auto& trav_c : traverser_combos) {
        size_t trav_orig_idx = trav_c.original_range_index;
        // Ensure index is valid for the utility vector and reach probs
        if (trav_orig_idx >= traverser_utility.size() || trav_orig_idx >= reach_probs[traverser].size()) continue;

        // Skip if traverser couldn't reach with this hand
        if (reach_probs[traverser][trav_orig_idx] < 1e-12) continue;

        uint64_t traverser_private_mask = trav_c.private_cards.GetBoardMask();
        double expected_value_for_hand = 0.0;

        for (size_t oppo_orig_idx = 0; oppo_orig_idx < opponent_hands; ++oppo_orig_idx) {
            // Skip if opponent couldn't reach with this hand
            if (reach_probs[opponent_player][oppo_orig_idx] < 1e-12) continue;

            const core::PrivateCards& oppo_hand_orig = opponent_range[oppo_orig_idx];
            uint64_t opponent_private_mask = oppo_hand_orig.GetBoardMask();

            // Check for blocker conflicts between the specific hands
            if (core::Card::DoBoardsOverlap(traverser_private_mask, opponent_private_mask)) {
                continue; // This opponent hand is impossible given traverser's hand
            }

            // Find the corresponding RiverComb for the opponent using the map
            auto map_it = oppo_orig_idx_to_river_idx.find(oppo_orig_idx);
            if (map_it == oppo_orig_idx_to_river_idx.end()) {
                 // This happens if the opponent's hand was filtered by the board
                 // in GetRiverCombos but still has non-zero reach probability here.
                 // This indicates an inconsistency but we should skip this opponent hand.
                continue;
            }
            const auto& oppo_c = opponent_combos[map_it->second];


            double payoff_for_traverser = 0.0;
            // Compare ranks (assuming lower is better)
            if (trav_c.rank < oppo_c.rank) { // Traverser wins
                payoff_for_traverser = (traverser == 0) ? p0_wins_payoffs[0] : p1_wins_payoffs[1];
            } else if (oppo_c.rank < trav_c.rank) { // Opponent wins
                payoff_for_traverser = (traverser == 0) ? p1_wins_payoffs[0] : p0_wins_payoffs[1];
            } else { // Tie
                payoff_for_traverser = tie_payoffs[traverser];
            }

            expected_value_for_hand += reach_probs[opponent_player][oppo_orig_idx] * payoff_for_traverser;
        }
         // Store the final expected value for the traverser's hand, weighted by chance
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
    const auto& payoffs = node->GetPayoffs(); // Payoffs for [P0, P1]
    const double payoff_for_traverser = payoffs[traverser];

    int opponent_player = 1 - traverser;
    const auto& traverser_range = pcm_->GetPlayerRange(traverser);
    const auto& opponent_range = pcm_->GetPlayerRange(opponent_player);
    size_t traverser_hands = traverser_range.size();
    size_t opponent_hands = opponent_range.size(); // Use size for loop bounds

    std::vector<double> node_utility(traverser_hands, 0.0); // Initialize utility vector

    // Check if opponent range/reach_probs are valid before accessing
    if (opponent_player < 0 || opponent_player >= static_cast<int>(reach_probs.size()) || opponent_hands != reach_probs[opponent_player].size()) {
         // Handle error: opponent index out of bounds or range size mismatch
         std::cerr << "[ERROR] Opponent range/reach_probs mismatch in cfr_terminal_node." << std::endl;
         // Returning zeros might be the safest fallback if you don't throw
         return node_utility;
    }


    // Iterate through each hand the traverser could hold
    for (size_t h_i = 0; h_i < traverser_hands; ++h_i) {
        // Skip calculation if traverser couldn't reach with this hand
        if (reach_probs[traverser][h_i] < 1e-12) {
            continue;
        }

        uint64_t traverser_mask = traverser_range[h_i].GetBoardMask();
        double compatible_opponent_reach_sum = 0.0;

        // Iterate through opponent's hands to calculate the compatible reach sum
        for (size_t h_j = 0; h_j < opponent_hands; ++h_j) {
            uint64_t opponent_mask = opponent_range[h_j].GetBoardMask();

            // Check for card removal conflict between the specific hands
            if (!core::Card::DoBoardsOverlap(traverser_mask, opponent_mask)) {
                // If no conflict, add the opponent's reach probability for this hand
                compatible_opponent_reach_sum += reach_probs[opponent_player][h_j];
            }
        }

        // Calculate the utility for this specific traverser hand
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
        return nullptr; // Return null for pruned nodes
    }

    result["round"] = core::GameTreeNode::GameRoundToString(node->GetRound());
    result["pot"] = node->GetPot();
    result["depth"] = current_depth;

    if (auto action_node = std::dynamic_pointer_cast<const nodes::ActionNode>(node)) {
        result["node_type"] = "Action";
        result["player"] = action_node->GetPlayerIndex();
        auto trainable = action_node->GetTrainableIfExists(0); // Still assuming perfect recall for dump
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
                     // Only add child if it's not null (not pruned)
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
            // Check if card is valid before trying to stringify
            if (!card.IsEmpty()) {
                dealt_cards_json.push_back(card.ToString());
            } else {
                 dealt_cards_json.push_back("InvalidCard"); // Placeholder for invalid cards
            }
        }
        result["dealt_cards"] = dealt_cards_json;
        // Removed donk opportunity check as it wasn't present in ChanceNode definition
        // result["donk_opportunity"] = chance_node->IsDonkOpportunity();
        if (chance_node->GetChild()) {
            json child_dump = dump_strategy_recursive(chance_node->GetChild(), dump_evs, current_depth + 1, max_depth);
            if (!child_dump.is_null()) result["child"] = child_dump;
        }

    } else if (auto showdown_node = std::dynamic_pointer_cast<const nodes::ShowdownNode>(node)) {
        result["node_type"] = "Showdown";
        // Optionally add pre-calculated payoffs if useful
        // result["payoffs_p0_wins"] = showdown_node->GetPayoffs(core::ComparisonResult::kPlayer1Wins);
        // result["payoffs_p1_wins"] = showdown_node->GetPayoffs(core::ComparisonResult::kPlayer2Wins);
        // result["payoffs_tie"]     = showdown_node->GetPayoffs(core::ComparisonResult::kTie);


    } else if (auto terminal_node = std::dynamic_pointer_cast<const nodes::TerminalNode>(node)) {
        result["node_type"] = "Terminal";
        result["payoffs"] = terminal_node->GetPayoffs();

    } else {
        result["node_type"] = "Unknown";
    }

    // Check if the node became effectively empty after processing children (due to max_depth)
    bool is_effectively_empty = true;
    if (result.contains("children") && !result["children"].empty()) is_effectively_empty = false;
    if (result.contains("child") && !result["child"].is_null()) is_effectively_empty = false;
    if (result["node_type"] == "Terminal" || result["node_type"] == "Showdown") is_effectively_empty = false;
    // Maybe add check for strategy_data presence for ActionNodes?
    if (result["node_type"] == "Action" && result.contains("strategy_data") && result["strategy_data"] != "Not trained") is_effectively_empty = false;


    if (is_effectively_empty) {
         return nullptr; // Return null if only basic info remains and not intrinsically terminal/showdown/trained action
    }


    return result;
}

} // namespace solver
} // namespace poker_solver