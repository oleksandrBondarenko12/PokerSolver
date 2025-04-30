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

    // No need for swapped array with the i < j check below
    // std::vector<bool> swapped(range_size, false);

    for (size_t i = 0; i < range_size; ++i) {
        // If we already swapped this element when processing its partner (j < i), skip.
        // This check relies on the fact that we only swap when i < j.
        // However, the previous logic with the swapped array was likely more robust.
        // Let's stick to the swapped array logic but ensure it's used correctly.

        // Reintroducing swapped array logic:
        std::vector<bool> swapped(range_size, false); // Re-initialize inside? No, needs to persist.
                                                      // Let's put it back outside the loop.
    } // End of incorrect loop placement for swapped array init

    // Vector to track which indices have already been swapped. Needs to be outside loop.
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
             swapped[i] = true;
             continue;
        }

        // Find the original index of this isomorphic hand using the object map
        auto it = hand_to_original_index.find(isomorphic_hand);

        if (it != hand_to_original_index.end()) {
            size_t j = it->second; // Index of the isomorphic partner hand

            // *** Refined Check: Only swap if i < j to handle duplicates cleanly ***
            // And ensure partner hasn't been involved in a swap initiated by itself.
            if (i < j && !swapped[j]) {
                 // Swap the corresponding values in the input vector
                std::swap(value[i], value[j]);
                // Mark both indices as swapped
                swapped[i] = true;
                swapped[j] = true;
            } else {
                 // If i >= j (meaning j was processed earlier) or if j was already swapped,
                 // just mark i as processed to avoid redundant checks/swaps.
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
