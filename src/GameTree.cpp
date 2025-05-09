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
// src/GameTree.cpp

// Builds the structure following a ChanceNode
// src/GameTree.cpp

// Builds the structure following a ChanceNode
void GameTree::BuildChanceNode(std::shared_ptr<nodes::ChanceNode> node,
                               const config::Rule& rule_at_chance_creation) { // Rule state when this ChanceNode was decided
    if (!node) return;

    // round_completed_by_this_chance_deal is the round that this ChanceNode's dealt cards complete.
    // e.g., if this node deals the Turn card, this variable will be GameRound::kTurn.
    // The child ActionNode will then be for the Turn betting round.
    core::GameRound round_completed_by_this_chance_deal = node->GetRound();
    double current_pot = node->GetPot();
    double stack = rule_at_chance_creation.GetInitialEffectiveStack(); // Use stack from the rule state

    // Determine if players were all-in *before* this specific chance event.
    // Use the commitments from the rule_at_chance_creation, which should reflect the state
    // just before this chance node was placed in the tree.
    double ip_commit = rule_at_chance_creation.GetInitialCommitment(0);
    double oop_commit = rule_at_chance_creation.GetInitialCommitment(1);
    constexpr double eps = 1e-9;

    const bool ip_is_all_in  = (stack - ip_commit) <= eps;
    const bool oop_is_all_in = (stack - oop_commit) <= eps;
    const bool effectively_all_in_runout = ip_is_all_in && oop_is_all_in; // Both players are all-in, just dealing cards

    // --- DEBUG PRINT (Entry) ---
    std::cout << "[GTB Debug BuildChanceNode] ENTER. ChanceNode deals for/completes Round: "
              << core::GameTreeNode::GameRoundToString(round_completed_by_this_chance_deal)
              << ", Pot: " << current_pot
              << ", AllInRunout: " << effectively_all_in_runout
              << " (IP Commit: " << ip_commit << ", OOP Commit: " << oop_commit << ", Stack: " << stack << ")"
              << std::endl;
    if (node->GetParent()) {
        std::cout << "    Parent Node Type: " << static_cast<int>(node->GetParent()->GetNodeType())
                  << ", Parent Round: " << core::GameTreeNode::GameRoundToString(node->GetParent()->GetRound()) << std::endl;
    }
    // --- END DEBUG PRINT ---

    std::shared_ptr<core::GameTreeNode> child_node_after_this_deal;

    if (round_completed_by_this_chance_deal == core::GameRound::kRiver) {
        // This ChanceNode dealt the River card. Next is always Showdown (betting is over or was already all-in).
        std::cout << "    [GTB Debug BuildChanceNode] This ChanceNode dealt River -> Creating ShowdownNode." << std::endl;
        child_node_after_this_deal = std::make_shared<nodes::ShowdownNode>(
            core::GameRound::kRiver, current_pot, node, 2,
            std::vector<double>{ip_commit, oop_commit}); // Commitments leading to this showdown
    } else if (effectively_all_in_runout) {
        // All-in, and this ChanceNode just dealt Flop or Turn. Need another ChanceNode for the NEXT street's deal.
        core::GameRound round_for_next_deal = core::GameTreeNode::IntToGameRound(core::GameTreeNode::GameRoundToInt(round_completed_by_this_chance_deal) + 1);
        std::cout << "    [GTB Debug BuildChanceNode] AllInRunout & current deal is "
                  << core::GameTreeNode::GameRoundToString(round_completed_by_this_chance_deal)
                  << " -> Creating next ChanceNode for "
                  << core::GameTreeNode::GameRoundToString(round_for_next_deal) << std::endl;
        child_node_after_this_deal = std::make_shared<nodes::ChanceNode>(
             round_for_next_deal, current_pot, node,
             std::vector<core::Card>{}, nullptr
         );
    } else {
        // Not an all-in runout, and not the river deal. Next is an ActionNode for betting on the current street.
        // The round for this ActionNode is the street that was just completed by this ChanceNode's deal.
        core::GameRound round_for_action_node = round_completed_by_this_chance_deal;
        size_t next_player_to_act = 1; // OOP (Player 1) typically acts first post-flop.
                                     // TODO: Refine for preflop if this builder handles it.

        std::cout << "    [GTB Debug BuildChanceNode] Not AllInRunout, not River deal -> Creating ActionNode for P" << next_player_to_act
                  << " on round " << core::GameTreeNode::GameRoundToString(round_for_action_node) << std::endl;
        child_node_after_this_deal = std::make_shared<nodes::ActionNode>(
            next_player_to_act, round_for_action_node, current_pot, node, 1);
    }

    node->SetChild(child_node_after_this_deal);
    // When recursing from a ChanceNode, the 'rule_at_chance_creation' is passed along.
    // The actions_this_round and raises_this_street are reset because a new street/betting sequence begins.
    BuildBranch(child_node_after_this_deal, rule_at_chance_creation, core::GameAction(core::PokerAction::kRoundBegin), 0, 0);
}}


