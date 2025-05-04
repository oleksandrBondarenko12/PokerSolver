#include "trainable/DiscountedCfrTrainable.h" // Adjust path if necessary
#include "nodes/ActionNode.h"             // Need full definition for constructor
#include "nodes/GameActions.h"            // For dumping action strings
#include "ranges/PrivateCards.h"          // For PrivateCards info

#include <json.hpp> // Include the actual JSON library header
#include <vector>
#include <cmath>     // For std::pow, std::max, std::isnan
#include <numeric>   // For std::accumulate
#include <stdexcept> // For exceptions
#include <limits>    // For numeric_limits
#include <iostream>  // For potential debug/error output
#include <utility>   // For std::move
#include <typeinfo>  // For dynamic_cast
#include <algorithm> // For std::copy, std::fill
#include <iomanip>   // For json dumping precision (optional)


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
        throw std::invalid_argument("DiscountedCfrTrainable: Player range pointer cannot be null.");
    }

    num_actions_ = action_node_.GetActions().size();
    num_hands_ = player_range_->size();

    if (num_actions_ == 0) {
        std::cerr << "[WARN] DiscountedCfrTrainable created for ActionNode with 0 actions." << std::endl;
    }
    if (num_hands_ == 0) {
        std::cerr << "[WARN] DiscountedCfrTrainable created with 0 hands in range." << std::endl;
    }

    size_t total_size = num_actions_ * num_hands_;
    if (total_size > 0) {
        cumulative_regrets_.resize(total_size, 0.0f);
        cumulative_strategy_sum_.resize(total_size, 0.0); // Initialize sums to 0

        float initial_prob = 1.0f / static_cast<float>(num_actions_);
        current_strategy_.resize(total_size, initial_prob);
        average_strategy_.resize(total_size, initial_prob);
        expected_values_.resize(total_size, std::numeric_limits<float>::quiet_NaN());

        current_strategy_valid_ = true;
        average_strategy_valid_ = true;
    } else {
        // Handle case with 0 actions or 0 hands - vectors remain empty
        current_strategy_valid_ = true;
        average_strategy_valid_ = true;
    }
}


// --- Strategy Calculation Helpers ---

void DiscountedCfrTrainable::CalculateCurrentStrategy() {
    if (num_actions_ == 0 || num_hands_ == 0) {
        current_strategy_valid_ = true;
        return; // Nothing to calculate
    }

    float default_prob = 1.0f / static_cast<float>(num_actions_);
    size_t total_size = num_actions_ * num_hands_;

    // Ensure strategy vector has correct size
    if(current_strategy_.size() != total_size) {
        current_strategy_.resize(total_size);
    }

    for (size_t h = 0; h < num_hands_; ++h) {
        double regret_sum = 0.0;
        for (size_t a = 0; a < num_actions_; ++a) {
            size_t index = a * num_hands_ + h;
            regret_sum += std::max(0.0f, cumulative_regrets_[index]);
        }

        for (size_t a = 0; a < num_actions_; ++a) {
            size_t index = a * num_hands_ + h;
            if (regret_sum > 1e-9) {
                current_strategy_[index] =
                    std::max(0.0f, cumulative_regrets_[index]) / static_cast<float>(regret_sum);
            } else {
                current_strategy_[index] = default_prob;
            }
        }
    }
    current_strategy_valid_ = true;
}


void DiscountedCfrTrainable::CalculateAverageStrategy() const {
     if (num_actions_ == 0 || num_hands_ == 0) {
        average_strategy_valid_ = true;
        return;
    }

    float default_prob = 1.0f / static_cast<float>(num_actions_);
    size_t total_size = num_actions_ * num_hands_;

    // Ensure strategy vector has correct size
     if(average_strategy_.size() != total_size) {
        average_strategy_.resize(total_size);
    }

    for (size_t h = 0; h < num_hands_; ++h) {
        double strategy_sum_total = 0.0;
        for (size_t a = 0; a < num_actions_; ++a) {
             size_t index = a * num_hands_ + h;
             strategy_sum_total += cumulative_strategy_sum_[index];
        }

        for (size_t a = 0; a < num_actions_; ++a) {
            size_t index = a * num_hands_ + h;
             if (strategy_sum_total > 1e-9) {
                 average_strategy_[index] =
                    static_cast<float>(cumulative_strategy_sum_[index] / strategy_sum_total);
             } else {
                 average_strategy_[index] = default_prob;
             }
        }
    }
    average_strategy_valid_ = true;
}


