#ifndef POKER_SOLVER_SOLVER_SOLVER_H_
#define POKER_SOLVER_SOLVER_SOLVER_H_

#include <memory> // For std::shared_ptr
#include <string>
#include <vector>
#include <json.hpp> // <<< INCLUDE full header instead of forward declaring

// Forward declarations
namespace poker_solver { namespace core { class GameTree; } }
// namespace nlohmann { class json; } // Forward declare json if used
using json = nlohmann::json;       // Bring into scope if used

namespace poker_solver {
namespace solver {

// Abstract base class (interface) for poker game solving algorithms (e.g., CFR variants).
class Solver {
 public:
  // Virtual destructor is essential for base classes.
  virtual ~Solver() = default;

  // Starts the training/solving process.
  // This will typically run for a specified number of iterations or until
  // a convergence criterion (like exploitability) is met.
  virtual void Train() = 0;

  // Signals the solver to stop the training process prematurely.
  // Useful for iterative algorithms running in a separate thread or loop.
  virtual void Stop() = 0;

  // Dumps the computed strategy (usually the average strategy) to a JSON object.
  // Args:
  //   dump_evs: If true, include calculated expected values alongside the strategy.
  //   max_depth: Optional maximum tree depth to dump (e.g., only up to the turn).
  //              A negative value typically means dump the entire relevant tree.
  // Returns:
  //   A json object representing the strategy and potentially EVs.
  virtual json DumpStrategy(bool dump_evs, int max_depth = -1) const = 0;

  // Returns a shared pointer to the game tree being solved.
  std::shared_ptr<core::GameTree> GetGameTree() const { return game_tree_; }

 protected:
  // Constructor for derived classes.
  // Args:
  //   game_tree: A shared pointer to the game tree to be solved.
  explicit Solver(std::shared_ptr<core::GameTree> game_tree)
      : game_tree_(std::move(game_tree)) {}

  // Default constructor (protected).
  Solver() = default;

  // The game tree instance the solver operates on.
  std::shared_ptr<core::GameTree> game_tree_;

 private:
  // Deleted copy/move operations to prevent slicing and enforce ownership semantics.
  Solver(const Solver&) = delete;
  Solver& operator=(const Solver&) = delete;
  Solver(Solver&&) = delete;
  Solver& operator=(Solver&&) = delete;
};

} // namespace solver
} // namespace poker_solver

#endif // POKER_SOLVER_SOLVER_SOLVER_H_
