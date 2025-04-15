#ifndef POKERSOLVER_SOLVER_H
#define POKERSOLVER_SOLVER_H

#include <memory>
#include "tools/Rule.h"
#include "GameTree.h"

namespace PokerSolver {

/**
 * @brief The Solver class is the central driver for the poker solver.
 *
 * This class initializes the game tree from a given rule, runs the solver for a number
 * of iterations, and (in a more complete implementation) gathers and exports the computed strategies.
 */
class Solver {
public:
    /**
     * @brief Constructs a Solver with a given game rule configuration.
     * @param rule The game configuration encapsulated in a Rule object.
     */
    explicit Solver(const Rule& rule);

    /// Destructor (defaulted).
    ~Solver() = default;

    /**
     * @brief Initializes the solver by constructing the game tree.
     *
     * In a complete implementation, this method would invoke a more sophisticated
     * game tree building routine. For this example, we build a dummy tree with a single root node.
     */
    void initialize();

    /**
     * @brief Runs the solver for the specified number of iterations.
     * @param iterations The number of iterations to simulate.
     *
     * On each iteration, the solver would update the regret values and strategies
     * in the game tree. In our simplified version, we print out the iteration count.
     */
    void run(int iterations);

    /**
     * @brief Exports the computed strategies.
     *
     * In a complete implementation, this would extract the average strategy from each Trainable node,
     * and output the results (e.g. to a JSON object or file). Here, we simply print a message.
     */
    void exportStrategies() const;

private:
    Rule rule_;                                   ///< The game rule configuration.
    std::shared_ptr<GameTree> gameTree_;          ///< The game tree that will be built and solved.
};

} // namespace PokerSolver

#endif // POKERSOLVER_SOLVER_H
