///////////////////////////////////////////////////////////////////////////////
// include/solver/Solver.h (Correct Namespace - tree)
///////////////////////////////////////////////////////////////////////////////
#ifndef POKER_SOLVER_SOLVER_SOLVER_H_
#define POKER_SOLVER_SOLVER_SOLVER_H_

#include <memory> // For std::shared_ptr
#include <string>
#include <vector>
#include <json.hpp> // Include full header

// *** INCLUDE the full definition of GameTree using correct relative path ***
#include "GameTree.h" // Go up one directory from 'solver' to reach 'include/'

// Use alias
using json = nlohmann::json;

// *** Ensure correct namespace is used based on GameTree.h definition ***
// namespace poker_solver { namespace tree { class GameTree; } } // Forward declare in tree

namespace poker_solver {
namespace solver {

// Abstract base class (interface) for poker game solving algorithms (e.g., CFR variants).
class Solver {
 public:
  // Virtual destructor is essential for base classes.
  virtual ~Solver() = default;

  // Starts the training/solving process.
  virtual void Train() = 0;

  // Signals the solver to stop the training process prematurely.
  virtual void Stop() = 0;

  // Dumps the computed strategy (usually the average strategy) to a JSON object.
  virtual json DumpStrategy(bool dump_evs, int max_depth = -1) const = 0;

  // Returns a shared pointer to the game tree being solved.
  // *** Use the correct namespace 'tree' ***
  std::shared_ptr<tree::GameTree> GetGameTree() const { return game_tree_; }

 protected:
  // Constructor for derived classes.
  // *** Use the correct namespace 'tree' ***
  explicit Solver(std::shared_ptr<tree::GameTree> game_tree)
      : game_tree_(std::move(game_tree)) {}

  // Default constructor (protected).
  Solver() = default;

  // The game tree instance the solver operates on.
  // *** Use the correct namespace 'tree' ***
  std::shared_ptr<tree::GameTree> game_tree_;

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