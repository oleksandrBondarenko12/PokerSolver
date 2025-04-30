#include "ranges/RiverRangeManager.h" // Adjust path if necessary
// Include the header where kInvalidRank is defined (likely the concrete compairer)
// If Dic5Compairer is the only one, include it. If others might exist,
// consider moving kInvalidRank to the base Compairer interface.
#include "compairer/Dic5Compairer.h" // Assuming kInvalidRank is here for now

#include <stdexcept>
#include <sstream>
#include <vector>
#include <algorithm> // For std::sort
#include <mutex>     // For std::lock_guard
#include <utility>   // For std::move
#include <bit>       // For std::popcount (C++20) - include if using

// Use aliases for namespaces
namespace core = poker_solver::core;
// Use eval namespace specifically for kInvalidRank constant
namespace eval = poker_solver::eval;


namespace poker_solver {
namespace ranges {

// --- Constructor ---

RiverRangeManager::RiverRangeManager(std::shared_ptr<core::Compairer> compairer)
    : compairer_(std::move(compairer)) { // Use std::move
    if (!compairer_) {
        throw std::invalid_argument("Compairer cannot be null.");
    }
}

// --- Public Methods ---

const std::vector<RiverCombs>& RiverRangeManager::GetRiverCombos(
    size_t player_index,
    const std::vector<core::PrivateCards>& initial_player_range,
    uint64_t river_board_mask) {

    // Select the appropriate cache and mutex based on player index
    // Use references for cache and mutex
    auto& cache = (player_index == 0) ? player_0_cache_ : player_1_cache_;
    auto& cache_mutex = (player_index == 0) ? player_0_cache_mutex_ : player_1_cache_mutex_;

    if (player_index > 1) { // Basic check for 2 players
         std::ostringstream oss;
         oss << "Invalid player index for RiverRangeManager: " << player_index;
         throw std::out_of_range(oss.str());
    }

    // --- Cache Lookup (Thread-Safe) ---
    { // Scope for lock guard
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto it = cache.find(river_board_mask);
        if (it != cache.end()) {
            // Found in cache, return it
            return it->second;
        }
    } // Lock released here

    // --- Not found in cache, calculate it ---
    return CalculateAndCacheRiverCombos(player_index, initial_player_range,
                                        river_board_mask);
}

// Overload for vector<int> board input
const std::vector<RiverCombs>& RiverRangeManager::GetRiverCombos(
     size_t player_index,
     const std::vector<core::PrivateCards>& initial_player_range,
     const std::vector<int>& river_board_ints) {

    // Convert board vector to mask, includes validation via CardIntsToUint64
    uint64_t river_board_mask;
    try {
         river_board_mask = core::Card::CardIntsToUint64(river_board_ints);
    } catch (const std::out_of_range& e) {
         std::ostringstream oss;
         oss << "Invalid card integer in river board vector: " << e.what();
         throw std::invalid_argument(oss.str());
    }
    // Delegate to the mask version
    return GetRiverCombos(player_index, initial_player_range, river_board_mask);
}


// --- Private Calculation and Caching Method ---

const std::vector<RiverCombs>& RiverRangeManager::CalculateAndCacheRiverCombos(
    size_t player_index,
    const std::vector<core::PrivateCards>& initial_player_range,
    uint64_t river_board_mask) {

    // Validate board mask represents 5 cards
    int pop_count = 0;
    // Use standard library popcount if available (C++20) or __builtin if acceptable
    // #include <bit> // For C++20 std::popcount
    // pop_count = std::popcount(river_board_mask);
    // Or use __builtin_popcountll for GCC/Clang
    #if defined(__cpp_lib_bitops) // Check for C++20 <bit> header
        pop_count = std::popcount(river_board_mask);
    #elif defined(__GNUC__) || defined(__clang__) // Check for GCC/Clang builtins
        pop_count = __builtin_popcountll(river_board_mask);
    #else
        // Manual popcount as fallback
        uint64_t count_mask = river_board_mask;
        while(count_mask > 0) {
            count_mask &= (count_mask - 1);
            pop_count++;
        }
    #endif

    if (pop_count != 5) {
        std::ostringstream oss;
        oss << "River board mask must represent exactly 5 cards. Mask: 0x"
            << std::hex << river_board_mask << " has " << std::dec << pop_count << " cards.";
        throw std::invalid_argument(oss.str());
    }


    std::vector<RiverCombs> calculated_combos;
    calculated_combos.reserve(initial_player_range.size()); // Pre-allocate approx size

    for (size_t i = 0; i < initial_player_range.size(); ++i) {
        const core::PrivateCards& hand = initial_player_range[i];
        uint64_t private_mask = hand.GetBoardMask();

        // Skip hands that conflict with the river board
        if (core::Card::DoBoardsOverlap(private_mask, river_board_mask)) {
            continue;
        }

        // Evaluate the 7-card hand rank using the injected compairer
        int rank = compairer_->GetHandRank(private_mask, river_board_mask);

        // Only add combos that could be successfully ranked
        // (rank != kInvalidRank could be added here if desired)
        calculated_combos.emplace_back(hand, rank, i);
    }

    // Sort the results by rank.
    // NOTE: Sorting descending by rank number (worst hand first).
    std::sort(calculated_combos.begin(), calculated_combos.end(),
              [](const RiverCombs& lhs, const RiverCombs& rhs) {
                  // Treat invalid ranks as the absolute worst
                  // Use the constant from the concrete class for comparison
                  bool lhs_invalid = (lhs.rank == eval::Dic5Compairer::kInvalidRank || lhs.rank == -1);
                  bool rhs_invalid = (rhs.rank == eval::Dic5Compairer::kInvalidRank || rhs.rank == -1);

                  if (lhs_invalid && rhs_invalid) return false; // Equal (both invalid)
                  if (lhs_invalid) return true;  // Invalid is "greater" (worse) than valid
                  if (rhs_invalid) return false; // Valid is "less" (better) than invalid

                  // Both valid, sort descending by rank number (worst first)
                  return lhs.rank > rhs.rank;
              });


    // --- Cache Insertion (Thread-Safe) ---
    // Select cache and mutex again (needed because this is a separate method)
    auto& cache = (player_index == 0) ? player_0_cache_ : player_1_cache_;
    auto& cache_mutex = (player_index == 0) ? player_0_cache_mutex_ : player_1_cache_mutex_;

    { // Scope for lock guard
        std::lock_guard<std::mutex> lock(cache_mutex);
        // Use emplace for potential efficiency, returns pair<iterator, bool>
        // Move the calculated vector into the cache.
        auto result = cache.emplace(river_board_mask, std::move(calculated_combos));
        // Return a reference to the vector in the map (either newly inserted or
        // existing if another thread calculated and inserted it between our
        // initial check and this insertion point).
        return result.first->second;
    }
}


} // namespace ranges
} // namespace poker_solver
