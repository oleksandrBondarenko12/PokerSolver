#ifndef POKER_SOLVER_SOLVER_PCFRSOLVER_H_
#define POKER_SOLVER_SOLVER_PCFRSOLVER_H_

#include "solver/Solver.h"          // Base class
#include "GameTree.h"               // For tree structure (ensure correct path)
#include "nodes/GameTreeNode.h"     // Node types and enums
#include "ranges/PrivateCardsManager.h" // Range management
#include "ranges/RiverRangeManager.h"   // River evaluation management
#include "compairer/Compairer.h"        // Hand evaluation
#include "Deck.h"                   // For deck info
#include "Card.h"                   // For Card utilities
#include "tools/Rule.h"             // For initial game state config

#include <vector>
#include <memory>
#include <string>
#include <atomic> // For stopping flag
#include <json.hpp> // Include actual json header

// Use alias defined in json.hpp
using json = nlohmann::json;

// Forward declarations to reduce header coupling
namespace poker_solver { namespace nodes { class ActionNode; } }
namespace poker_solver { namespace nodes { class ChanceNode; } }
namespace poker_solver { namespace nodes { class ShowdownNode; } }
namespace poker_solver { namespace nodes { class TerminalNode; } }
namespace poker_solver { namespace tree { class GameTree; } } // Ensure GameTree forward decl namespace matches definition

namespace poker_solver {
namespace solver {

// Concrete implementation of Solver using Discounted CFR.
class PCfrSolver : public Solver {
public:
    // Configuration for the solver
    struct Config {
        int iteration_limit = 1000;
        int num_threads = 1; // Default to single-threaded
        // bool use_isomorphism = false; // Future option
        // Add trainer type enum if needed (e.g., CFR+, DCFR)
        // Add precision enum if needed
    };

    // Constructor
    // Takes dependencies needed for solving. Rule provides initial state.
    PCfrSolver(std::shared_ptr<tree::GameTree> game_tree,
               std::shared_ptr<ranges::PrivateCardsManager> pcm,
               std::shared_ptr<ranges::RiverRangeManager> rrm,
               const config::Rule& rule,
               Config solver_config = {}); // Use default config if none provided

    // --- Solver Interface Implementation ---
    void Train() override;
    void Stop() override;
    json DumpStrategy(bool dump_evs, int max_depth = -1) const override;

private:
    // The core recursive CFR function
    // Returns utility vector from the perspective of the 'traverser'.
    std::vector<double> cfr_utility(
        std::shared_ptr<core::GameTreeNode> node,
        const std::vector<std::vector<double>>& reach_probs, // pi_i(h), pi_{-i}(h)
        int traverser,          // Player whose perspective we calculate utility for
        int iteration,
        uint64_t current_board_mask, // Pass board down
        double chance_reach);       // Probability of reaching this chance outcome

    // Helper function for Action Nodes within cfr_utility
    std::vector<double> cfr_action_node(
        std::shared_ptr<nodes::ActionNode> node,
        const std::vector<std::vector<double>>& reach_probs,
        int traverser,
        int iteration,
        uint64_t current_board_mask,
        double chance_reach);

    // Helper function for Chance Nodes within cfr_utility
    std::vector<double> cfr_chance_node(
        std::shared_ptr<nodes::ChanceNode> node,
        const std::vector<std::vector<double>>& reach_probs,
        int traverser,
        int iteration,
        uint64_t current_board_mask,
        double parent_chance_reach); // Renamed for clarity

    // Helper function for Showdown Nodes within cfr_utility
    std::vector<double> cfr_showdown_node(
        std::shared_ptr<nodes::ShowdownNode> node,
        const std::vector<std::vector<double>>& reach_probs,
        int traverser,
        uint64_t final_board_mask,
        double chance_reach); // Pass chance reach for correct weighting

    // Helper function for Terminal Nodes within cfr_utility
    std::vector<double> cfr_terminal_node(
        std::shared_ptr<nodes::TerminalNode> node,
        const std::vector<std::vector<double>>& reach_probs,
        int traverser,
        double chance_reach); // Pass chance reach for correct weighting

    // Helper to recursively dump strategy from the tree
    json dump_strategy_recursive(
        const std::shared_ptr<core::GameTreeNode>& node,
        bool dump_evs,
        int current_depth,
        int max_depth) const;


    // --- Member Variables ---
    std::shared_ptr<ranges::PrivateCardsManager> pcm_;
    std::shared_ptr<ranges::RiverRangeManager> rrm_;
    uint64_t initial_board_mask_;
    core::Deck deck_; // Need deck access for chance nodes
    Config config_;
    std::atomic<bool> stop_signal_{false};
    bool evs_calculated_ = false; // Track if final EVs are computed
    const size_t num_players_ = 2; // Hardcoded for now
};

} // namespace solver
} // namespace poker_solver

#endif // POKER_SOLVER_SOLVER_PCFRSOLVER_H_
