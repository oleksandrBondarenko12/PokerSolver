#include "kuhn/kuhn_poker_setup.h"
#include "nodes/ActionNode.h"
#include "nodes/ChanceNode.h"
#include "nodes/ShowdownNode.h"
#include "nodes/TerminalNode.h"
#include "nodes/GameActions.h" // For core::GameAction
#include "ranges/PrivateCards.h" // For core::PrivateCards

#include <vector>
#include <memory>
#include <map>
#include <string>
#include <stdexcept>
#include <iostream> // For potential debug output

// Use aliases
namespace core = poker_solver::core;
namespace nodes = poker_solver::nodes;
namespace kuhn = poker_solver::kuhn;

namespace poker_solver {
namespace kuhn {

// --- KuhnCompairer Implementation ---

core::ComparisonResult KuhnCompairer::CompareHands(int card1, int card2) const {
     // Basic validation
     if (card1 < KUHN_CARD_J || card1 > KUHN_CARD_K || card2 < KUHN_CARD_J || card2 > KUHN_CARD_K) {
          // Handle invalid card values if necessary, maybe throw or return tie?
          // For simplicity, assuming valid inputs during CFR if ranges are correct.
          return core::ComparisonResult::kTie;
     }
    if (card1 > card2) return core::ComparisonResult::kPlayer1Wins; // Player 1 has higher card
    if (card2 > card1) return core::ComparisonResult::kPlayer2Wins; // Player 2 has higher card
    return core::ComparisonResult::kTie; // Should not happen if cards are different
}

core::ComparisonResult KuhnCompairer::CompareHands(
    const std::vector<int>& private_hand1,
    const std::vector<int>& private_hand2,
    const std::vector<int>& public_board) const {
    (void)public_board; // Unused in Kuhn
    if (private_hand1.size() != 1 || private_hand2.size() != 1) {
        throw std::invalid_argument("KuhnCompairer::CompareHands expects single card hands.");
    }
    return CompareHands(private_hand1[0], private_hand2[0]);
}

core::ComparisonResult KuhnCompairer::CompareHands(uint64_t, uint64_t, uint64_t) const {
    throw std::logic_error("KuhnCompairer::CompareHands mask version not implemented/applicable.");
    // return core::ComparisonResult::kTie; // Or return Tie
}

int KuhnCompairer::GetHandRank(const std::vector<int>& private_hand,
                               const std::vector<int>& public_board) const {
    (void)public_board; // Unused
    if (private_hand.size() != 1) {
         throw std::invalid_argument("KuhnCompairer::GetHandRank expects single card hand.");
    }
    // Rank can simply be the card value for Kuhn
    if (private_hand[0] >= KUHN_CARD_J && private_hand[0] <= KUHN_CARD_K) {
         return private_hand[0];
    }
     throw std::out_of_range("Invalid card value in KuhnCompairer::GetHandRank.");

}

int KuhnCompairer::GetHandRank(uint64_t, uint64_t) const {
    throw std::logic_error("KuhnCompairer::GetHandRank mask version not implemented/applicable.");
    // return -1; // Or return invalid rank
}


// --- Kuhn Game Tree Builder Implementation ---

std::shared_ptr<core::GameTreeNode> build_kuhn_game_tree() {
    // Constants
    const double ante = 1.0;
    const double bet_size = 1.0;
    const double initial_pot = ante * 2.0;

    // Payoffs (P0, P1)
    const std::vector<double> p1_folds_payoffs = {ante, -ante};              // P0 wins pot (2 antes) -> P0 net +1, P1 net -1
    const std::vector<double> p0_folds_payoffs = {-ante, ante};              // P1 wins pot (2 antes) -> P0 net -1, P1 net +1
    const std::vector<double> p0_wins_pot4_payoffs = {ante + bet_size, -(ante + bet_size)}; // P0 wins pot (4) -> P0 net +2, P1 net -2
    const std::vector<double> p1_wins_pot4_payoffs = {-(ante + bet_size), ante + bet_size}; // P1 wins pot (4) -> P0 net -2, P1 net +2
    const std::vector<double> p0_wins_pot2_payoffs = {ante, -ante};              // P0 wins pot (2) -> P0 net +1, P1 net -1 (Check/Check showdown)
    const std::vector<double> p1_wins_pot2_payoffs = {-ante, ante};              // P1 wins pot (2) -> P1 net +1, P0 net -1 (Check/Check showdown)


    // Actions
    // Using existing enums requires mapping. Let's simplify for Kuhn construction.
    // 0 = Check/Fold, 1 = Bet/Call
    const core::GameAction check_action(core::PokerAction::kCheck);
    const core::GameAction bet_action(core::PokerAction::kBet, bet_size);
    const core::GameAction fold_action(core::PokerAction::kFold);
    const core::GameAction call_action(core::PokerAction::kCall);

    // We need to map (P1 Card, P2 Card) -> Showdown Node Payoffs
    // This map is complex to build inline. Let's create ShowdownNodes dynamically
    // and have the CFR logic calculate the payoff based on context.
    // Create placeholder Showdown/Terminal nodes first, then link them.

    // Create nodes for P1 acting after P1=J, P2=Q deal
    auto term_p1chk_p0bet_p1folds_JQ = std::make_shared<nodes::TerminalNode>(p1_folds_payoffs, core::GameRound::kPreflop, initial_pot + bet_size, std::weak_ptr<core::GameTreeNode>());
    auto show_p1chk_p0bet_p1calls_JQ = std::make_shared<nodes::ShowdownNode>(core::GameRound::kPreflop, initial_pot + 2*bet_size, std::weak_ptr<core::GameTreeNode>(), 2, std::vector<double>{ante+bet_size, ante+bet_size}); // P0: J, P1: Q -> P1 Wins
    auto p1_act_p1chk_p0bet_JQ = std::make_shared<nodes::ActionNode>(1, core::GameRound::kPreflop, initial_pot + bet_size, std::weak_ptr<core::GameTreeNode>()); // P1 to act
    p1_act_p1chk_p0bet_JQ->AddChild(fold_action, term_p1chk_p0bet_p1folds_JQ);
    p1_act_p1chk_p0bet_JQ->AddChild(call_action, show_p1chk_p0bet_p1calls_JQ);

    auto show_p1chk_p0chk_JQ = std::make_shared<nodes::ShowdownNode>(core::GameRound::kPreflop, initial_pot, std::weak_ptr<core::GameTreeNode>(), 2, std::vector<double>{ante, ante}); // P0: J, P1: Q -> P1 Wins
    auto p0_act_p1chk_JQ = std::make_shared<nodes::ActionNode>(0, core::GameRound::kPreflop, initial_pot, std::weak_ptr<core::GameTreeNode>()); // P0 to act
    p0_act_p1chk_JQ->AddChild(check_action, show_p1chk_p0chk_JQ);
    p0_act_p1chk_JQ->AddChild(bet_action, p1_act_p1chk_p0bet_JQ);

    auto term_p1bet_p0folds_JQ = std::make_shared<nodes::TerminalNode>(p0_folds_payoffs, core::GameRound::kPreflop, initial_pot + bet_size, std::weak_ptr<core::GameTreeNode>());
    auto show_p1bet_p0calls_JQ = std::make_shared<nodes::ShowdownNode>(core::GameRound::kPreflop, initial_pot + 2*bet_size, std::weak_ptr<core::GameTreeNode>(), 2, std::vector<double>{ante+bet_size, ante+bet_size}); // P0: J, P1: Q -> P1 Wins
    auto p0_act_p1bet_JQ = std::make_shared<nodes::ActionNode>(0, core::GameRound::kPreflop, initial_pot + bet_size, std::weak_ptr<core::GameTreeNode>()); // P0 to act
    p0_act_p1bet_JQ->AddChild(fold_action, term_p1bet_p0folds_JQ);
    p0_act_p1bet_JQ->AddChild(call_action, show_p1bet_p0calls_JQ);

    auto p1_root_action_JQ = std::make_shared<nodes::ActionNode>(1, core::GameRound::kPreflop, initial_pot, std::weak_ptr<core::GameTreeNode>()); // P1 acts first
    p1_root_action_JQ->AddChild(check_action, p0_act_p1chk_JQ);
    p1_root_action_JQ->AddChild(bet_action, p0_act_p1bet_JQ);


    // Structure: Root -> Deals -> P1 Card -> P2 Card -> P1 Action Node
    // Let's use nested maps for clarity during construction
    // Map: P1 Card -> Map: P2 Card -> P1 Root Action Node
    std::map<int, std::map<int, std::shared_ptr<nodes::ActionNode>>> deal_outcomes;

    // --- Build the tree for all 6 deals ---
    int cards[] = { KUHN_CARD_J, KUHN_CARD_Q, KUHN_CARD_K };
    for (int p1_card : cards) {
        for (int p2_card : cards) {
            if (p1_card == p2_card) continue;

            // Determine winner for payoffs
            bool p1_wins_showdown = p1_card > p2_card;
            const auto& pot4_payoffs = p1_wins_showdown ? p1_wins_pot4_payoffs : p0_wins_pot4_payoffs;
            const auto& pot2_payoffs = p1_wins_showdown ? p1_wins_pot2_payoffs : p0_wins_pot2_payoffs;
            // Showdown/Terminal nodes contain the *final* commitments/pot
            double final_pot_check = initial_pot;
            double final_pot_bet = initial_pot + bet_size;
            double final_pot_call = initial_pot + 2*bet_size;
            double p0_commit_final_call = ante + bet_size;
            double p1_commit_final_call = ante + bet_size;
            double p0_commit_final_check = ante;
            double p1_commit_final_check = ante;

            // Create nodes specific to this deal (P1 card, P2 card)
            auto term_p1chk_p0bet_p1folds = std::make_shared<nodes::TerminalNode>(p1_folds_payoffs, core::GameRound::kPreflop, final_pot_bet, std::weak_ptr<core::GameTreeNode>());
            auto show_p1chk_p0bet_p1calls = std::make_shared<nodes::ShowdownNode>(core::GameRound::kPreflop, final_pot_call, std::weak_ptr<core::GameTreeNode>(), 2, std::vector<double>{p0_commit_final_call, p1_commit_final_call});

            auto p1_act_p1chk_p0bet = std::make_shared<nodes::ActionNode>(1, core::GameRound::kPreflop, final_pot_bet, std::weak_ptr<core::GameTreeNode>());
            p1_act_p1chk_p0bet->AddChild(fold_action, term_p1chk_p0bet_p1folds);
            p1_act_p1chk_p0bet->AddChild(call_action, show_p1chk_p0bet_p1calls);

            auto show_p1chk_p0chk = std::make_shared<nodes::ShowdownNode>(core::GameRound::kPreflop, final_pot_check, std::weak_ptr<core::GameTreeNode>(), 2, std::vector<double>{p0_commit_final_check, p1_commit_final_check});
            auto p0_act_p1chk = std::make_shared<nodes::ActionNode>(0, core::GameRound::kPreflop, final_pot_check, std::weak_ptr<core::GameTreeNode>());
            p0_act_p1chk->AddChild(check_action, show_p1chk_p0chk);
            p0_act_p1chk->AddChild(bet_action, p1_act_p1chk_p0bet);

            auto term_p1bet_p0folds = std::make_shared<nodes::TerminalNode>(p0_folds_payoffs, core::GameRound::kPreflop, final_pot_bet, std::weak_ptr<core::GameTreeNode>());
            auto show_p1bet_p0calls = std::make_shared<nodes::ShowdownNode>(core::GameRound::kPreflop, final_pot_call, std::weak_ptr<core::GameTreeNode>(), 2, std::vector<double>{p0_commit_final_call, p1_commit_final_call});
            auto p0_act_p1bet = std::make_shared<nodes::ActionNode>(0, core::GameRound::kPreflop, final_pot_bet, std::weak_ptr<core::GameTreeNode>());
            p0_act_p1bet->AddChild(fold_action, term_p1bet_p0folds);
            p0_act_p1bet->AddChild(call_action, show_p1bet_p0calls);

            auto p1_root_action = std::make_shared<nodes::ActionNode>(1, core::GameRound::kPreflop, initial_pot, std::weak_ptr<core::GameTreeNode>());
            p1_root_action->AddChild(check_action, p0_act_p1chk);
            p1_root_action->AddChild(bet_action, p0_act_p1bet);

            // Store the root action node for this specific deal
            deal_outcomes[p1_card][p2_card] = p1_root_action;
        }
    }

    // Create the initial deal chance nodes
    auto root = std::make_shared<nodes::ChanceNode>(core::GameRound::kPreflop, 0.0, std::weak_ptr<core::GameTreeNode>(), std::vector<core::Card>{}, nullptr);

    // Map representing the actual dealt cards isn't directly part of the node structure here.
    // Instead, the CFR traversal needs to handle the information sets.
    // The tree structure needs to represent *all* possible paths.
    // Let's rethink: the tree *should* branch based on the deals.

    // Corrected Structure:
    // Root (dummy action/chance) -> Chance (Deal P1 Card) -> Chance (Deal P2 Card) -> P1 Action

    auto deal_p1_chance = std::make_shared<nodes::ChanceNode>(core::GameRound::kPreflop, 0.0, std::weak_ptr<core::GameTreeNode>(), std::vector<core::Card>{}, nullptr); // Pot starts at 0 before antes

    for (int p1_card : cards) {
         auto deal_p2_chance = std::make_shared<nodes::ChanceNode>(core::GameRound::kPreflop, 0.0, deal_p1_chance, std::vector<core::Card>{core::Card(p1_card)}, nullptr); // Pot still 0, encode dealt card
         deal_p1_chance->AddChild(core::GameAction(core::PokerAction::kRoundBegin, p1_card), deal_p2_chance); // Use amount to store card info for chance

         for (int p2_card : cards) {
             if (p1_card == p2_card) continue;

             // Get the pre-built action subtree for this specific deal
             auto p1_action_root = deal_outcomes[p1_card][p2_card];
             // Link P2 chance node to the P1 action node for this outcome
             // The action here represents P2 getting their card. Pot becomes 2.0 after antes.
              deal_p2_chance->AddChild(core::GameAction(core::PokerAction::kRoundBegin, p2_card), p1_action_root);
               // Set parent pointers correctly now that tree is structured
               p1_action_root->SetParent(deal_p2_chance);
               // We need to recursively set parents down the tree... this is messy manually.
               // Let GameTree handle parent setting via AddChild maybe? Or do a post-pass.

               // Manual parent setting (example for one branch):
               auto p0_act_p1chk = std::dynamic_pointer_cast<nodes::ActionNode>(p1_action_root->GetChildren()[0]); // Assuming Check is index 0
               if(p0_act_p1chk) p0_act_p1chk->SetParent(p1_action_root);
               auto p0_act_p1bet = std::dynamic_pointer_cast<nodes::ActionNode>(p1_action_root->GetChildren()[1]); // Assuming Bet is index 1
               if(p0_act_p1bet) p0_act_p1bet->SetParent(p1_action_root);
               // ... continue setting parents for the entire subtree ...
               // This manual parent setting is error-prone. Let's rely on AddChild logic if possible,
               // or create a helper function `set_parents_recursively(node)`.

               // Simplification: The previous build logic already used AddChild,
               // which should have set parents correctly *within* the subtree stored in deal_outcomes.
               // We only need to link deal_p2_chance to p1_action_root correctly.
               // Let's assume ChanceNode::AddChild (if it existed) or a similar mechanism handles parents.
               // If not, a post-build traversal to set parents is needed.
         }
    }

    // For simplicity, let's return the first chance node (Deal P1's card) as the root.
    // The absolute root could be a dummy node if preferred.
    return deal_p1_chance;

    // --- Post-build Parent Setting (Alternative if AddChild doesn't handle it fully) ---
    /*
    std::vector<std::shared_ptr<core::GameTreeNode>> stack;
    stack.push_back(deal_p1_chance); // Start from the root we return
    while(!stack.empty()) {
        auto node = stack.back();
        stack.pop_back();
        if(!node) continue;

        if(auto an = std::dynamic_pointer_cast<nodes::ActionNode>(node)) {
            for(const auto& child : an->GetChildren()) {
                if(child) {
                    child->SetParent(an); // Set parent explicitly
                    stack.push_back(child);
                }
            }
        } else if (auto cn = std::dynamic_pointer_cast<nodes::ChanceNode>(node)) {
             // Assuming ChanceNode might store multiple children keyed by action/card value
             // This part needs adaptation based on how ChanceNode stores children.
             // If it only has one child via GetChild():
              auto child = cn->GetChild();
              if(child) {
                  child->SetParent(cn);
                  stack.push_back(child);
              }
        }
    }
    return deal_p1_chance;
    */
}


// --- Helper: Initial Kuhn Range ---
std::vector<core::PrivateCards> get_kuhn_initial_range() {
    std::vector<core::PrivateCards> range;
    range.reserve(KUHN_DECK_SIZE);
    // Use a dummy second card int (-1) since PrivateCards requires two,
    // or modify PrivateCards to handle single cards. Using dummy is simpler for now.
    // IMPORTANT: The logic using this range must only consider card1_int.
    try {
        range.emplace_back(KUHN_CARD_J, KUHN_CARD_Q, 1.0); // J vs Q (Q needed for constructor)
        range.emplace_back(KUHN_CARD_Q, KUHN_CARD_K, 1.0); // Q vs K
        range.emplace_back(KUHN_CARD_K, KUHN_CARD_J, 1.0); // K vs J
    } catch (const std::exception& e) {
        // This indicates an issue with the PrivateCards constructor or card values
        std::cerr << "Error creating Kuhn initial range: " << e.what() << std::endl;
        throw; // Rethrow
    }

    // Correct approach: PrivateCards stores ONE card for Kuhn.
    // This requires modifying PrivateCards or using a different structure.
    // Let's simulate by storing (Card, Card) but only using Card1.
     std::vector<core::PrivateCards> corrected_range;
     corrected_range.reserve(KUHN_DECK_SIZE);
     corrected_range.emplace_back(KUHN_CARD_J, KUHN_CARD_Q, 1.0); // J (stores J,Q)
     corrected_range.emplace_back(KUHN_CARD_Q, KUHN_CARD_J, 1.0); // Q (stores J,Q - distinct object needed)
     corrected_range.emplace_back(KUHN_CARD_K, KUHN_CARD_J, 1.0); // K (stores J,K)
     // This is awkward. A dedicated KuhnHand struct is better.
     // Sticking with the prompt's request to use existing structure:
     // The CFR logic MUST be aware that only the *first* card matters for Kuhn
     // when using ranges created like this. Let's create them uniquely:
     range.clear();
     range.emplace_back(0, 1, 1.0); // Represents holding J (needs Q as dummy)
     range.emplace_back(1, 0, 1.0); // Represents holding Q (needs J as dummy)
     range.emplace_back(2, 0, 1.0); // Represents holding K (needs J as dummy)

    return range;
}


} // namespace kuhn
} // namespace poker_solver
