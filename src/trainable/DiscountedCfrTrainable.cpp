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
#include <iostream>  // *** For std::cout, std::cerr ***
#include <utility>   // For std::move
#include <typeinfo>  // For dynamic_cast
#include <algorithm> // For std::copy, std::fill
#include <iomanip>   // *** For std::fixed, std::setprecision ***


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
        cumulative_regrets_.resize(total_size, 0.0);
        cumulative_strategy_sum_.resize(total_size, 0.0);

        double initial_prob = (num_actions_ > 0) ? 1.0 / static_cast<double>(num_actions_) : 0.0;
        // Initialize Hand-Major: Iterate hands outer, actions inner
        current_strategy_.resize(total_size);
        average_strategy_.resize(total_size);
        for (size_t h = 0; h < num_hands_; ++h) {
            for (size_t a = 0; a < num_actions_; ++a) {
                size_t index = h * num_actions_ + a; // Hand-Major index
                current_strategy_[index] = initial_prob;
                average_strategy_[index] = initial_prob;
            }
        }
        expected_values_.resize(total_size, std::numeric_limits<double>::quiet_NaN());

        current_strategy_valid_ = true;
        average_strategy_valid_ = true;
    } else {
        current_strategy_valid_ = true;
        average_strategy_valid_ = true;
    }
}

// --- Strategy Calculation Helpers ---

void DiscountedCfrTrainable::CalculateCurrentStrategy() {
    if (current_strategy_valid_) return;

    if (num_actions_ == 0 || num_hands_ == 0) {
        current_strategy_.clear();
        current_strategy_valid_ = true;
        return;
    }

    double default_prob = (num_actions_ > 0) ? 1.0 / static_cast<double>(num_actions_) : 0.0;
    size_t total_size = num_actions_ * num_hands_;
    if(current_strategy_.size() != total_size) current_strategy_.resize(total_size);

    for (size_t h = 0; h < num_hands_; ++h) {
        double regret_sum = 0.0;
        for (size_t a = 0; a < num_actions_; ++a) {
            // *** FIXED INDEXING ***
            size_t index = h * num_actions_ + a; // Hand-Major index
            if(index < cumulative_regrets_.size()) {
                regret_sum += std::max(0.0, cumulative_regrets_[index]);
            }
        }

        for (size_t a = 0; a < num_actions_; ++a) {
             // *** FIXED INDEXING ***
            size_t index = h * num_actions_ + a; // Hand-Major index
            if (index < current_strategy_.size() && index < cumulative_regrets_.size()) {
                if (regret_sum > 1e-12) {
                    current_strategy_[index] =
                        std::max(0.0, cumulative_regrets_[index]) / regret_sum;
                } else {
                    current_strategy_[index] = default_prob;
                }
            }
        }
    }
    current_strategy_valid_ = true;
}



void DiscountedCfrTrainable::CalculateAverageStrategy() const {
    if (average_strategy_valid_) return;

    if (num_actions_ == 0 || num_hands_ == 0) {
        average_strategy_.clear();
        average_strategy_valid_ = true;
        return;
    }

    double default_prob = (num_actions_ > 0) ? 1.0 / static_cast<double>(num_actions_) : 0.0;
    size_t total_size = num_actions_ * num_hands_;
    if(average_strategy_.size() != total_size) average_strategy_.resize(total_size);

    for (size_t h = 0; h < num_hands_; ++h) {
        double strategy_sum_total = 0.0;
        for (size_t a = 0; a < num_actions_; ++a) {
            // *** FIXED INDEXING ***
             size_t index = h * num_actions_ + a; // Hand-Major index
             if (index < cumulative_strategy_sum_.size()) {
                 strategy_sum_total += cumulative_strategy_sum_[index];
             }
        }

        for (size_t a = 0; a < num_actions_; ++a) {
            // *** FIXED INDEXING ***
            size_t index = h * num_actions_ + a; // Hand-Major index
            if (index < average_strategy_.size() && index < cumulative_strategy_sum_.size()) {
                 if (strategy_sum_total > 1e-12) {
                     average_strategy_[index] =
                        cumulative_strategy_sum_[index] / strategy_sum_total;
                 } else {
                     average_strategy_[index] = default_prob;
                 }
            }
        }
    }
    average_strategy_valid_ = true;
}


// --- Overridden Interface Methods ---

const std::vector<double>& DiscountedCfrTrainable::GetCurrentStrategy() const {
    if (!current_strategy_valid_) {
        const_cast<DiscountedCfrTrainable*>(this)->CalculateCurrentStrategy();
    }
    return current_strategy_;
}

const std::vector<double>& DiscountedCfrTrainable::GetAverageStrategy() const {
    if (!average_strategy_valid_) {
        CalculateAverageStrategy();
    }
    return average_strategy_;
}


void DiscountedCfrTrainable::UpdateRegrets(const std::vector<double>& weighted_regrets,
    int iteration,
    double reach_prob_opponent_chance_scalar) {

    size_t total_size = num_actions_ * num_hands_;
    if (weighted_regrets.size() != total_size) {
        throw std::invalid_argument("Regret vector size mismatch in UpdateRegrets.");
     }
    if (iteration <= 0) {
        throw std::invalid_argument("Iteration number must be positive in UpdateRegrets.");
    }

    double iter_d = static_cast<double>(iteration);
    double alpha_discount = std::pow(iter_d, kAlpha) / (std::pow(iter_d, kAlpha) + 1.0);
    double beta_discount = std::pow(iter_d, kBeta) / (std::pow(iter_d, kBeta) + 1.0);

    // Loop over flat index, calculation is independent of layout here
    for (size_t i = 0; i < total_size; ++i) {
        double current_cum_regret = cumulative_regrets_[i];
        double discount_factor = (current_cum_regret > 0) ? alpha_discount : beta_discount;
        double discounted_regret = current_cum_regret * discount_factor;
        cumulative_regrets_[i] = discounted_regret + weighted_regrets[i]; // Add pre-weighted regret
    }

    current_strategy_valid_ = false;
    average_strategy_valid_ = false;
}


