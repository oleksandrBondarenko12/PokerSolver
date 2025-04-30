#include "ranges/PrivateCardsManager.h" // Adjust path if necessary

#include <stdexcept>
#include <sstream>
#include <vector>
#include <numeric> // For std::accumulate
#include <iostream>
#include <optional>

// Use aliases for namespaces
namespace core = poker_solver::core;

namespace poker_solver {
namespace ranges {

PrivateCardsManager::PrivateCardsManager(
    std::vector<std::vector<core::PrivateCards>> initial_ranges,
    uint64_t initial_board_mask)
    : num_players_(initial_ranges.size()),
      initial_board_mask_(initial_board_mask),
      player_ranges_(std::move(initial_ranges)),
      initial_reach_probs_(num_players_) {

    if (num_players_ == 0) {
        throw std::invalid_argument("Initial ranges cannot be empty.");
    }
    // Currently, relative probability calculation only supports 2 players.
    if (num_players_ != 2) {
         throw std::invalid_argument(
            "PrivateCardsManager currently only supports exactly 2 players "
            "for initial reach probability calculation.");
    }

    // --- Build the Hand Hash Lookup Table ---
    std::hash<core::PrivateCards> hasher; // Get the hasher for PrivateCards
    for (size_t player_idx = 0; player_idx < num_players_; ++player_idx) {
        const auto& range = player_ranges_[player_idx];
        for (size_t hand_idx = 0; hand_idx < range.size(); ++hand_idx) {
            const core::PrivateCards& hand = range[hand_idx];
            std::size_t hand_hash = hasher(hand);

            // Ensure map entry exists for this hash
            if (hand_hash_to_indices_.find(hand_hash) == hand_hash_to_indices_.end()) {
                // Initialize vector for this hash with nullopt for all players
                hand_hash_to_indices_[hand_hash] =
                    std::vector<std::optional<size_t>>(num_players_, std::nullopt);
            }

            // Check for duplicate hands within the same player's range (optional)
            if (hand_hash_to_indices_[hand_hash][player_idx].has_value()) {
                 std::ostringstream oss;
                 oss << "Duplicate hand found in range for player " << player_idx
                     << " at index " << hand_idx << " and "
                     << hand_hash_to_indices_[hand_hash][player_idx].value()
                     << " (Hand: " << hand.ToString() << ")";
                 // Depending on requirements, could throw or just warn
                 // throw std::invalid_argument(oss.str());
                 std::cerr << "[WARNING] " << oss.str() << std::endl;
            }

            // Store the index for this player and hand hash
            hand_hash_to_indices_[hand_hash][player_idx] = hand_idx;
        }
    }

    // --- Calculate Initial Reach Probabilities ---
    CalculateInitialReachProbs();
}

const std::vector<core::PrivateCards>& PrivateCardsManager::GetPlayerRange(
    size_t player_index) const {
    if (player_index >= num_players_) {
        std::ostringstream oss;
        oss << "Invalid player index: " << player_index << ". Must be less than "
            << num_players_ << ".";
        throw std::out_of_range(oss.str());
    }
    return player_ranges_[player_index];
}

std::optional<size_t> PrivateCardsManager::GetOpponentHandIndex(
    size_t from_player_index,
    size_t to_player_index,
    size_t from_hand_index) const {

    if (from_player_index >= num_players_ || to_player_index >= num_players_ ||
        from_hand_index >= player_ranges_[from_player_index].size()) {
        return std::nullopt; // Invalid input indices
    }

    const core::PrivateCards& source_hand = player_ranges_[from_player_index][from_hand_index];
    std::hash<core::PrivateCards> hasher;
    std::size_t hand_hash = hasher(source_hand);

    auto it = hand_hash_to_indices_.find(hand_hash);
    if (it == hand_hash_to_indices_.end()) {
        return std::nullopt; // Hash not found (shouldn't happen if constructed correctly)
    }

    // Return the optional index stored for the target player
    return it->second[to_player_index];
}


// Calculates initial reach probabilities considering card removal.
// Assumes num_players_ == 2.
void PrivateCardsManager::CalculateInitialReachProbs() {
    if (num_players_ != 2) return; // Only implemented for 2 players

    std::vector<double> relative_prob_sums(num_players_, 0.0);

    for (size_t player_id = 0; player_id < num_players_; ++player_id) {
        size_t oppo_id = 1 - player_id;
        const auto& player_range = player_ranges_[player_id];
        const auto& oppo_range = player_ranges_[oppo_id];
        initial_reach_probs_[player_id].resize(player_range.size()); // Ensure size

        for (size_t i = 0; i < player_range.size(); ++i) {
            const core::PrivateCards& player_hand = player_range[i];
            uint64_t player_mask = player_hand.GetBoardMask();

            // Skip hands conflicting with the initial board
            if (core::Card::DoBoardsOverlap(player_mask, initial_board_mask_)) {
                initial_reach_probs_[player_id][i] = 0.0;
                continue;
            }

            double opponent_weight_sum = 0.0;
            for (const auto& oppo_hand : oppo_range) {
                uint64_t oppo_mask = oppo_hand.GetBoardMask();
                // Accumulate weight only if opponent hand is valid (no overlap)
                if (!core::Card::DoBoardsOverlap(oppo_mask, initial_board_mask_) &&
                    !core::Card::DoBoardsOverlap(oppo_mask, player_mask)) {
                    opponent_weight_sum += oppo_hand.Weight();
                }
            }

            // Relative probability = P(Hand) * Sum(P(Opponent Hands not blocked by Hand))
            double relative_prob = player_hand.Weight() * opponent_weight_sum;
            initial_reach_probs_[player_id][i] = relative_prob;
            relative_prob_sums[player_id] += relative_prob;
        }
    }

    // Normalize the probabilities for each player
    for (size_t player_id = 0; player_id < num_players_; ++player_id) {
        double total_sum = relative_prob_sums[player_id];
        if (total_sum > 0) { // Avoid division by zero
            for (double& prob : initial_reach_probs_[player_id]) {
                prob /= total_sum;
            }
        } else {
             // Handle case where no hands are possible (e.g., board conflicts with all ranges)
             // Assign uniform probability (or handle as error?)
             size_t num_hands = initial_reach_probs_[player_id].size();
             if (num_hands > 0) {
                 double uniform_prob = 1.0 / static_cast<double>(num_hands);
                 for (double& prob : initial_reach_probs_[player_id]) {
                     // Only assign if it wasn't already zero due to board conflict
                     if (prob != 0.0) { // Check needed if we want to preserve 0 for board conflicts
                        prob = uniform_prob;
                     }
                 }
                 // Re-normalize just in case some were zeroed by board conflict
                 double actual_sum = std::accumulate(initial_reach_probs_[player_id].begin(),
                                                     initial_reach_probs_[player_id].end(), 0.0);
                 if (actual_sum > 0) {
                     for (double& prob : initial_reach_probs_[player_id]) {
                         prob /= actual_sum;
                     }
                 }
             }
        }
    }
}


const std::vector<double>& PrivateCardsManager::GetInitialReachProbs(
    size_t player_index) const {
    if (player_index >= num_players_) {
        std::ostringstream oss;
        oss << "Invalid player index: " << player_index << ". Must be less than "
            << num_players_ << ".";
        throw std::out_of_range(oss.str());
    }
    return initial_reach_probs_[player_index];
}


} // namespace ranges
} // namespace poker_solver