// Builds the structure for an ActionNode
// Takes Rule by value as it modifies it for recursive calls
// In src/GameTree.cpp

// Builds the structure for an ActionNode
// Takes Rule by value as it modifies it for recursive calls
void tree::GameTree::BuildActionNode(std::shared_ptr<nodes::ActionNode> node,
    config::Rule current_rule_state, // Takes Rule by value
    const core::GameAction& last_action,
    int actions_this_round,
    int raises_this_street) {
    if (!node) return;

    size_t current_player = node->GetPlayerIndex();
    size_t opponent_player = 1 - current_player;
    double current_player_commit = current_rule_state.GetInitialCommitment(current_player);
    double opponent_commit = current_rule_state.GetInitialCommitment(opponent_player);
    double pot_before_action = node->GetPot(); // Pot *before* this player acts
    double stack = current_rule_state.GetInitialEffectiveStack();
    double player_stack_remaining = stack - current_player_commit;
    core::GameRound current_round = node->GetRound();

    // --- DEBUG PRINT (Entry) ---
    std::cout << "[GTB Debug BuildActionNode] ENTER. Player: " << current_player
    << ", Round: " << core::GameTreeNode::GameRoundToString(current_round)
    << ", Pot: " << pot_before_action
    << ", P" << current_player << " Commit: " << current_player_commit
    << ", P" << opponent_player << " Commit: " << opponent_commit
    << ", P" << current_player << " StackRem: " << player_stack_remaining
    << ", ActionsThisRound: " << actions_this_round
    << ", RaisesThisStreet: " << raises_this_street << std::endl;
    // --- END DEBUG PRINT ---


    if (player_stack_remaining <= 1e-9 && opponent_commit > current_player_commit) { // Player is all-in but opponent made a bigger bet
        std::cerr << "[WARNING] ActionNode for player " << current_player
        << " who is all-in but faces a larger bet. This path should ideally lead to Showdown/Terminal from parent." << std::endl;
        // This situation might occur if a player goes all-in with a very small amount,
        // and the opponent has already committed more or can still act.
        // The tree should have likely ended or gone to a specific all-in resolution.
        // For robustness, we might create a terminal node here if P must fold (cannot call).
        // Or, if they can "call" with their remaining 0 stack, it's a showdown.
        // For now, let's assume the prior logic in parent nodes handles this.
        // If not, this node should probably be a Showdown or Terminal.
        // To prevent infinite loops or errors, we stop further branching from here.
        node->SetActionsAndChildren({}, {});
        return;
    } else if (player_stack_remaining <= 1e-9) { // Player is all-in and no decision to make
        node->SetActionsAndChildren({}, {}); // No actions if all-in
        return;
    }


    std::vector<core::GameAction> possible_node_actions;
    std::vector<std::shared_ptr<core::GameTreeNode>> children_nodes;

    constexpr double eps = 1e-9;

    bool can_check = std::abs(current_player_commit - opponent_commit) < eps;
    bool can_call  = (opponent_commit - current_player_commit) > eps;

    double opp_remaining_stack = stack - opponent_commit;
    bool opponent_is_all_in = opp_remaining_stack <= eps; // Simpler all-in check


    bool can_fold = can_call; // Can only fold if facing a bet/raise that requires a call

    bool can_bet_or_raise = !opponent_is_all_in &&
    (player_stack_remaining > eps) && // Must have something to bet/raise
    (raises_this_street < current_rule_state.GetRaiseLimitPerStreet());

    // --- 1. Check Action ---
    if (can_check) {
        core::GameAction check_action(core::PokerAction::kCheck);
        possible_node_actions.push_back(check_action);
        std::shared_ptr<core::GameTreeNode> child_node_after_check;

        bool round_ends = (actions_this_round > 0); // If P1 checks, P0 acts (actions_this_round=0). If P0 then checks, round ends (actions_this_round=1).

        if (round_ends) {
            if (current_round == core::GameRound::kRiver) {
            std::vector<double> final_commitments_for_showdown = {
                current_rule_state.GetInitialCommitment(0), // IP commit
                current_rule_state.GetInitialCommitment(1)  // OOP commit
            };
            child_node_after_check = std::make_shared<nodes::ShowdownNode>(
            current_round, pot_before_action, node, 2,
            final_commitments_for_showdown// Pass final commitments
            );
            std::cout << "    [GTB Debug BuildActionNode] Check (ends round) on River -> Created ShowdownNode. Pot: " << pot_before_action << std::endl;
        } else {
            core::GameRound round_for_next_deal = core::GameTreeNode::IntToGameRound(core::GameTreeNode::GameRoundToInt(current_round) + 1);
            child_node_after_check = std::make_shared<nodes::ChanceNode>(
            round_for_next_deal, pot_before_action, node, std::vector<core::Card>{}, nullptr);
            std::cout << "    [GTB Debug BuildActionNode] Check (ends round) on " << core::GameTreeNode::GameRoundToString(current_round)
            << " -> Created ChanceNode for " << core::GameTreeNode::GameRoundToString(round_for_next_deal)
            << ". Pot: " << pot_before_action << std::endl;
        }
    } else { // First check in the round -> opponent's turn on the same street
            child_node_after_check = std::make_shared<nodes::ActionNode>(
            opponent_player, current_round, pot_before_action, node, 1);
            std::cout << "    [GTB Debug BuildActionNode] Check (first action) -> Created ActionNode for P"
            << opponent_player << " on " << core::GameTreeNode::GameRoundToString(current_round)
            << ". Pot: " << pot_before_action << std::endl;
        }
    children_nodes.push_back(child_node_after_check);
    BuildBranch(child_node_after_check, current_rule_state, check_action, actions_this_round + 1, raises_this_street);
    }

    // --- 2. Call Action ---
    if (can_call) {
        core::GameAction call_action(core::PokerAction::kCall); // Amount is implicit
        possible_node_actions.push_back(call_action);

        double amount_to_call = opponent_commit - current_player_commit;
        double actual_call_amount = std::min(amount_to_call, player_stack_remaining); // Cannot call more than stack

        double next_pot = pot_before_action + actual_call_amount;
        double next_player_commit = current_player_commit + actual_call_amount;

        std::shared_ptr<core::GameTreeNode> child_node_after_call;
        bool called_player_is_now_all_in = (player_stack_remaining - actual_call_amount) <= eps;
        bool all_in_by_call = called_player_is_now_all_in || opponent_is_all_in;

        if (current_round == core::GameRound::kRiver) {
            std::vector<double> final_commitments_for_showdown;
            if (current_player == 0) { // IP called
                final_commitments_for_showdown = {next_player_commit, opponent_commit};
            } else { // OOP called
                final_commitments_for_showdown = {opponent_commit, next_player_commit};
            }
            child_node_after_call = std::make_shared<nodes::ShowdownNode>(
            core::GameRound::kRiver, next_pot, node, 2, final_commitments_for_showdown // P1 commit
            );
            std::cout << "    [GTB Debug BuildActionNode] Call on River -> Created ShowdownNode. Pot: " << next_pot << std::endl;
        } else if (all_in_by_call) {
            core::GameRound round_for_next_deal = core::GameTreeNode::IntToGameRound(core::GameTreeNode::GameRoundToInt(current_round) + 1);
            child_node_after_call = std::make_shared<nodes::ChanceNode>(
            round_for_next_deal, next_pot, node, std::vector<core::Card>{}, nullptr);
            std::cout << "    [GTB Debug BuildActionNode] Call (All-in on " << core::GameTreeNode::GameRoundToString(current_round)
            << ") -> Created ChanceNode for " << core::GameTreeNode::GameRoundToString(round_for_next_deal)
            << ". Pot: " << next_pot << std::endl;
        } else { // Not river, not all-in, call completes betting for this street
            core::GameRound round_for_next_deal = core::GameTreeNode::IntToGameRound(core::GameTreeNode::GameRoundToInt(current_round) + 1);
            child_node_after_call = std::make_shared<nodes::ChanceNode>(
            round_for_next_deal, next_pot, node, std::vector<core::Card>{}, nullptr);
            std::cout << "    [GTB Debug BuildActionNode] Call (Normal on " << core::GameTreeNode::GameRoundToString(current_round)
            << ") -> Created ChanceNode for " << core::GameTreeNode::GameRoundToString(round_for_next_deal)
            << ". Pot: " << next_pot << std::endl;
        }
    children_nodes.push_back(child_node_after_call);
    config::Rule next_rule_call = current_rule_state;
    if (current_player == 0) next_rule_call.SetInitialIpCommit(next_player_commit);
    else next_rule_call.SetInitialOopCommit(next_player_commit);
    BuildBranch(child_node_after_call, next_rule_call, call_action, actions_this_round + 1, raises_this_street);
}

// --- 3. Fold Action ---
    if (can_fold) {
    const core::GameAction fold_action(core::PokerAction::kFold);
    possible_node_actions.push_back(fold_action);

    std::vector<double> payoffs(2);
    // Player who folds loses their current commitment.
    // Opponent wins what the folder had committed.
    payoffs[current_player] = -current_player_commit;
    payoffs[opponent_player] = current_player_commit; // Simplified: opponent wins current player's commitment
                                // More accurately, opponent wins what was in the pot from THIS player

    // Let's refine payoffs for fold:
    // Pot before this action was `pot_before_action`.
    // If current_player folds, opponent wins `pot_before_action`.
    // Net for current_player = -current_player_commit
    // Net for opponent_player = (pot_before_action - opponent_commit)
    // Example: Pot=10 (5 from P0, 5 from P1). P1 (OOP) bets 5 (pot=15, P1_commit=10). P0 (IP) folds.
    // P0 commit=5. P0 net = -5.
    // P1 wins pot of 15. P1 initial commit was 10. P1 net = 15 - 10 = +5.
    // This matches payoffs[current_player] = -current_player_commit; payoffs[opponent_player] = current_player_commit;
    // IF pot_before_action = current_player_commit + opponent_commit AND opponent_commit is what the opponent had in
    // before the action that current_player is folding to.
    // The current `pot_before_action` is the total pot *before* the current player's turn.
    // If P1 bets, P0 folds:
    // P0 loses: P0's initial commitment.
    // P1 wins: Pot before P1's bet + P0's initial commitment.
    // The simple payoffs above are correct in terms of *net change from start of hand*.

    auto child_node_after_fold = std::make_shared<nodes::TerminalNode>(
    payoffs, current_round, pot_before_action, node); // Pot doesn't change due to fold itself
    std::cout << "    [GTB Debug BuildActionNode] Fold -> Created TerminalNode. Pot: " << pot_before_action << std::endl;

    children_nodes.push_back(child_node_after_fold);
    // No BuildBranch for TerminalNode
    }

    // --- 4. Bet / Raise Actions ---
    if (can_bet_or_raise) {
        bool is_facing_action = opponent_commit > current_player_commit + eps;
        bool is_raise = is_facing_action;
        auto action_type = is_raise ? core::PokerAction::kRaise : core::PokerAction::kBet;

        std::vector<double> bet_amounts_to_add = GetPossibleBets(
        current_rule_state, current_player, current_player_commit, opponent_commit,
        stack, last_action, pot_before_action, current_round);

        for (double total_amount_player_will_put_in_for_this_action : bet_amounts_to_add) {
        // total_amount_player_will_put_in_for_this_action is the amount TO ADD to the pot by current player
        // for this specific bet/raise action. It's not the total commitment.

        double actual_bet_or_raise_value = total_amount_player_will_put_in_for_this_action;
            if (is_raise) {
            // total_amount_player_will_put_in_for_this_action includes the call part
            actual_bet_or_raise_value = total_amount_player_will_put_in_for_this_action - (opponent_commit - current_player_commit);
            if (actual_bet_or_raise_value < eps) continue; // Not a valid raise beyond call
        }


        core::GameAction bet_raise_action(action_type, actual_bet_or_raise_value);
        possible_node_actions.push_back(bet_raise_action);

        double next_pot = pot_before_action + total_amount_player_will_put_in_for_this_action;
        double next_player_commit = current_player_commit + total_amount_player_will_put_in_for_this_action;

        std::shared_ptr<core::GameTreeNode> child_node_after_bet_raise;
        bool bet_makes_current_player_all_in = (next_player_commit >= stack - eps);

        // After a bet/raise, it's always the opponent's turn to act on the same street,
        // unless the bet/raise was an all-in that the opponent is already covering or is forced to call all-in.
        if (bet_makes_current_player_all_in && opponent_commit >= next_player_commit) {
        // Player raised all-in, but opponent already has them covered or committed the same.
        // This should effectively be a showdown after this action if opponent's previous action was a bet/raise.
        // Or, if opponent checked, they now face an all-in.
        // This case is tricky: if P0 bets all-in, P1 must act (call/fold).
        // If P0 raises all-in over P1's bet, P1 must act (call/fold).
        // The only way it goes to showdown immediately is if P0 calls P1's all-in (handled by Call logic)
        // OR P0 raises all-in and P1 already has less stack and is thus all-in by calling.
        // For simplicity, if current player bets/raises all-in, next node is ActionNode for opponent
        // unless opponent is already all-in and covered.

        // If opponent IS all-in and player's bet/raise covers them:
        if (opponent_is_all_in && next_player_commit >= opponent_commit) {
            if (current_round == core::GameRound::kRiver) {
                std::vector<double> final_commitments_for_showdown;
                if (current_player == 0) { // IP made the final bet/raise
                    final_commitments_for_showdown = {next_player_commit, opponent_commit};
                } else { // OOP made the final bet/raise
                    final_commitments_for_showdown = {opponent_commit, next_player_commit};
                }
                child_node_after_bet_raise = std::make_shared<nodes::ShowdownNode>(
                    core::GameRound::kRiver, next_pot, node, 2,
                    final_commitments_for_showdown);
                std::cout << "    [GTB Debug BuildActionNode] Bet/Raise (P" << current_player << " covers Opponent All-in) on River -> Showdown. Pot: " << next_pot << std::endl;
                } else {
                core::GameRound round_for_next_deal = core::GameTreeNode::IntToGameRound(core::GameTreeNode::GameRoundToInt(current_round) + 1);
                child_node_after_bet_raise = std::make_shared<nodes::ChanceNode>(
                round_for_next_deal, next_pot, node, std::vector<core::Card>{}, nullptr);
                std::cout << "    [GTB Debug BuildActionNode] Bet/Raise (P" << current_player << " covers Opponent All-in) on " << core::GameTreeNode::GameRoundToString(current_round)
                    << " -> ChanceNode for " << core::GameTreeNode::GameRoundToString(round_for_next_deal) << ". Pot: " << next_pot << std::endl;
            }
        } else { // Player bets/raises (possibly all-in), opponent is NOT all-in or is not covered yet
        child_node_after_bet_raise = std::make_shared<nodes::ActionNode>(
        opponent_player, current_round, next_pot, node, 1);
        std::cout << "    [GTB Debug BuildActionNode] Bet/Raise by P" << current_player << " on " << core::GameTreeNode::GameRoundToString(current_round)
        << " (Amount: " << actual_bet_or_raise_value << ", All-in: " << bet_makes_current_player_all_in
        << ") -> ActionNode for P" << opponent_player << ". Pot: " << next_pot << std::endl;
    }
    } else { // Not an all-in situation by current player that ends action
        child_node_after_bet_raise = std::make_shared<nodes::ActionNode>(
        opponent_player, current_round, next_pot, node, 1);
        std::cout << "    [GTB Debug BuildActionNode] Bet/Raise by P" << current_player << " on " << core::GameTreeNode::GameRoundToString(current_round)
        << " (Amount: " << actual_bet_or_raise_value
        << ") -> ActionNode for P" << opponent_player << ". Pot: " << next_pot << std::endl;
    }
        children_nodes.push_back(child_node_after_bet_raise);

        config::Rule next_rule_bet_raise = current_rule_state;
        if (current_player == 0) next_rule_bet_raise.SetInitialIpCommit(next_player_commit);
        else next_rule_bet_raise.SetInitialOopCommit(next_player_commit);
        BuildBranch(child_node_after_bet_raise, next_rule_bet_raise, bet_raise_action, actions_this_round + 1, raises_this_street + 1);
    }
    }
    node->SetActionsAndChildren(possible_node_actions, children_nodes);
}


