#include "trainable/DiscountedCfrTrainable.h" // Adjust path if necessary
#include "nodes/ActionNode.h" // Need full definition now
#include "nodes/GameActions.h" // For dumping action strings
#include "ranges/PrivateCards.h"

#include <json.hpp> // Include the actual JSON library header
#include <vector>
#include <cmath>     // For std::pow, std::max
#include <numeric>   // For std::accumulate
#include <stdexcept> // For exceptions
#include <limits>    // For numeric_limits
#include <iostream>  // For potential debug/error output
#include <utility>   // For std::move
#include <typeinfo> // For dynamic_cast

// Use aliases
using json = nlohmann::json;
namespace core = poker_solver::core;
namespace nodes = poker_solver::nodes;


namespace poker_solver {
namespace solver {

// --- Constructor ---
DiscountedCfrTrainable::DiscountedCfrTrainable(
    const std::vector<core::PrivateCards>* player_range,
    const nodes::ActionNode& action_node)
    : action_node_(action_node),
      player_range_(player_range) {

    if (!player_range_) {
        throw std::invalid_argument("Player range pointer cannot be null.");
    }

    // Use GetActions() method from ActionNode
    num_actions_ = action_node_.GetActions().size();
    num_hands_ = player_range_->size();

    // Handle nodes with zero actions or hands gracefully
    if (num_actions_ == 0) {
        // std::cerr << "[WARNING] DiscountedCfrTrainable created for ActionNode with 0 actions." << std::endl;
    }
    if (num_hands_ == 0) {
        // std::cerr << "[WARNING] DiscountedCfrTrainable created with 0 hands in range." << std::endl;
    }

    size_t total_size = num_actions_ * num_hands_;
    cumulative_regrets_.resize(total_size, 0.0f);
    cumulative_strategy_sum_.resize(total_size, 0.0); // Initialize sums to 0

    // Initialize strategies to uniform (handle division by zero if num_actions_ is 0)
    float initial_prob = (num_actions_ > 0) ? 1.0f / static_cast<float>(num_actions_) : 0.0f;
    current_strategy_.resize(total_size, initial_prob);
    average_strategy_.resize(total_size, initial_prob);

    // Initialize EVs to NaN to indicate they haven't been set
    expected_values_.resize(total_size, std::numeric_limits<float>::quiet_NaN());

    // Initial strategies are considered valid
    current_strategy_valid_ = true;
    average_strategy_valid_ = true;
}


// --- Strategy Calculation Helpers ---

// Calculates the current strategy profile based on positive regrets.
void DiscountedCfrTrainable::CalculateCurrentStrategy() {
    if (num_actions_ == 0 || num_hands_ == 0) {
        current_strategy_valid_ = true;
        return; // Nothing to calculate
    }

    // No need to reset, we overwrite below
    // current_strategy_.assign(num_actions_ * num_hands_, 0.0f);
    float default_prob = 1.0f / static_cast<float>(num_actions_);

    for (size_t h = 0; h < num_hands_; ++h) {
        double regret_sum = 0.0; // Use double for sum to maintain precision
        // Calculate sum of positive regrets for this hand
        for (size_t a = 0; a < num_actions_; ++a) {
            size_t index = a * num_hands_ + h;
            // Ensure index is valid (should be, but good practice)
            if (index < cumulative_regrets_.size()) {
                 regret_sum += std::max(0.0f, cumulative_regrets_[index]);
            }
        }

        // Calculate strategy for this hand
        for (size_t a = 0; a < num_actions_; ++a) {
            size_t index = a * num_hands_ + h;
             if (index < cumulative_regrets_.size()) { // Check index again
                if (regret_sum > 1e-9) { // Use a slightly larger tolerance for float sums
                    current_strategy_[index] =
                        std::max(0.0f, cumulative_regrets_[index]) / static_cast<float>(regret_sum);
                } else {
                    // Default to uniform strategy if all regrets are non-positive
                    current_strategy_[index] = default_prob;
                }
             } else {
                 // Should not happen if logic is correct
                 current_strategy_[index] = default_prob;
             }
        }
    }
    current_strategy_valid_ = true;
}

// Calculates the average strategy profile based on cumulative strategy sums.
void DiscountedCfrTrainable::CalculateAverageStrategy() const {
     if (num_actions_ == 0 || num_hands_ == 0) {
        average_strategy_valid_ = true;
        return; // Nothing to calculate
    }

    // No need to reset, we overwrite below
    // average_strategy_.assign(num_actions_ * num_hands_, 0.0f);
    float default_prob = 1.0f / static_cast<float>(num_actions_);

    for (size_t h = 0; h < num_hands_; ++h) {
        double strategy_sum_total = 0.0;
        // Calculate total strategy sum for this hand
        for (size_t a = 0; a < num_actions_; ++a) {
             size_t index = a * num_hands_ + h;
             if (index < cumulative_strategy_sum_.size()) { // Index check
                 strategy_sum_total += cumulative_strategy_sum_[index];
             }
        }

        // Calculate average strategy for this hand
        for (size_t a = 0; a < num_actions_; ++a) {
            size_t index = a * num_hands_ + h;
             if (index < cumulative_strategy_sum_.size()) { // Index check
                 if (strategy_sum_total > 1e-9) { // Use tolerance
                     average_strategy_[index] =
                        static_cast<float>(cumulative_strategy_sum_[index] / strategy_sum_total);
                 } else {
                     // Default to uniform if sum is zero (can happen early on)
                     average_strategy_[index] = default_prob;
                 }
             } else {
                 average_strategy_[index] = default_prob;
             }
        }
    }
    average_strategy_valid_ = true;
}


// --- Overridden Interface Methods ---

const std::vector<float>& DiscountedCfrTrainable::GetCurrentStrategy() const {
    // Lazily calculate if needed
    if (!current_strategy_valid_) {
        // Need to cast away const to call the non-const calculation method.
        // This is a common pattern for lazy evaluation in const methods.
        const_cast<DiscountedCfrTrainable*>(this)->CalculateCurrentStrategy();
    }
    return current_strategy_;
}

const std::vector<float>& DiscountedCfrTrainable::GetAverageStrategy() const {
    // Lazily calculate if needed
    if (!average_strategy_valid_) {
        CalculateAverageStrategy(); // This method is const
    }
    return average_strategy_;
}

void DiscountedCfrTrainable::UpdateRegrets(const std::vector<float>& regrets,
                                           int iteration,
                                           const std::vector<float>& reach_probs) {
    size_t total_size = num_actions_ * num_hands_;
    if (regrets.size() != total_size) {
         throw std::invalid_argument("Regret vector size mismatch in UpdateRegrets.");
    }
     // Allow reach_probs to be empty if not used by specific DCFR variant,
     // but check size if provided.
     if (!reach_probs.empty() && reach_probs.size() != num_hands_) {
          throw std::invalid_argument("Reach probability vector size mismatch in UpdateRegrets.");
     }
     // If reach_probs is empty, we need a default vector of 1.0s if the algorithm uses it
     const std::vector<float>* rp_ptr = &reach_probs;
     std::vector<float> default_reach_probs;
     if (reach_probs.empty()) {
         default_reach_probs.assign(num_hands_, 1.0f);
         rp_ptr = &default_reach_probs;
     }


    // Iteration must be positive for DCFR powers
    if (iteration <= 0) {
        // Allow iteration 0 for initialization maybe? Let's require positive.
        throw std::invalid_argument("Iteration number must be positive for DCFR.");
    }

    // Calculate discounting factors (using double for intermediate precision)
    double iter_d = static_cast<double>(iteration);
    double alpha_discount = std::pow(iter_d, kAlpha);
    alpha_discount = alpha_discount / (alpha_discount + 1.0);
    double beta_discount = std::pow(iter_d, kBeta);
    beta_discount = beta_discount / (beta_discount + 1.0);
    double gamma_discount_base = std::pow(iter_d / (iter_d + 1.0), kGamma);

    // Update cumulative regrets
    for (size_t i = 0; i < total_size; ++i) {
        // Apply discount factor based on sign of cumulative regret *before* adding current regret
        if (cumulative_regrets_[i] > 0) {
            cumulative_regrets_[i] *= static_cast<float>(alpha_discount);
        } else {
            cumulative_regrets_[i] *= static_cast<float>(beta_discount);
        }
        cumulative_regrets_[i] += regrets[i]; // Add current iteration's regret
    }

    // Update cumulative strategy sums
    // First, ensure current strategy is up-to-date based on new regrets
    CalculateCurrentStrategy(); // Recalculates based on updated regrets
    for (size_t h = 0; h < num_hands_; ++h) {
        // DCFR applies player's reach probability to strategy update
        // Note: Some DCFR versions apply opponent reach prob to regret update instead.
        // This implementation follows the strategy weighting approach.
        double weighted_gamma = gamma_discount_base * static_cast<double>((*rp_ptr)[h]);
        for (size_t a = 0; a < num_actions_; ++a) {
            size_t index = a * num_hands_ + h;
            // cumulative_strategy_sum_[index] *= kTheta; // Apply optional theta discount
            cumulative_strategy_sum_[index] += weighted_gamma * static_cast<double>(current_strategy_[index]);
        }
    }

    // Mark cached strategies as invalid (current was just updated, average needs recalc)
    current_strategy_valid_ = true;
    average_strategy_valid_ = false;
}

void DiscountedCfrTrainable::SetEv(const std::vector<float>& evs) {
    if (evs.size() != expected_values_.size()) {
         throw std::invalid_argument("EV vector size mismatch in SetEv.");
    }
    expected_values_ = evs; // Copy the EV data
}

json DiscountedCfrTrainable::DumpStrategy(bool with_ev) const {
    json result;
    json strategy_map;
    // Ensure average strategy is calculated before dumping
    const auto& avg_strategy = GetAverageStrategy();

    if (!player_range_) {
        result["error"] = "Player range not set";
        return result;
    }

    // Get action strings
    std::vector<std::string> action_strings;
    // Use const reference from action_node_
    for(const auto& action : action_node_.GetActions()) {
        action_strings.push_back(action.ToString());
    }
    result["actions"] = action_strings;


    for (size_t h = 0; h < num_hands_; ++h) {
        // Use const reference and check pointer validity
        const core::PrivateCards& hand = (*player_range_)[h];
        std::string hand_str = hand.ToString();
        std::vector<float> hand_strategy(num_actions_);
        for (size_t a = 0; a < num_actions_; ++a) {
             size_t index = a * num_hands_ + h;
             if (index < avg_strategy.size()) { // Bounds check
                hand_strategy[a] = avg_strategy[index];
             } else {
                 hand_strategy[a] = std::numeric_limits<float>::quiet_NaN(); // Indicate error
             }
        }
        strategy_map[hand_str] = hand_strategy;
    }
    result["strategy"] = strategy_map;

    if (with_ev) {
        // Check if EVs have been set (are not all NaN) before dumping
        bool evs_set = false;
        for(float ev : expected_values_) {
            if (!std::isnan(ev)) {
                evs_set = true;
                break;
            }
        }
        if (evs_set) {
            result["evs"] = DumpEvs()["evs"]; // Reuse DumpEvs logic
        } else {
             result["evs"] = "Not calculated";
        }
    }

    return result;
}

json DiscountedCfrTrainable::DumpEvs() const {
    json result;
    json ev_map;

     if (!player_range_) {
        result["error"] = "Player range not set";
        return result;
    }

    // Get action strings
    std::vector<std::string> action_strings;
     for(const auto& action : action_node_.GetActions()) {
        action_strings.push_back(action.ToString());
    }
    result["actions"] = action_strings;

    for (size_t h = 0; h < num_hands_; ++h) {
        const core::PrivateCards& hand = (*player_range_)[h];
        std::string hand_str = hand.ToString();
        std::vector<float> hand_evs(num_actions_);
        for (size_t a = 0; a < num_actions_; ++a) {
             size_t index = a * num_hands_ + h;
             if (index < expected_values_.size()) { // Bounds check
                hand_evs[a] = expected_values_[index];
             } else {
                  hand_evs[a] = std::numeric_limits<float>::quiet_NaN(); // Indicate error
             }
        }
        ev_map[hand_str] = hand_evs;
    }
    result["evs"] = ev_map;

    return result;
}


// --- Implementation for CopyStateFrom ---
void DiscountedCfrTrainable::CopyStateFrom(const Trainable& other) {
    // Use dynamic_cast to safely check if 'other' is the same type
    // Cast to const pointer first
    const auto* other_dcfr_ptr = dynamic_cast<const DiscountedCfrTrainable*>(&other);
    if (!other_dcfr_ptr) {
        // Throw if the type is not compatible
        throw std::invalid_argument(
            "Cannot copy state: 'other' is not a DiscountedCfrTrainable.");
    }
    // Now safe to use other_dcfr_ptr
    const DiscountedCfrTrainable& other_dcfr = *other_dcfr_ptr;

    // Check dimensions are compatible.
    if (num_actions_ != other_dcfr.num_actions_ || num_hands_ != other_dcfr.num_hands_) {
         throw std::invalid_argument(
            "Cannot copy state: Dimensions mismatch between Trainable objects.");
    }

    // Copy the relevant state vectors
    this->cumulative_regrets_ = other_dcfr.cumulative_regrets_;
    this->cumulative_strategy_sum_ = other_dcfr.cumulative_strategy_sum_;
    this->current_strategy_ = other_dcfr.current_strategy_;
    this->average_strategy_ = other_dcfr.average_strategy_;
    this->current_strategy_valid_ = other_dcfr.current_strategy_valid_;
    this->average_strategy_valid_ = other_dcfr.average_strategy_valid_;
    this->expected_values_ = other_dcfr.expected_values_; // Copy EVs
}


} // namespace solver
} // namespace poker_solver
