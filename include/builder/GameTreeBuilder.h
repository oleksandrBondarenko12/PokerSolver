#ifndef POKERSOLVER_GAMETREEBUILDER_H
#define POKERSOLVER_GAMETREEBUILDER_H


#include <memory>
#include "GameTreeNode.h"
#include "tools/Rule.h"
#include "GameActions.h" // For constructing dummy action nodes


namespace PokerSolver {

/**
 * @brief The GameTreeBuilder is responsible for dynamically expanding
 * the game tree on demand. It uses game rules (through a Rule object)
 * to determine how to expand node (for example, moving from the preflop 
 * ActionNode to a chance node for the flop and so on).
 * 
 */

class GameTreeBuilder {
public:
    /// @brief Constructs a GameTreeBuilder with the provided game rule.
    explicit GameTreeBuilder(const Rule& rule);

    /// @brief Default destructor.
    ~GameTreeBuilder() = default;

    /// @brief Creates and returns the root node of the game tree based on the rule.
    std::shared_ptr<GameTreeNode> buildRoot();

    /// @brief Dynamically expands a given node based on its type and current
    /// game state.
    /// @param node The node to expand.
    void expandNode(std::shared_ptr<GameTreeNode> node);

private:
    /// The game rule that provides prarameters for the tree building.
    Rule rule_;

};

} // namespace PokerSolver

#endif // POKERSOLVER_GAMETREEBUILDER_H