// --- Overridden Interface Methods ---

const std::vector<float>& DiscountedCfrTrainable::GetCurrentStrategy() const {
    if (!current_strategy_valid_) {
        const_cast<DiscountedCfrTrainable*>(this)->CalculateCurrentStrategy();
    }
    return current_strategy_;
}

const std::vector<float>& DiscountedCfrTrainable::GetAverageStrategy() const {
    if (!average_strategy_valid_) {
        CalculateAverageStrategy();
    }
    return average_strategy_;
}

// *** Uses scalar weight for pi_{-i} * pi_c ***
void DiscountedCfrTrainable::UpdateRegrets(const std::vector<float>& regrets,
                                           int iteration,
                                           double reach_prob_opponent_chance_scalar) {
    size_t total_size = num_actions_ * num_hands_;
    if (regrets.size() != total_size) {
         throw std::invalid_argument("Regret vector size mismatch in UpdateRegrets.");
    }
    if (iteration <= 0) {
        throw std::invalid_argument("Iteration number must be positive for DCFR.");
    }
    double weight = std::max(0.0, reach_prob_opponent_chance_scalar); // Ensure non-negative

    // Calculate discounting factors
    double iter_d = static_cast<double>(iteration);
    double alpha_discount = std::pow(iter_d, kAlpha) / (std::pow(iter_d, kAlpha) + 1.0);
    double beta_discount = std::pow(iter_d, kBeta) / (std::pow(iter_d, kBeta) + 1.0);

    float cf_weight_float = static_cast<float>(weight);

    for (size_t i = 0; i < total_size; ++i) {
        // Apply discount factor based on sign *before* adding weighted regret
        if (cumulative_regrets_[i] > 0) {
            cumulative_regrets_[i] *= static_cast<float>(alpha_discount);
        } else {
            cumulative_regrets_[i] *= static_cast<float>(beta_discount);
        }
        // Weight the added regret by the scalar cf_weight
        cumulative_regrets_[i] += regrets[i] * cf_weight_float;
    }

    // Mark cached strategies as invalid
    current_strategy_valid_ = false;
    average_strategy_valid_ = false;
}

// *** Renamed & uses scalar weight for pi_i * pi_c ***
void DiscountedCfrTrainable::AccumulateAverageStrategy(
    const std::vector<float>& current_strategy,
    int iteration,
    double reach_prob_player_chance_scalar) {

    size_t total_size = num_actions_ * num_hands_;
     if (current_strategy.size() != total_size) {
         throw std::invalid_argument("Current strategy vector size mismatch in AccumulateAverageStrategy.");
     }
     if (iteration <= 0) {
        throw std::invalid_argument("Iteration number must be positive for DCFR.");
     }
     double weight = std::max(0.0, reach_prob_player_chance_scalar); // Ensure non-negative

     // Calculate discounting factor for strategy accumulation
     double iter_d = static_cast<double>(iteration);
     // Note: Original paper might use iteration^gamma or (iter/(iter+1))^gamma.
     // Using (iter/(iter+1))^gamma here for consistency with previous steps.
     double gamma_discount_factor = std::pow(iter_d / (iter_d + 1.0), kGamma);

     double final_weight = weight * gamma_discount_factor;

     for (size_t i = 0; i < total_size; ++i) {
         // Weight the added strategy by the scalar weight pi_i * pi_c * discount
         cumulative_strategy_sum_[i] += final_weight * static_cast<double>(current_strategy[i]);
     }

     // Mark average strategy as invalid since the sum changed
     average_strategy_valid_ = false;
}

void DiscountedCfrTrainable::SetEv(const std::vector<float>& evs) {
    size_t total_size = num_actions_ * num_hands_;
    if (evs.size() != total_size) {
         // Allow setting EVs even if sizes mismatch, but warn and resize?
         // Or throw? Let's throw for stricter checking.
         throw std::invalid_argument("EV vector size mismatch in SetEv. Expected "
              + std::to_string(total_size) + ", got " + std::to_string(evs.size()));
    }
    expected_values_ = evs;
}