// Calculates possible bet/raise amounts based on StreetSettings
// Added const qualifier
std::vector<double> tree::GameTree::GetPossibleBets(
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
double tree::GameTree::RoundBet(double amount, double min_bet_increment) {
    if (min_bet_increment <= 1e-9) return amount;
    return std::max(min_bet_increment,
                    std::round(amount / min_bet_increment) * min_bet_increment);
}


// --- JSON Loading Helpers ---
// (Implementations remain placeholders)
std::shared_ptr<core::GameTreeNode> tree::GameTree::ParseNodeJson(const json& node_json, std::weak_ptr<core::GameTreeNode> parent) { throw std::logic_error("JSON loading not implemented."); return nullptr; }
std::shared_ptr<nodes::ActionNode> tree::GameTree::ParseActionNode(const json& node_json, std::weak_ptr<core::GameTreeNode> parent) { throw std::logic_error("JSON loading not implemented."); return nullptr; }
std::shared_ptr<nodes::ChanceNode> tree::GameTree::ParseChanceNode(const json& node_json, std::weak_ptr<core::GameTreeNode> parent) { throw std::logic_error("JSON loading not implemented."); return nullptr; }
std::shared_ptr<nodes::ShowdownNode> tree::GameTree::ParseShowdownNode(const json& node_json, std::weak_ptr<core::GameTreeNode> parent) { throw std::logic_error("JSON loading not implemented."); return nullptr; }
std::shared_ptr<nodes::TerminalNode> tree::GameTree::ParseTerminalNode(const json& node_json, std::weak_ptr<core::GameTreeNode> parent) { throw std::logic_error("JSON loading not implemented."); return nullptr; }


// --- Tree Analysis ---
void tree::GameTree::CalculateTreeMetadata() { if (root_) CalculateMetadataRecursive(root_, 0); }
int tree::GameTree::CalculateMetadataRecursive(std::shared_ptr<core::GameTreeNode> node, int depth) {
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
void tree::GameTree::PrintTree(int max_depth) const { if (!root_) { std::cout << "Tree is empty." << std::endl; return; } PrintTreeRecursive(root_, 0, max_depth, ""); }
void tree::GameTree::PrintTreeRecursive(const std::shared_ptr<core::GameTreeNode>& node, int current_depth, int max_depth, const std::string& prefix) {
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
uint64_t tree::GameTree::EstimateTrainableMemory(size_t p0_range_size, size_t p1_range_size) const { if (!root_) return 0; return EstimateMemoryRecursive(root_, p0_range_size, p1_range_size, 1); }
// Added const qualifier
uint64_t tree::GameTree::EstimateMemoryRecursive(const std::shared_ptr<core::GameTreeNode>& node, size_t p0_range_size, size_t p1_range_size, size_t num_deals_multiplier) const {
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
 // namespace poker_solver
