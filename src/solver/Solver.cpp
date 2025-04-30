#include "solver/Solver.h" // Adjust path if necessary
#include "GameTree.h" // Include full GameTree definition
#include <utility> // For std::move
#include <stdexcept> // For std::invalid_argument

namespace poker_solver {
namespace solver {

// --- Constructors ---

// Default constructor definition removed (already defaulted in header)
// Solver::Solver() = default;

// Constructor taking a GameTree definition removed (already defined via initializer list in header)
// Solver::Solver(std::shared_ptr<core::GameTree> game_tree)
//     : game_tree_(std::move(game_tree)) {
//     if (!game_tree_) {
//         // Optionally throw if a null tree is unacceptable
//         // throw std::invalid_argument("GameTree pointer cannot be null for Solver.");
//     }
// }

// Destructor implementation is defaulted in header.

// GetGameTree() is implemented inline in the header.

// Pure virtual functions (Train, Stop, DumpStrategy) have no implementation here
// as this is an abstract base class. Concrete solver implementations (like PCfrSolver)
// will provide these.

} // namespace solver
} // namespace poker_solver
