#include "GameTree.h" // Adjust path if necessary
#include "nodes/ActionNode.h"
#include "nodes/ChanceNode.h"
#include "nodes/ShowdownNode.h"
#include "nodes/TerminalNode.h"
#include "nodes/GameActions.h"
#include "Card.h"
#include "tools/StreetSetting.h"
#include "trainable/Trainable.h" // Needed for EstimateMemoryRecursive

#include <fstream>   // For std::ifstream
#include <stdexcept> // For exceptions
#include <sstream>   // For error messages
#include <vector>
#include <string>
#include <cmath>     // For std::round, std::max, std::min, std::abs
#include <algorithm> // For std::find, std::sort, std::min, std::max
#include <set>       // For ensuring unique bet sizes
#include <iostream>  // For PrintTree, std::cerr
#include <iomanip>   // For PrintTree formatting
#include <utility>   // For std::move
#include <optional>  // For build_rule_ member
#include <bit>       // For std::popcount (optional)

// Use aliases
using json = nlohmann::json;
namespace core = poker_solver::core;
namespace nodes = poker_solver::nodes;
namespace config = poker_solver::config;
namespace solver = poker_solver::solver;


namespace poker_solver {
namespace tree {

// --- Constructor (JSON Loading) ---

GameTree::GameTree(const std::string& json_filepath, const core::Deck& deck)
    : deck_(deck), build_rule_(std::nullopt) { // Initialize build_rule_ as empty
    std::ifstream json_file(json_filepath);
    if (!json_file.is_open()) {
        throw std::runtime_error("Cannot open game tree JSON file: " + json_filepath);
    }

    json tree_json;
    try {
        json_file >> tree_json;
    } catch (const json::parse_error& e) {
         throw std::runtime_error("Failed to parse game tree JSON file: " + std::string(e.what()));
    }

    if (!tree_json.contains("root")) {
         throw std::invalid_argument("Game tree JSON missing 'root' node.");
    }

    // Recursively parse the tree structure
    root_ = ParseNodeJson(tree_json["root"], std::weak_ptr<core::GameTreeNode>());
    if (!root_) {
        throw std::runtime_error("Failed to parse root node from JSON.");
    }

    // Calculate metadata after loading
    CalculateTreeMetadata();
}

// --- Constructor (Dynamic Building) ---

GameTree::GameTree(const config::Rule& rule)
    : deck_(rule.GetDeck()), build_rule_(rule) { // Copy deck and store rule

    size_t starting_player = 1; // Default to OOP acting first postflop
    if (rule.GetStartingRound() == core::GameRound::kPreflop) {
        std::cerr << "[WARNING] Dynamic tree building starting Preflop assumes OOP acts first." << std::endl;
        starting_player = 1; // TODO: Implement proper preflop start logic
    }

    root_ = std::make_shared<nodes::ActionNode>(
        starting_player,
        rule.GetStartingRound(),
        rule.GetInitialPot(),
        std::weak_ptr<core::GameTreeNode>(), // No parent for root
        1 // Assume perfect recall initially
    );

    // Start the recursive build process
    // Pass the initial rule by value as it will be modified
    BuildBranch(root_, rule, core::GameAction(core::PokerAction::kRoundBegin), 0, 0);

    // Calculate metadata after building
    CalculateTreeMetadata();
}


// --- Dynamic Tree Building Helpers ---

// Pass Rule by value because commitments are modified for child branches
void GameTree::BuildBranch(std::shared_ptr<core::GameTreeNode> current_node,
                           config::Rule current_rule, // Pass Rule by value
                           const core::GameAction& last_action,
                           int actions_this_round,
                           int raises_this_street) {
    if (!current_node) return;

    switch(current_node->GetNodeType()) {
        case core::GameTreeNodeType::kAction: {
            auto action_node = std::dynamic_pointer_cast<nodes::ActionNode>(current_node);
            BuildActionNode(action_node, current_rule, last_action, actions_this_round, raises_this_street);
            break;
        }
        case core::GameTreeNodeType::kChance: {
             auto chance_node = std::dynamic_pointer_cast<nodes::ChanceNode>(current_node);
             if (!build_rule_.has_value()) {
                 // This should not happen if built dynamically via the Rule constructor
                 throw std::logic_error("Build rule not set in GameTree for ChanceNode building.");
             }
            BuildChanceNode(chance_node, current_rule); // Use stored original rule
            break;
        }
        case core::GameTreeNodeType::kShowdown: // Terminal state
        case core::GameTreeNodeType::kTerminal: // Terminal state
            // Do nothing, recursion stops here
            break;
        // default: // Optional: Add default for robustness
        //     throw std::logic_error("Unknown node type encountered during tree build.");
    }
}

// Builds the structure following a ChanceNode
void GameTree::BuildChanceNode(std::shared_ptr<nodes::ChanceNode> node,
                               const config::Rule& rule) { // Takes original build rule
    if (!node) return;

    core::GameRound round_before_chance = node->GetRound();
    double current_pot = node->GetPot();
    double stack = rule.GetInitialEffectiveStack();
    // Get commitments *at the point the chance node was created*
    // We use the original rule's commitments for the all-in check, as the
    // exact commitments depend on the path taken, which isn't tracked here easily.
    // Payoffs in Showdown/Terminal nodes should handle final state correctly.
    double ip_commit = rule.GetInitialCommitment(0);
    double oop_commit = rule.GetInitialCommitment(1);
    constexpr double eps = 1e-9;
    // if you still need the “both-players” check:
    const double ip_remaining  = stack - ip_commit;
    const double oop_remaining = stack - oop_commit;

    const bool ip_all_in  = ip_remaining  <= stack * (1.0 - rule.GetAllInThresholdRatio()) + eps;
    const bool oop_all_in = oop_remaining <= stack * (1.0 - rule.GetAllInThresholdRatio()) + eps;

    const bool effectively_all_in = ip_all_in && oop_all_in;

    std::shared_ptr<core::GameTreeNode> next_node;

    if (effectively_all_in) {
         if (round_before_chance == core::GameRound::kRiver) {
              // Should have ended in Showdown in the parent ActionNode's logic
              std::cerr << "[WARNING] Reached ChanceNode on River when already all-in." << std::endl;
              next_node = std::make_shared<nodes::ShowdownNode>(
                 core::GameRound::kRiver, current_pot, node, 2,
                 std::vector<double>{stack, stack}); // Both committed full stack
         } else {
             // Not river, but all-in -> skip to next chance node until river
             core::GameRound round_after_chance = core::GameTreeNode::IntToGameRound(core::GameTreeNode::GameRoundToInt(round_before_chance) + 1);
              next_node = std::make_shared<nodes::ChanceNode>(
                  round_after_chance, current_pot, node,
                  std::vector<core::Card>{}, nullptr // Dealt cards handled by solver
              );
        }
    } else {
        // Not all-in, next state is an ActionNode for the next player (usually OOP=1)
        core::GameRound round_after_chance = core::GameTreeNode::IntToGameRound(core::GameTreeNode::GameRoundToInt(round_before_chance) + 1);
        if (core::GameTreeNode::GameRoundToInt(round_after_chance) > core::GameTreeNode::GameRoundToInt(core::GameRound::kRiver)) {
             // Finished river betting round, not all-in -> Showdown
             next_node = std::make_shared<nodes::ShowdownNode>(
                 core::GameRound::kRiver, current_pot, node, 2,
                 std::vector<double>{ip_commit, oop_commit}); // Use commitments entering chance node
        } else {
            size_t next_player = 1; // OOP acts first postflop
            next_node = std::make_shared<nodes::ActionNode>(
                next_player, round_after_chance, current_pot, node, 1);
        }
    }

    node->SetChild(next_node);
    // Pass the original build rule down; commitments reset implicitly by starting new round/street
    BuildBranch(next_node, rule, core::GameAction(core::PokerAction::kRoundBegin), 0, 0);
}


// Builds the structure for an ActionNode
// Takes Rule by value as it modifies it for recursive calls
void GameTree::BuildActionNode(std::shared_ptr<nodes::ActionNode> node,
                               config::Rule current_rule_state, // Takes Rule by value
                               const core::GameAction& last_action,
                               int actions_this_round,
                               int raises_this_street) {
    if (!node) return;

    size_t current_player = node->GetPlayerIndex();
    size_t opponent_player = 1 - current_player;
    // Get commitments *at this node* based on the rule state passed down
    double current_player_commit = current_rule_state.GetInitialCommitment(current_player);
    double opponent_commit = current_rule_state.GetInitialCommitment(opponent_player);
    double pot = node->GetPot(); // Pot *before* this action
    double stack = current_rule_state.GetInitialEffectiveStack(); // Use the original stack size
    // Effective stack remaining for the *current* player
    double player_stack_remaining = stack - current_player_commit;
    core::GameRound current_round = node->GetRound();

    if (player_stack_remaining <= 1e-9) {
         std::cerr << "[WARNING] ActionNode created for player already all-in. Commit: "
                   << current_player_commit << ", Stack: " << stack << std::endl;
         node->SetActionsAndChildren({}, {}); // Set empty actions/children
        return;
    }

    std::vector<core::GameAction> possible_node_actions;
    std::vector<std::shared_ptr<core::GameTreeNode>> children_nodes;

    // Determine possible actions based on game state
    constexpr double eps = 1e-9;

    bool can_check = std::abs(current_player_commit - opponent_commit) < eps;
    bool can_call  = (opponent_commit - current_player_commit) > eps;
    
    // AFTER
        double opp_remaining = stack - opponent_commit;
        bool   opponent_is_all_in  =
            opp_remaining <= stack * (1.0 - current_rule_state.GetAllInThresholdRatio()) + 1e-9;
    
    bool can_fold = can_call;
    
    bool can_bet_or_raise =
            !opponent_is_all_in &&
            (player_stack_remaining > current_rule_state.GetBigBlind() - eps) &&
            (raises_this_street < current_rule_state.GetRaiseLimitPerStreet());

    // 1. Check Action
    if (can_check) {
        core::GameAction check_action(core::PokerAction::kCheck);
        possible_node_actions.push_back(check_action);
        std::shared_ptr<core::GameTreeNode> next_node;
        if (actions_this_round > 0) { // Check completes the round
             if (current_round == core::GameRound::kRiver) {
                 next_node = std::make_shared<nodes::ShowdownNode>(
                     current_round, pot, node, 2,
                     std::vector<double>{current_player_commit, opponent_commit});
             } else {
                 core::GameRound next_round = core::GameTreeNode::IntToGameRound(core::GameTreeNode::GameRoundToInt(current_round) + 1);
                 next_node = std::make_shared<nodes::ChanceNode>(
                      next_round, pot, node, std::vector<core::Card>{}, nullptr);
             }
        } else { // First check -> opponent's turn
             next_node = std::make_shared<nodes::ActionNode>(
                 opponent_player, current_round, pot, node, 1);
        }
        children_nodes.push_back(next_node);
        // Pass current rule state (no commit change)
        BuildBranch(next_node, current_rule_state, check_action, actions_this_round + 1, raises_this_street);
    }

    // 2. Call Action
    if (can_call) {
        core::GameAction call_action(core::PokerAction::kCall);
        possible_node_actions.push_back(call_action);
        double call_amount = std::min(opponent_commit - current_player_commit, player_stack_remaining);
        double next_pot = pot + call_amount;
        double next_player_commit = current_player_commit + call_amount;
        std::shared_ptr<core::GameTreeNode> next_node;
        bool now_all_in = (next_player_commit >= stack - 1e-9) || opponent_is_all_in;
        if (current_round == core::GameRound::kRiver || now_all_in) {
            next_node = std::make_shared<nodes::ShowdownNode>(
                core::GameRound::kRiver, next_pot, node, 2,
                (current_player == 0) ? std::vector<double>{next_player_commit, opponent_commit}
                                      : std::vector<double>{opponent_commit, next_player_commit});
        } else {
            core::GameRound next_round = core::GameTreeNode::IntToGameRound(core::GameTreeNode::GameRoundToInt(current_round) + 1);
            next_node = std::make_shared<nodes::ChanceNode>(
                 next_round, next_pot, node, std::vector<core::Card>{}, nullptr);
        }
        children_nodes.push_back(next_node);
        config::Rule next_rule = current_rule_state; // Copy rule
        // Use setters to modify the copy
        if (current_player == 0) next_rule.SetInitialIpCommit(next_player_commit);
        else next_rule.SetInitialOopCommit(next_player_commit);
        BuildBranch(next_node, next_rule, call_action, actions_this_round + 1, raises_this_street);
    }

    // 3. Fold Action
    // --- Fold Action -----------------------------------------------------------
    if (can_fold) {
        const core::GameAction fold_action(core::PokerAction::kFold);
        possible_node_actions.push_back(fold_action);

        std::vector<double> payoffs(2, 0.0);

        // folder loses everything already in the pot
        payoffs[current_player] = -current_player_commit;

        // winner gains exactly what folder just lost
        payoffs[opponent_player] =  current_player_commit;

        auto next_node = std::make_shared<nodes::TerminalNode>(
            payoffs, current_round, pot, node);

        children_nodes.push_back(next_node);
        // no BuildBranch : terminal node ends recursion
    }

    // 4. Bet / Raise Actions
    // 4. Bet / Raise actions
    if (can_bet_or_raise) {
        bool   is_facing_action = opponent_commit > current_player_commit + 1e-9;
        bool   is_raise         = is_facing_action;               // clarity
        auto   action_type      = is_raise ? core::PokerAction::kRaise
                                        : core::PokerAction::kBet;

        // ---- get candidate sizes ------------------------------------------------
        std::vector<double> bet_amounts_to_add = GetPossibleBets(
            current_rule_state,
            current_player,
            /* current_player_commit */ current_player_commit,
            /* opponent_commit       */ opponent_commit,
            /* effective_stack       */ stack,
            /* last action           */ last_action,
            /* pot_before_action     */ node->GetPot(),
            /* street                */ current_round);

        double call_amount = opponent_commit - current_player_commit;

        for (double amount_to_add : bet_amounts_to_add) {
            if (amount_to_add <= 1e-9 ||
                amount_to_add > player_stack_remaining + 1e-9)
                continue;                                           // out of range

            double actual_add   = std::min(amount_to_add, player_stack_remaining);
            double raise_top_up = actual_add - call_amount;         // 0 if bet

            // --- min-bet / min-raise legality checks ----------------------------
            if (!is_raise) {                                        // opening bet
                if (actual_add < current_rule_state.GetBigBlind() - 1e-9 &&
                    actual_add < player_stack_remaining  - 1e-9)
                    continue;                                       // too small
            } else {                                                // raise
                if (raise_top_up < current_rule_state.GetBigBlind() - 1e-9 &&
                    actual_add    < player_stack_remaining  - 1e-9)
                    continue;                                       // too small
            }

            // store pure bet or raise amount (not call+raise)
            double action_size = is_raise ? raise_top_up : actual_add;
            core::GameAction bet_action(action_type, action_size);
            possible_node_actions.push_back(bet_action);

            // --- build child node ----------------------------------------------
            double next_pot            = pot + actual_add;
            double next_player_commit  = current_player_commit + actual_add;

            auto next_node = std::make_shared<nodes::ActionNode>(
                opponent_player, current_round, next_pot, node, /*num deals*/ 1);
            children_nodes.push_back(next_node);

            config::Rule next_rule = current_rule_state;            // copy
            if (current_player == 0)
                next_rule.SetInitialIpCommit(next_player_commit);
            else
                next_rule.SetInitialOopCommit(next_player_commit);

            BuildBranch(next_node,
                        next_rule,
                        bet_action,
                        actions_this_round + 1,
                        raises_this_street + 1);                    // bet counts, too
        }
    }
    node->SetActionsAndChildren(possible_node_actions, children_nodes);
}


// Calculates possible bet/raise amounts based on StreetSettings
// Added const qualifier
std::vector<double> GameTree::GetPossibleBets(
    const config::Rule& rule,
    size_t player_index,
    double current_player_commit,
    double opponent_commit,
    double effective_stack,
    const core::GameAction& last_action,
    double pot_before_action,
    core::GameRound round) const 
{ // Pot size *before* this player acts

    // Preflop betting logic is typically different (based on blinds/limps/raises)
    // and often hardcoded or handled by a separate configuration.
    // This implementation focuses on postflop pot-percentage betting.
    if (round == core::GameRound::kPreflop) {
        std::cerr << "[WARNING] GetPossibleBets called for Preflop - returning empty. Implement preflop logic if needed." << std::endl;
        return {};
    }

    const config::GameTreeBuildingSettings& settings = rule.GetBuildSettings();
    const config::StreetSetting& street_setting = settings.GetSetting(player_index, round);

    std::vector<double> size_ratios_percent_double;
    bool allow_all_in = street_setting.allow_all_in;
    bool is_raise = opponent_commit > current_player_commit;
    bool is_donk = (player_index == 1 && // OOP
                    round > core::GameRound::kPreflop &&
                    // Check if the last action was the start of the round for the opponent (implied check)
                    // or if the last action was explicitly a check by the opponent.
                    (last_action.GetAction() == core::PokerAction::kRoundBegin || last_action.GetAction() == core::PokerAction::kCheck) &&
                    !street_setting.donk_sizes_percent.empty());

    if (is_donk) {
        size_ratios_percent_double = street_setting.donk_sizes_percent;
    } else if (is_raise) {
        size_ratios_percent_double = street_setting.raise_sizes_percent;
    } else { // Bet
        size_ratios_percent_double = street_setting.bet_sizes_percent;
    }

    std::set<double> possible_amounts_to_add; // Use set to store unique absolute amounts to add
    double stack_remaining = effective_stack - current_player_commit;
    if (stack_remaining <= 1e-9) {
        return {}; // Cannot bet/raise if no stack left
    }

    double call_amount = is_raise ? (opponent_commit - current_player_commit) : 0.0;
    double min_bet_size = rule.GetBigBlind(); // Minimum legal bet size postflop is BB

    for (double ratio_percent : size_ratios_percent_double) {
        if (ratio_percent <= 0) continue;
        double ratio = ratio_percent / 100.0;

        double calculated_size = 0; // Size of the bet OR the raise amount *on top of the call*

        if (is_raise) {
            // Calculate raise size based on pot *after* calling
            double pot_after_call = pot_before_action + call_amount; // Pot already includes opponent's bet; add hero's call once
            calculated_size = ratio * pot_after_call; // This is the desired raise amount *on top*
        } else { // Bet or Donk
            // Calculate bet size based on pot *before* betting
            calculated_size = ratio * pot_before_action;
        }

        // --- Apply minimum size rules ---
        double final_size = 0.0; // Size of bet or raise amount *on top* of call
        if (is_raise) {
            // Minimum raise amount *on top* must be at least the size of the previous bet/raise
            // For simplicity here, we use the amount needed to call (call_amount) as the minimum *additional* raise amount
            // A more precise implementation would track the previous bet/raise amount explicitly.
            // Also enforce minimum raise is at least BB size on top.
            double min_raise_top_up = std::max(min_bet_size, call_amount);
            final_size = std::max(calculated_size, min_raise_top_up);
        } else { // Bet or Donk
            // Minimum bet is Big Blind
            final_size = std::max(calculated_size, min_bet_size);
        }

        // Round the calculated size (bet size or raise-top-up size)
        double rounded_size = RoundBet(final_size, rule.GetSmallBlind());

        // Calculate the TOTAL amount the player needs to add to the pot for this action
        double amount_to_add = is_raise ? (call_amount + rounded_size) : rounded_size;

        // Cap amount_to_add by the player's remaining stack
        amount_to_add = std::min(amount_to_add, stack_remaining);

        // Add the valid amount to the set if it's positive and adds value if raising
        if (amount_to_add > 1e-9) {
            if (!is_raise || amount_to_add > call_amount + 1e-9) { // Ensure raise adds > 0 on top of call
                possible_amounts_to_add.insert(amount_to_add);
            }
        }
    }

// --- Handle All-In ---
    if (allow_all_in && stack_remaining > 1e-9) {
        bool all_in_covered = false;
        if (!possible_amounts_to_add.empty()) {
            // Check if the largest calculated bet/raise is already the all-in amount
            double max_calculated_add = *possible_amounts_to_add.rbegin();
            if (std::abs(max_calculated_add - stack_remaining) < 1e-9) {
                all_in_covered = true;
            }
        }

        if (!all_in_covered) {
            // Determine if pushing the remaining stack is a valid action size
            bool is_valid_all_in = false;
            if (is_raise) {
                // All-in raise is valid if it's more than just calling
                if (stack_remaining > call_amount + 1e-9) {
                    // Check if the raise amount meets the minimum raise requirement
                    double raise_amount = stack_remaining - call_amount;
                    double min_raise_top_up = std::max(min_bet_size, call_amount);
                    if (raise_amount >= min_raise_top_up - 1e-9) {
                        is_valid_all_in = true;
                    } else {
                        // Allow undersized all-in raise (standard poker rule)
                        is_valid_all_in = true;
                    }
                }
            } else { // Bet or Donk
                // All-in bet is valid if it's at least the minimum bet size,
                // or if it's less but it's all the player has.
                if (stack_remaining >= min_bet_size - 1e-9) {
                    is_valid_all_in = true;
                } else {
                    // Allow undersized all-in bet
                    is_valid_all_in = true;
                }
            }

            if (is_valid_all_in) {
                possible_amounts_to_add.insert(stack_remaining);
            }
        }
    }

    // Convert the set of unique amounts to a vector
    return std::vector<double>(possible_amounts_to_add.begin(), possible_amounts_to_add.end());
}


// Helper for rounding bets
double GameTree::RoundBet(double amount, double min_bet_increment) {
    if (min_bet_increment <= 1e-9) return amount;
    return std::max(min_bet_increment,
                    std::round(amount / min_bet_increment) * min_bet_increment);
}


// --- JSON Loading Helpers ---
// (Implementations remain placeholders)
std::shared_ptr<core::GameTreeNode> GameTree::ParseNodeJson(const json& node_json, std::weak_ptr<core::GameTreeNode> parent) { throw std::logic_error("JSON loading not implemented."); return nullptr; }
std::shared_ptr<nodes::ActionNode> GameTree::ParseActionNode(const json& node_json, std::weak_ptr<core::GameTreeNode> parent) { throw std::logic_error("JSON loading not implemented."); return nullptr; }
std::shared_ptr<nodes::ChanceNode> GameTree::ParseChanceNode(const json& node_json, std::weak_ptr<core::GameTreeNode> parent) { throw std::logic_error("JSON loading not implemented."); return nullptr; }
std::shared_ptr<nodes::ShowdownNode> GameTree::ParseShowdownNode(const json& node_json, std::weak_ptr<core::GameTreeNode> parent) { throw std::logic_error("JSON loading not implemented."); return nullptr; }
std::shared_ptr<nodes::TerminalNode> GameTree::ParseTerminalNode(const json& node_json, std::weak_ptr<core::GameTreeNode> parent) { throw std::logic_error("JSON loading not implemented."); return nullptr; }


// --- Tree Analysis ---
void GameTree::CalculateTreeMetadata() { if (root_) CalculateMetadataRecursive(root_, 0); }
int GameTree::CalculateMetadataRecursive(std::shared_ptr<core::GameTreeNode> node, int depth) {
    if (!node) return 0;
    node->SetDepth(depth);
    int subtree_node_count = 1;
    if (auto action_node = std::dynamic_pointer_cast<nodes::ActionNode>(node)) {
        for (const auto& child : action_node->GetChildren()) { subtree_node_count += CalculateMetadataRecursive(child, depth + 1); }
    } else if (auto chance_node = std::dynamic_pointer_cast<nodes::ChanceNode>(node)) {
        subtree_node_count += CalculateMetadataRecursive(chance_node->GetChild(), depth + 1);
    }
    node->SetSubtreeSize(subtree_node_count);
    return subtree_node_count;
}
void GameTree::PrintTree(int max_depth) const { if (!root_) { std::cout << "Tree is empty." << std::endl; return; } PrintTreeRecursive(root_, 0, max_depth, ""); }
void GameTree::PrintTreeRecursive(const std::shared_ptr<core::GameTreeNode>& node, int current_depth, int max_depth, const std::string& prefix) {
    if (!node || (max_depth >= 0 && current_depth > max_depth)) return;
    std::cout << prefix;
    if (current_depth > 0) std::cout << "└── ";
    if (auto action_node = std::dynamic_pointer_cast<const nodes::ActionNode>(node)) {
         std::cout << "P" << action_node->GetPlayerIndex() << " Action (Pot: " << action_node->GetPot() << ", Round: " << core::GameTreeNode::GameRoundToString(action_node->GetRound()) << ")" << std::endl;
         const auto& actions = action_node->GetActions(); const auto& children = action_node->GetChildren();
         for (size_t i = 0; i < actions.size(); ++i) {
             std::string child_prefix = prefix + (current_depth > 0 ? "    " : "");
             bool is_last = (i == actions.size() - 1);
             std::cout << child_prefix << (is_last ? "└── " : "├── ") << "[" << actions[i].ToString() << "]" << std::endl;
             PrintTreeRecursive(children[i], current_depth + 1, max_depth, child_prefix + (is_last ? "    " : "│   "));
         }
    } else if (auto chance_node = std::dynamic_pointer_cast<const nodes::ChanceNode>(node)) {
         std::cout << "Chance (Pot: " << chance_node->GetPot() << ", Round: " << core::GameTreeNode::GameRoundToString(chance_node->GetRound()) << ")" << std::endl;
         PrintTreeRecursive(chance_node->GetChild(), current_depth + 1, max_depth, prefix + "    ");
    } else if (auto showdown_node = std::dynamic_pointer_cast<const nodes::ShowdownNode>(node)) {
         std::cout << "Showdown (Pot: " << showdown_node->GetPot() << ")" << std::endl;
    } else if (auto terminal_node = std::dynamic_pointer_cast<const nodes::TerminalNode>(node)) {
         std::cout << "Terminal (Pot: " << terminal_node->GetPot() << ", Payoffs: {";
         const auto& payoffs = terminal_node->GetPayoffs();
         for(size_t i=0; i<payoffs.size(); ++i) { std::cout << payoffs[i] << (i == payoffs.size()-1 ? "" : ", "); }
         std::cout << "})" << std::endl;
    } else { std::cout << "Unknown Node Type" << std::endl; }
}
// Added const qualifier
uint64_t GameTree::EstimateTrainableMemory(size_t p0_range_size, size_t p1_range_size) const { if (!root_) return 0; return EstimateMemoryRecursive(root_, p0_range_size, p1_range_size, 1); }
// Added const qualifier
uint64_t GameTree::EstimateMemoryRecursive(const std::shared_ptr<core::GameTreeNode>& node, size_t p0_range_size, size_t p1_range_size, size_t num_deals_multiplier) const {
    if (!node) return 0;
    uint64_t current_node_memory = 0;
    uint64_t children_memory = 0;
    if (auto action_node = std::dynamic_pointer_cast<const nodes::ActionNode>(node)) {
        size_t num_actions = action_node->GetActions().size();
        size_t range_size = (action_node->GetPlayerIndex() == 0) ? p0_range_size : p1_range_size;
        size_t trainable_size = num_actions * range_size;
        // Estimate size based on DiscountedCfrTrainable members
        size_t bytes_per_trainable = trainable_size * (sizeof(float) + sizeof(double) + sizeof(float)); // regrets + sum + evs
        // Need to know how many deal states the node actually stores
        // This info isn't directly on the node, assume perfect recall for now (multiplier=1)
        // A more accurate estimate would need to query the ActionNode's internal trainable vector size.
        // Let's use num_deals_multiplier passed down for approximation.
        current_node_memory = num_deals_multiplier * bytes_per_trainable;
        for (const auto& child : action_node->GetChildren()) {
            children_memory += EstimateMemoryRecursive(child, p0_range_size, p1_range_size, num_deals_multiplier);
        }
    } else if (auto chance_node = std::dynamic_pointer_cast<const nodes::ChanceNode>(node)) {
        // Approximation: Multiply deals by cards per street
        size_t deals_this_round = 1;
        core::GameRound round = node->GetRound(); // Round *before* the chance
        if (round == core::GameRound::kPreflop) deals_this_round = 3; // Flop
        else if (round == core::GameRound::kFlop) deals_this_round = 1; // Turn
        else if (round == core::GameRound::kTurn) deals_this_round = 1; // River
        // This is extremely rough, doesn't account for deck size reduction
        size_t next_multiplier = num_deals_multiplier * deals_this_round;
        children_memory = EstimateMemoryRecursive(chance_node->GetChild(), p0_range_size, p1_range_size, next_multiplier);
    }
    return current_node_memory + children_memory;
}


} // namespace tree
} // namespace poker_solver
