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
#include <omp.h>

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
    const auto& board_ints_from_rule = rule.GetInitialBoardCardsInt();
    std::cout << "[DEBUG_SOLVER_CONSTRUCTOR] Board ints from Rule object: ";
    for(int c_int : board_ints_from_rule) std::cout << c_int << " (" << core::Card::IntToString(c_int) << ") ";
    std::cout << std::endl;

    initial_board_mask_ = core::Card::CardIntsToUint64(board_ints_from_rule);

    // *** DEBUG PRINT ADDED HERE ***
    std::cout << "[DEBUG_SOLVER_CONSTRUCTOR] Calculated initial_board_mask_: 0x" << std::hex << initial_board_mask_ << std::dec << std::endl;
    std::vector<core::Card> temp_cards_from_mask = core::Card::Uint64ToCards(initial_board_mask_);
    std::cout << "[DEBUG_SOLVER_CONSTRUCTOR] Mask 0x" << std::hex << initial_board_mask_ << std::dec << " represents cards: ";
    for(const auto& card : temp_cards_from_mask) std::cout << card.ToString() << " ";
    std::cout << std::endl;
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

    // --- Set OpenMP Threads ---
    if (config_.num_threads > 0) {
        omp_set_num_threads(config_.num_threads);
        std::cout << "[INFO] OpenMP enabled with " << config_.num_threads << " threads." << std::endl;
    } else {
         omp_set_num_threads(1); // Explicitly set to 1 if config is 0 or less
        std::cout << "[INFO] OpenMP running with 1 thread (parallelism disabled by config)." << std::endl;
    }

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
    // --- Get necessary info from the node ---
    std::shared_ptr<core::GameTreeNode> child = node->GetChild();
    if (!child) throw std::logic_error("Chance node has no child.");

    core::GameRound round_after_chance = node->GetRound();
    int num_cards_to_deal = 0;
    if (round_after_chance == core::GameRound::kFlop) num_cards_to_deal = 3;
    else if (round_after_chance == core::GameRound::kTurn) num_cards_to_deal = 1;
    else if (round_after_chance == core::GameRound::kRiver) num_cards_to_deal = 1;
    else {
        std::cerr << "[ERROR] cfr_chance_node: Invalid round after chance: " << core::GameTreeNode::GameRoundToString(round_after_chance) << std::endl;
        throw std::logic_error("Invalid round after ChanceNode in cfr_chance_node.");
    }

    // --- Determine unavailable cards (board + player hands) ---
    uint64_t unavailable_mask = current_board_mask; // Start with existing board
    for(size_t p=0; p < num_players_; ++p) {
        if (p >= reach_probs.size()) continue; // Safety check
        const auto& range = pcm_->GetPlayerRange(p); // Read-only access to shared pcm_
        if (range.size() != reach_probs[p].size() && std::abs(static_cast<long>(range.size()) - static_cast<long>(reach_probs[p].size())) > 5) {
            continue;
        }
        for(size_t h=0; h < std::min(range.size(), reach_probs[p].size()); ++h) {
            if (reach_probs[p][h] > 1e-12) {
                unavailable_mask |= range[h].GetBoardMask();
            }
        }
    }

    // --- Determine available cards for dealing ---
    std::vector<int> available_card_indices;
    const auto& all_deck_cards = deck_.GetCards(); // Read-only access to shared deck_
    available_card_indices.reserve(core::kNumCardsInDeck);
    for(int i=0; i < core::kNumCardsInDeck; ++i) {
        if (!core::Card::DoBoardsOverlap(1ULL << i, unavailable_mask)) {
            available_card_indices.push_back(i);
        }
    }

    // --- Check if enough cards are available ---
    if (static_cast<int>(available_card_indices.size()) < num_cards_to_deal) {
        return std::vector<double>(pcm_->GetPlayerRange(traverser).size(), 0.0);
    }

    // --- Generate card combinations (outcomes) ---
    utils::Combinations<int> combinations(available_card_indices, num_cards_to_deal);
    const auto& outcomes = combinations.GetCombinations();
    double num_possible_outcomes = static_cast<double>(outcomes.size());

    if (num_possible_outcomes < 1.0) {
        return std::vector<double>(pcm_->GetPlayerRange(traverser).size(), 0.0);
    }

    // --- Prepare for parallel loop ---
    double outcome_probability = 1.0 / num_possible_outcomes;
    double next_node_chance_reach = parent_chance_reach * outcome_probability;

    size_t traverser_num_hands = pcm_->GetPlayerRange(traverser).size();
    // Initialize the shared result vector *before* the parallel region
    std::vector<double> total_expected_utility(traverser_num_hands, 0.0);

    // --- Parallel Loop over Chance Outcomes ---
    // We removed the reduction clause. We will manually add to the shared
    // total_expected_utility vector inside a critical section.
    #pragma omp parallel for schedule(dynamic)
    for (size_t i = 0; i < outcomes.size(); ++i) {
        const auto& outcome_cards = outcomes[i];

        // --- Thread-Local Variables ---
        uint64_t outcome_board_mask = core::Card::CardIntsToUint64(outcome_cards);
        uint64_t next_board_mask = current_board_mask | outcome_board_mask;
        std::vector<std::vector<double>> next_reach_probs = reach_probs; // Thread-local copy
        bool traverser_can_reach_this_outcome = false;
        bool opponent_can_reach_this_outcome = false;
        int opponent_player = 1 - traverser;

        // --- Update thread-local reach probabilities ---
        for (size_t p = 0; p < num_players_; ++p) {
            if (p >= next_reach_probs.size()) continue;
            const auto& range = pcm_->GetPlayerRange(p);
            double current_reach_sum_for_outcome = 0.0;
            size_t current_range_size = std::min(range.size(), next_reach_probs[p].size());
            for (size_t h = 0; h < current_range_size; ++h) {
                 if (core::Card::DoBoardsOverlap(range[h].GetBoardMask(), outcome_board_mask)) {
                     next_reach_probs[p][h] = 0.0;
                 }
                 current_reach_sum_for_outcome += next_reach_probs[p][h];
            }
            if (p == static_cast<size_t>(traverser) && current_reach_sum_for_outcome > 1e-12) traverser_can_reach_this_outcome = true;
            if (p == static_cast<size_t>(opponent_player) && current_reach_sum_for_outcome > 1e-12) opponent_can_reach_this_outcome = true;
        }

        // --- Recurse if possible ---
        if (traverser_can_reach_this_outcome || opponent_can_reach_this_outcome) {
            // Calculate utility for this specific outcome (thread-local result)
            std::vector<double> child_utility = cfr_utility(child, next_reach_probs, traverser, iteration, next_board_mask, next_node_chance_reach);

            // --- Manual Reduction using Critical Section ---
            // Check size before entering critical section for efficiency
            if (total_expected_utility.size() == child_utility.size()) {
                // This critical section ensures only one thread at a time
                // modifies the shared 'total_expected_utility' vector.
                #pragma omp critical
                {
                    for (size_t util_idx = 0; util_idx < total_expected_utility.size(); ++util_idx) {
                        total_expected_utility[util_idx] += child_utility[util_idx];
                    }
                } // --- End of critical section ---
            } else if (!child_utility.empty()) {
                 // Handle error (still use critical for safe output)
                 #pragma omp critical
                 {
                    std::cerr << "[ERROR] Utility vector size mismatch from chance node child in parallel region. Thread: "
                              << omp_get_thread_num() << ". Expected: " << total_expected_utility.size()
                              << ", Got: " << child_utility.size() << std::endl;
                 }
            }
        }
    } // --- End of parallel loop ---

    // Return the final accumulated utility vector
    return total_expected_utility;
}




