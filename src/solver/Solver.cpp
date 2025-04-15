#include "Solver.h"
#include "GameTreeNode.h"
#include <iostream>

namespace PokerSolver {

// A simple dummy node type to serve as the root of our game tree.
// In a full implementation, the root would be built by the game tree builder.
class DummyNode : public GameTreeNode {
public:
    DummyNode() : GameTreeNode(GameRound::Preflop, 0.0, nullptr) {}
    NodeType type() const override { return NodeType::Action; }
    std::string nodeTypeToString() const override { return "Dummy"; }
};

Solver::Solver(const Rule& rule)
    : rule_(rule)
{
    // Initially, the game tree is uninitialized.
    gameTree_ = nullptr;
}

void Solver::initialize() {
    // Build the game tree from the rule.
    // Here we create a dummy root node—this should be replaced with a proper game tree
    // builder that uses the Rule and various StreetSettings.
    std::shared_ptr<GameTreeNode> root = std::make_shared<DummyNode>();

    // Construct the GameTree container using the root node.
    gameTree_ = std::make_shared<GameTree>(root);

    std::cout << "Solver initialized: Game tree created with root type: " 
              << root->nodeTypeToString() << "\n";
}

void Solver::run(int iterations) {
    if (!gameTree_) {
        std::cerr << "Error: Game tree is uninitialized. Call initialize() before running the solver.\n";
        return;
    }
    std::cout << "Running solver for " << iterations << " iterations...\n";

    // In a full solver implementation, each iteration would:
    //   - Traverse the game tree.
    //   - Update regrets and strategies via the Trainable modules.
    //   - Possibly perform sampling and counterfactual calculations.
    // Here we simulate the iterative process.
    for (int i = 0; i < iterations; ++i) {
        // Placeholder: in reality, we would call an iterate() method here.
        std::cout << "Iteration " << (i + 1) << " completed.\n";
    }
    std::cout << "Solver run completed.\n";
}

void Solver::exportStrategies() const {
    // In a complete implementation, this method would traverse the game tree,
    // extract the average strategy from each node (by calling dumpStrategy on Trainable objects),
    // and then output the cumulative strategy (for instance, to JSON).
    // For now, we display a placeholder message.
    std::cout << "Exporting strategies (not implemented in this simplified version).\n";
}

} // namespace PokerSolver
