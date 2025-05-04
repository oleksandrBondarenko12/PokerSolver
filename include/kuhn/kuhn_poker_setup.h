i#ifndef POKER_SOLVER_KUHN_KUHN_POKER_SETUP_H_
#define POKER_SOLVER_KUHN_KUHN_POKER_SETUP_H_

#include "compairer/Compairer.h" // Base class for comparer
#include "nodes/GameTreeNode.h"  // Base class for tree nodes
#include "GameTree.h"            // For GameTree class (optional, could return root node directly)

#include <vector>
#include <memory> // For std::shared_ptr
#include <stdexcept>

namespace poker_solver {

// Forward declare node types used in the builder
namespace nodes {
    class ActionNode;
    class ChanceNode;
    class ShowdownNode;
    class TerminalNode;
} // namespace nodes

namespace kuhn {

// --- Constants for Kuhn Poker ---
constexpr int KUHN_CARD_J = 0;
constexpr int KUHN_CARD_Q = 1;
constexpr int KUHN_CARD_K = 2;
constexpr int KUHN_DECK_SIZE = 3;

// Represent actions simply for Kuhn
enum class KuhnActionType { Check = 0, Bet = 1, Fold = 0, Call = 1 }; // Check/Fold=0, Bet/Call=1

// --- Kuhn Hand Comparer ---
class KuhnCompairer : public core::Compairer {
public:
    KuhnCompairer() = default;
    ~KuhnCompairer() override = default;

    // Compares two single Kuhn cards (higher card wins)
    core::ComparisonResult CompareHands(int private_card1, int private_card2) const;

    // --- Implementations matching the Compairer interface ---
    // We only really need the single-card comparison for Kuhn.
    // Others can throw or return default/invalid values.

    core::ComparisonResult CompareHands(
        const std::vector<int>& private_hand1,
        const std::vector<int>& private_hand2,
        const std::vector<int>& public_board) const override;

    // Mask versions are not applicable to Kuhn in this simple setup
    core::ComparisonResult CompareHands(uint64_t private_mask1,
                                        uint64_t private_mask2,
                                        uint64_t public_mask) const override;

    int GetHandRank(const std::vector<int>& private_hand,
                    const std::vector<int>& public_board) const override;

    // Mask version not applicable
    int GetHandRank(uint64_t private_mask,
                    uint64_t public_mask) const override;

};

// --- Kuhn Game Tree Builder ---
// Function to explicitly build the Kuhn Poker game tree.
// Returns the root node of the constructed tree.
std::shared_ptr<core::GameTreeNode> build_kuhn_game_tree();


// --- Helper: Kuhn Range ---
// Creates the initial uniform range {J, Q, K} for Kuhn Poker.
// Note: Uses the base PrivateCards struct, but only card1_int matters.
std::vector<core::PrivateCards> get_kuhn_initial_range();

} // namespace kuhn
} // namespace poker_solver

#endif // POKER_SOLVER_KUHN_KUHN_POKER_SETUP_H_
