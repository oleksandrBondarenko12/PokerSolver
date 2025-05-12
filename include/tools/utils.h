#ifndef POKER_SOLVER_UTILS_UTILS_H_
#define POKER_SOLVER_UTILS_UTILS_H_

#include "ranges/PrivateCards.h" // For PrivateCards
#include "Card.h"          // For Card utilities
#include <vector>
#include <stdexcept> // For std::runtime_error, std::invalid_argument, std::out_of_range
#include <numeric>   // For std::iota (potentially useful for index mapping)
#include <algorithm> // For std::swap
#include <unordered_map> // For hash map
#include <functional>    // For std::hash

namespace poker_solver {
namespace utils {

// Exchanges elements in a value vector based on suit isomorphism.
template <typename T>
void ExchangeColorIsomorphism(std::vector<T>& value,
                              const std::vector<core::PrivateCards>& range,
                              int suit_index1, int suit_index2) {

    if (value.size() != range.size()) {
        throw std::invalid_argument(
            "Value vector size must match range vector size in ExchangeColorIsomorphism.");
    }
    if (suit_index1 < 0 || suit_index1 >= core::kNumSuits ||
        suit_index2 < 0 || suit_index2 >= core::kNumSuits) {
         throw std::out_of_range("Invalid suit index provided to ExchangeColorIsomorphism.");
    }
    if (suit_index1 == suit_index2 || value.empty()) {
        return;
    }

    size_t range_size = range.size();
    // Map from the PrivateCards object itself back to its original index.
    std::unordered_map<core::PrivateCards, size_t, std::hash<core::PrivateCards>>
        hand_to_original_index;
    hand_to_original_index.reserve(range_size);

    for (size_t i = 0; i < range_size; ++i) {
         // Map the hand object to its first encountered index
         hand_to_original_index.try_emplace(range[i], i);
    }

    // Vector to track which indices have already been swapped.
    // Needs to be outside the loop to persist across iterations of i.
    std::vector<bool> swapped(range_size, false);

    for (size_t i = 0; i < range_size; ++i) {
        if (swapped[i]) {
            continue;
        }

        const core::PrivateCards& original_hand = range[i];
        int c1_orig = original_hand.Card1Int();
        int c2_orig = original_hand.Card2Int();

        // Create the isomorphic hand by swapping suits
        int c1_iso = c1_orig;
        int c2_iso = c2_orig;

        if (c1_orig % core::kNumSuits == suit_index1) c1_iso = c1_orig - suit_index1 + suit_index2;
        else if (c1_orig % core::kNumSuits == suit_index2) c1_iso = c1_orig - suit_index2 + suit_index1;

        if (c2_orig % core::kNumSuits == suit_index1) c2_iso = c2_orig - suit_index1 + suit_index2;
        else if (c2_orig % core::kNumSuits == suit_index2) c2_iso = c2_orig - suit_index2 + suit_index1;

        if (c1_iso == c1_orig && c2_iso == c2_orig) {
            swapped[i] = true; // Mark as processed even if no swap needed
            continue;
        }

        core::PrivateCards isomorphic_hand;
        try {
             isomorphic_hand = core::PrivateCards(c1_iso, c2_iso, original_hand.Weight());
        } catch (const std::invalid_argument& ) {
             // This can happen if c1_iso == c2_iso after suit swapping,
             // which means the original hand didn't have a distinct isomorphic partner
             // with these particular suit swaps (e.g. swapping c->d on AcAd when board doesn't block).
             // Or if the resulting card ints become invalid (less likely with just suit swap).
             swapped[i] = true;
             continue;
        }

        // Find the original index of this isomorphic hand using the object map
        auto it = hand_to_original_index.find(isomorphic_hand);

        if (it != hand_to_original_index.end()) {
            size_t j = it->second; // Index of the isomorphic partner hand

            // Only swap if i < j to handle duplicates cleanly and ensure each pair is swapped once.
            // And ensure partner hasn't been involved in a swap initiated by itself (covered by swapped[j]).
            if (i < j && !swapped[j]) {
                 // Swap the corresponding values in the input vector
                std::swap(value[i], value[j]);
                // Mark both indices as swapped
                swapped[i] = true;
                swapped[j] = true;
            } else if (j < i && !swapped[j]) {
                // This case means 'j' should have initiated the swap with 'i'.
                // If swapped[j] is false, it means it hasn't been processed yet.
                // To ensure the i < j rule is the primary driver, we can let j handle it.
                // However, to be safe and avoid re-swapping if j later processes i,
                // we can swap here and mark both. Or, rely on the i < j condition strictly.
                // The current logic: if i < j, i initiates. If j < i, j initiates.
                // If we reach here with j < i and !swapped[j], it means j hasn't run yet.
                // The current `swapped[i] = true` at the end of the outer loop handles this correctly
                // by ensuring `i` isn't processed again if `j` later swaps with it.
                // The `if (i < j && !swapped[j])` is the most straightforward.
                // If we are at `i` and its partner `j` has `j < i`, then `j` should have already
                // processed and swapped with `i`. If `swapped[j]` is false, something is off or
                // `j` just hasn't been reached yet in the outer loop.
                // The key is that one of (i,j) or (j,i) will satisfy the i < j (or j < i) condition first.
                // Let's stick to `i < j` as the sole initiator condition for simplicity.
                // If `j < i`, then when the outer loop reaches `j`, it will find `i` as its partner
                // and `j < i` will be true, leading to a swap.
                // So, if `i < j` is false, we just mark `i` as swapped to prevent it from
                // being considered again if its partner `j` (where `j > i`) hasn't run yet.
                swapped[i] = true;
            } else {
                 // If i == j (should be caught by c1_iso == c1_orig && c2_iso == c2_orig), or
                 // if j was already swapped (swapped[j] is true),
                 // or if i > j and j was already processed (so swapped[j] might be true or i was its partner)
                 // just mark i as processed.
                 swapped[i] = true;
            }
        } else {
            // Isomorphic partner not found in the range. Mark current as processed.
             swapped[i] = true;
        }
    }
}


} // namespace utils
} // namespace poker_solver

#endif // POKER_SOLVER_UTILS_UTILS_H_