// --- Showdown Node Helper ---
std::vector<double> PCfrSolver::cfr_showdown_node(
    std::shared_ptr<nodes::ShowdownNode> node,
    const std::vector<std::vector<double>>& reach_probs,
    int traverser,
    uint64_t final_board_mask,
    double chance_reach)
{
    // --- Basic Setup ---
    int opponent_player = 1 - traverser;
    const auto& traverser_range = pcm_->GetPlayerRange(traverser); // Read-only access
    const auto& opponent_range = pcm_->GetPlayerRange(opponent_player); // Read-only access
    size_t traverser_hands = traverser_range.size();
    size_t opponent_hands = opponent_range.size();

    // Initialize result vector
    std::vector<double> traverser_utility(traverser_hands, 0.0);

    // --- Pre-computation and Checks (Before Parallel Region) ---

    // Check for potential size mismatches or invalid opponent index
    if (opponent_player < 0 || static_cast<size_t>(opponent_player) >= reach_probs.size() || (opponent_hands > 0 && opponent_hands != reach_probs[opponent_player].size())) {
        // std::cerr << "[WARN] cfr_showdown_node: Opponent range/reach_probs mismatch or invalid index. Opponent: " << opponent_player
        //           << ", Opponent Hands: " << opponent_hands
        //           << ", Reach Probs size for opp: " << (static_cast<size_t>(opponent_player) < reach_probs.size() ? reach_probs[opponent_player].size() : -1)
        //           << ". Returning zero utility." << std::endl;
        return traverser_utility; // Cannot proceed safely
    }
    // Check traverser reach_probs size as well
     if (static_cast<size_t>(traverser) >= reach_probs.size() || (traverser_hands > 0 && traverser_hands != reach_probs[traverser].size())) {
        // std::cerr << "[WARN] cfr_showdown_node: Traverser range/reach_probs mismatch or invalid index. Traverser: " << traverser
        //           << ", Traverser Hands: " << traverser_hands
        //           << ", Reach Probs size for trav: " << (static_cast<size_t>(traverser) < reach_probs.size() ? reach_probs[traverser].size() : -1)
        //           << ". Returning zero utility." << std::endl;
         return traverser_utility; // Cannot proceed safely
     }


    // Get river combos (potentially expensive, do it once)
    // Assumes rrm_ is thread-safe for concurrent reads if called inside parallel region,
    // but safer to call it outside.
    const auto& traverser_combos = rrm_->GetRiverCombos(traverser, traverser_range, final_board_mask);
    const auto& opponent_combos = rrm_->GetRiverCombos(opponent_player, opponent_range, final_board_mask);

    // Get payoffs (read-only access is safe)
    const auto& p0_wins_payoffs = node->GetPayoffs(core::ComparisonResult::kPlayer1Wins);
    const auto& p1_wins_payoffs = node->GetPayoffs(core::ComparisonResult::kPlayer2Wins);
    const auto& tie_payoffs     = node->GetPayoffs(core::ComparisonResult::kTie);

    // Create lookup maps *before* the parallel loop for efficiency
    std::unordered_map<size_t, size_t> oppo_orig_idx_to_river_idx;
    oppo_orig_idx_to_river_idx.reserve(opponent_combos.size()); // Optional: reserve space
    for(size_t rc_idx = 0; rc_idx < opponent_combos.size(); ++rc_idx) {
        oppo_orig_idx_to_river_idx[opponent_combos[rc_idx].original_range_index] = rc_idx;
    }

    std::unordered_map<size_t, size_t> trav_orig_idx_to_river_idx;
    trav_orig_idx_to_river_idx.reserve(traverser_combos.size()); // Optional: reserve space
    for(size_t rc_idx = 0; rc_idx < traverser_combos.size(); ++rc_idx) {
        trav_orig_idx_to_river_idx[traverser_combos[rc_idx].original_range_index] = rc_idx;
    }

    // --- Parallel Loop over Traverser's Hands ---
    // Each thread calculates the utility for a subset of the traverser's original hands.
    // Writes to traverser_utility[trav_orig_idx] are safe because each thread handles distinct indices.
    #pragma omp parallel for schedule(dynamic) // schedule(dynamic) might help if opponent comparisons vary
    for (size_t trav_orig_idx = 0; trav_orig_idx < traverser_hands; ++trav_orig_idx) {
        // --- Per-Hand Calculation (Private within thread/iteration) ---

        // Check if traverser can actually reach with this hand
        // (Bounds check already done before parallel region)
        if (reach_probs[traverser][trav_orig_idx] < 1e-12) {
            continue; // Skip this hand if it has zero reach probability
        }

        // Find the corresponding RiverHandCombo for this original index
        // This lookup uses the map created before the parallel region (read-only access)
        auto trav_map_it = trav_orig_idx_to_river_idx.find(trav_orig_idx);
        if (trav_map_it == trav_orig_idx_to_river_idx.end()) {
            // This hand wasn't valid on the river (e.g., blocked by final board), utility remains 0.0
            continue;
        }
        // Get the actual river combo data (read-only access to traverser_combos)
        const auto& trav_c = traverser_combos[trav_map_it->second];

        // Variables local to this iteration are private
        uint64_t traverser_private_mask = trav_c.private_cards.GetBoardMask();
        double expected_value_for_hand = 0.0; // Accumulator for this specific hand

        // Inner loop: Iterate over opponent hands to calculate expected value
        for (size_t oppo_orig_idx = 0; oppo_orig_idx < opponent_hands; ++oppo_orig_idx) {
            // Check opponent reach probability (read-only access to reach_probs)
            if (reach_probs[opponent_player][oppo_orig_idx] < 1e-12) {
                continue; // Skip opponent hand if it has zero reach probability
            }

            // Get opponent's original hand data (read-only access to opponent_range)
            const core::PrivateCards& oppo_hand_orig = opponent_range[oppo_orig_idx];
            uint64_t opponent_private_mask = oppo_hand_orig.GetBoardMask();

            // Check for card conflicts (blockers)
            if (core::Card::DoBoardsOverlap(traverser_private_mask, opponent_private_mask)) {
                continue; // Skip conflicting hands
            }

            // Find the opponent's corresponding RiverHandCombo
            // (read-only access to oppo_orig_idx_to_river_idx map)
            auto oppo_map_it = oppo_orig_idx_to_river_idx.find(oppo_orig_idx);
            if (oppo_map_it == oppo_orig_idx_to_river_idx.end()) {
                // Opponent hand invalid on river (blocked by final board), skip comparison
                continue;
            }
            // Get opponent river combo data (read-only access to opponent_combos)
            const auto& oppo_c = opponent_combos[oppo_map_it->second];

            // Determine payoff based on hand ranks
            double payoff_for_traverser = 0.0;
            if (trav_c.rank < oppo_c.rank) { // Traverser has better rank (lower is better)
                payoff_for_traverser = (traverser == 0) ? p0_wins_payoffs[0] : p1_wins_payoffs[1]; // Payoff for P0 if P0 wins, Payoff for P1 if P1 wins
            } else if (oppo_c.rank < trav_c.rank) { // Opponent has better rank
                payoff_for_traverser = (traverser == 0) ? p1_wins_payoffs[0] : p0_wins_payoffs[1]; // Payoff for P0 if P1 wins, Payoff for P1 if P0 wins
            } else { // Ranks are equal (tie)
                payoff_for_traverser = tie_payoffs[traverser]; // Payoff for the traverser in case of a tie
            }

            // Accumulate expected value, weighted by opponent's reach probability
            expected_value_for_hand += reach_probs[opponent_player][oppo_orig_idx] * payoff_for_traverser;

        } // --- End inner loop (opponent hands) ---

        // Write the final calculated utility for this specific hand to the shared result vector.
        // This is safe because each thread writes to a unique index `trav_orig_idx`.
        traverser_utility[trav_orig_idx] = expected_value_for_hand * chance_reach;

    } // --- End of parallel loop ---

    // Return the completed utility vector
    return traverser_utility;
}