void DiscountedCfrTrainable::AccumulateAverageStrategy(
    const std::vector<double>& current_strategy,
    int iteration,
    const std::vector<double>& reach_probs_player_chance_vector) {

    size_t total_size = num_actions_ * num_hands_;
    if (current_strategy.size() != total_size || reach_probs_player_chance_vector.size() != num_hands_) {
        std::ostringstream oss;
        oss << "Size mismatch in AccumulateAverageStrategy: strategy=" << current_strategy.size() << " (expected " << total_size << "), reach_probs=" << reach_probs_player_chance_vector.size() << " (expected " << num_hands_ << ")";
        throw std::invalid_argument(oss.str());
    }
    if (iteration <= 0) {
         throw std::invalid_argument("Iteration number must be positive in AccumulateAverageStrategy.");
     }

     double iter_d = static_cast<double>(iteration);
     double gamma_discount_factor = std::pow(iter_d, kGamma); // Using t^gamma


     for (size_t h = 0; h < num_hands_; ++h) { // Iterate hands
         double hand_reach_weight = std::max(0.0, reach_probs_player_chance_vector[h]);
         double final_weight_for_hand = hand_reach_weight * gamma_discount_factor;

         if (final_weight_for_hand < 1e-12) continue; // Skip if weight is zero

         for (size_t a = 0; a < num_actions_; ++a) { // Iterate actions
            // *** FIXED INDEXING ***
             size_t index = h * num_actions_ + a; // Hand-Major index
             if(index < current_strategy.size() && index < cumulative_strategy_sum_.size()) {
                cumulative_strategy_sum_[index] += final_weight_for_hand * current_strategy[index];
             }
         }
     }

     average_strategy_valid_ = false;
}


void DiscountedCfrTrainable::SetEv(const std::vector<double>& evs) {
    size_t total_size = num_actions_ * num_hands_;
    if (evs.size() != total_size) { throw std::invalid_argument("EV vector size mismatch in SetEv."); }
    expected_values_ = evs;
}

json DiscountedCfrTrainable::DumpStrategy(bool with_ev) const {
    json result = json::object(); json strategy_map = json::object(); json ev_map = json::object();
    const auto& avg_strategy = GetAverageStrategy(); // Ensure it's calculated
    if (!player_range_) { result["error"] = "Player range not set"; return result; }
    if (num_hands_ == 0) { result["warning"] = "Player range is empty"; }
    std::vector<std::string> action_strings; action_strings.reserve(num_actions_);
    for(const auto& action : action_node_.GetActions()) { action_strings.push_back(action.ToString()); }
    result["actions"] = action_strings;

    for (size_t h = 0; h < num_hands_; ++h) {
        const core::PrivateCards& hand = (*player_range_)[h]; std::string hand_str = hand.ToString();
        std::vector<double> hand_avg_strategy(num_actions_);
        std::vector<double> hand_evs(num_actions_, std::numeric_limits<double>::quiet_NaN());

        for (size_t a = 0; a < num_actions_; ++a) {
            // *** FIXED INDEXING ***
             size_t index = h * num_actions_ + a; // Hand-Major index
             if (index < avg_strategy.size()) { hand_avg_strategy[a] = avg_strategy[index]; }
             if (with_ev && index < expected_values_.size()) { hand_evs[a] = expected_values_[index]; }
        }
        strategy_map[hand_str] = hand_avg_strategy;
        if(with_ev) { ev_map[hand_str] = hand_evs; }
    }
    result["strategy"] = strategy_map; if (with_ev) { result["evs"] = ev_map; } return result;
}

json DiscountedCfrTrainable::DumpEvs() const {
     json result = json::object(); json ev_map = json::object();
     if (!player_range_) { result["error"] = "Player range not set"; return result; }
     if (num_hands_ == 0) { result["warning"] = "Player range is empty"; }
     std::vector<std::string> action_strings; action_strings.reserve(num_actions_);
     for(const auto& action : action_node_.GetActions()) { action_strings.push_back(action.ToString()); }
     result["actions"] = action_strings;

     for (size_t h = 0; h < num_hands_; ++h) {
        const core::PrivateCards& hand = (*player_range_)[h]; std::string hand_str = hand.ToString();
        std::vector<double> hand_evs(num_actions_, std::numeric_limits<double>::quiet_NaN());
        for (size_t a = 0; a < num_actions_; ++a) {
            // *** FIXED INDEXING ***
             size_t index = h * num_actions_ + a; // Hand-Major index
             if (index < expected_values_.size()) { hand_evs[a] = expected_values_[index]; }
        }
        ev_map[hand_str] = hand_evs;
     }
     result["evs"] = ev_map; return result;
}


void DiscountedCfrTrainable::CopyStateFrom(const Trainable& other) {
    // Implementation remains the same (copies whole vectors)
    const auto* other_dcfr_ptr = dynamic_cast<const DiscountedCfrTrainable*>(&other);
    if (!other_dcfr_ptr) { throw std::invalid_argument("Cannot copy state: 'other' is not a DiscountedCfrTrainable."); }
    const DiscountedCfrTrainable& other_dcfr = *other_dcfr_ptr;
    if (num_actions_ != other_dcfr.num_actions_ || num_hands_ != other_dcfr.num_hands_) { throw std::invalid_argument("Cannot copy state: Dimensions mismatch."); }
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