json DiscountedCfrTrainable::DumpStrategy(bool with_ev) const {
    json result = json::object(); // Ensure it's an object
    json strategy_map = json::object();
    json ev_map = json::object();

    const auto& avg_strategy = GetAverageStrategy(); // Ensure calculated

    if (!player_range_) {
        result["error"] = "Player range not set";
        return result;
    }
    if (num_hands_ == 0) {
        result["warning"] = "Player range is empty";
        // Still return basic structure if possible
    }

    // Get action strings
    std::vector<std::string> action_strings;
    action_strings.reserve(num_actions_);
    for(const auto& action : action_node_.GetActions()) {
        action_strings.push_back(action.ToString());
    }
    result["actions"] = action_strings;


    for (size_t h = 0; h < num_hands_; ++h) {
        const core::PrivateCards& hand = (*player_range_)[h];
        std::string hand_str = hand.ToString();

        std::vector<float> hand_avg_strategy(num_actions_);
        std::vector<float> hand_evs(num_actions_, std::numeric_limits<float>::quiet_NaN());

        for (size_t a = 0; a < num_actions_; ++a) {
             size_t index = a * num_hands_ + h;
             if (index < avg_strategy.size()) {
                hand_avg_strategy[a] = avg_strategy[index];
             }
             if (with_ev && index < expected_values_.size()) {
                 hand_evs[a] = expected_values_[index];
             }
        }
        strategy_map[hand_str] = hand_avg_strategy;
        if(with_ev) {
            ev_map[hand_str] = hand_evs;
        }
    }
    result["strategy"] = strategy_map;

    if (with_ev) {
        result["evs"] = ev_map;
    }

    return result;
}

json DiscountedCfrTrainable::DumpEvs() const {
    json result = json::object();
    json ev_map = json::object();

     if (!player_range_) {
        result["error"] = "Player range not set";
        return result;
    }
     if (num_hands_ == 0) {
         result["warning"] = "Player range is empty";
     }

    // Get action strings
    std::vector<std::string> action_strings;
    action_strings.reserve(num_actions_);
     for(const auto& action : action_node_.GetActions()) {
        action_strings.push_back(action.ToString());
    }
    result["actions"] = action_strings;

    for (size_t h = 0; h < num_hands_; ++h) {
        const core::PrivateCards& hand = (*player_range_)[h];
        std::string hand_str = hand.ToString();
        std::vector<float> hand_evs(num_actions_, std::numeric_limits<float>::quiet_NaN());
        for (size_t a = 0; a < num_actions_; ++a) {
             size_t index = a * num_hands_ + h;
             if (index < expected_values_.size()) {
                hand_evs[a] = expected_values_[index];
             }
        }
        ev_map[hand_str] = hand_evs;
    }
    result["evs"] = ev_map;

    return result;
}


void DiscountedCfrTrainable::CopyStateFrom(const Trainable& other) {
    const auto* other_dcfr_ptr = dynamic_cast<const DiscountedCfrTrainable*>(&other);
    if (!other_dcfr_ptr) {
        throw std::invalid_argument("Cannot copy state: 'other' is not a DiscountedCfrTrainable.");
    }
    const DiscountedCfrTrainable& other_dcfr = *other_dcfr_ptr;

    if (num_actions_ != other_dcfr.num_actions_ || num_hands_ != other_dcfr.num_hands_) {
         throw std::invalid_argument("Cannot copy state: Dimensions mismatch.");
    }

    this->cumulative_regrets_ = other_dcfr.cumulative_regrets_;
    this->cumulative_strategy_sum_ = other_dcfr.cumulative_strategy_sum_;
    this->current_strategy_ = other_dcfr.current_strategy_;
    this->average_strategy_ = other_dcfr.average_strategy_;
    this->current_strategy_valid_ = other_dcfr.current_strategy_valid_;
    this->average_strategy_valid_ = other_dcfr.average_strategy_valid_;
    this->expected_values_ = other_dcfr.expected_values_;
}

} // namespace solver
} // namespace poker_solver