// --- Terminal Node Helper (Corrected) ---
std::vector<double> PCfrSolver::cfr_terminal_node(
    std::shared_ptr<nodes::TerminalNode> node,
    const std::vector<std::vector<double>>& reach_probs,
    int traverser,
    double chance_reach)
{
    // --- Basic Setup & Pre-computation (Before Parallel Region) ---

    // Get payoffs for this terminal state
    const auto& payoffs = node->GetPayoffs(); // Read-only access ok
    if (traverser < 0 || static_cast<size_t>(traverser) >= payoffs.size()) {
         throw std::out_of_range("Traverser index out of bounds for payoffs vector in terminal node.");
    }
    // Get the specific payoff for the player traversing the tree
    const double payoff_for_traverser = payoffs[traverser];

    // Determine opponent and get player ranges (read-only access ok)
    int opponent_player = 1 - traverser;
    const auto& traverser_range = pcm_->GetPlayerRange(traverser);
    const auto& opponent_range = pcm_->GetPlayerRange(opponent_player);
    size_t traverser_hands = traverser_range.size();
    size_t opponent_hands = opponent_range.size();

    // Initialize result vector
    std::vector<double> node_utility(traverser_hands, 0.0);

    // --- Safety Checks (Before Parallel Region) ---
    // Check for potential size mismatches or invalid opponent index
    if (opponent_player < 0 || static_cast<size_t>(opponent_player) >= reach_probs.size() || (opponent_hands > 0 && opponent_hands != reach_probs[opponent_player].size()) ) {
        // std::cerr << "[WARN] cfr_terminal_node: Opponent range/reach_probs mismatch or invalid index. Opponent: " << opponent_player
        //           << ", Opponent Hands: " << opponent_hands
        //           << ", Reach Probs size for opp: " << (static_cast<size_t>(opponent_player) < reach_probs.size() ? reach_probs[opponent_player].size() : -1)
        //           << ". Returning zero utility." << std::endl;
         return node_utility; // Cannot proceed safely
    }
     // Check traverser reach_probs size as well
     if (static_cast<size_t>(traverser) >= reach_probs.size() || (traverser_hands > 0 && traverser_hands != reach_probs[traverser].size())) {
        // std::cerr << "[WARN] cfr_terminal_node: Traverser range/reach_probs mismatch or invalid index. Traverser: " << traverser
        //           << ", Traverser Hands: " << traverser_hands
        //           << ", Reach Probs size for trav: " << (static_cast<size_t>(traverser) < reach_probs.size() ? reach_probs[traverser].size() : -1)
        //           << ". Returning zero utility." << std::endl;
         return node_utility; // Cannot proceed safely
     }


    // --- Parallel Loop over Traverser's Hands ---
    // Each thread calculates the utility for a subset of the traverser's hands.
    // Writes to node_utility[h_i] are safe because each thread handles distinct indices.
    #pragma omp parallel for schedule(dynamic)
    for (size_t h_i = 0; h_i < traverser_hands; ++h_i) {
        // --- Per-Hand Calculation (Private within thread/iteration) ---

        // Check if traverser can actually reach with this hand
        // (Bounds check already done before parallel region)
        if (reach_probs[traverser][h_i] < 1e-12) {
            continue; // Skip this hand if it has zero reach probability
        }

        // Get traverser's hand mask (read-only access to traverser_range)
        uint64_t traverser_mask = traverser_range[h_i].GetBoardMask();
        // Accumulator for the sum of reach probabilities of non-conflicting opponent hands
        double compatible_opponent_reach_sum = 0.0;

        // Inner loop: Iterate over opponent hands
        for (size_t h_j = 0; h_j < opponent_hands; ++h_j) {
            // Get opponent's hand mask (read-only access to opponent_range)
            uint64_t opponent_mask = opponent_range[h_j].GetBoardMask();

            // Check for card conflicts (blockers)
            if (!core::Card::DoBoardsOverlap(traverser_mask, opponent_mask)) {
                // If no conflict, add opponent's reach probability to the sum
                // (Bounds check already done before parallel region)
                compatible_opponent_reach_sum += reach_probs[opponent_player][h_j];
            }
        } // --- End inner loop (opponent hands) ---

        // Calculate and store the utility for this traverser hand.
        // Write is safe as h_i is unique per thread/iteration.
        node_utility[h_i] = payoff_for_traverser * compatible_opponent_reach_sum * chance_reach;

    } // --- End of parallel loop ---

    // Return the completed utility vector
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