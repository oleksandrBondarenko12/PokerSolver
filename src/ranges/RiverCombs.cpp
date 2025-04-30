#include "ranges/RiverCombs.h" // Adjust path if necessary

namespace poker_solver {
namespace ranges {

// Default constructor implementation.
// Initializes with default PrivateCards and invalid rank/index.
RiverCombs::RiverCombs() = default;
// Note: Relying on default member initializers from the header.
// If specific default logic was needed, it would go here.


// Constructor implementation.
RiverCombs::RiverCombs(const core::PrivateCards& pc, int r,
                       size_t original_idx)
    : private_cards(pc), rank(r), original_range_index(original_idx) {
  // Initialization done via member initializer list.
}

} // namespace ranges
} // namespace poker_solver
