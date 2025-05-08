
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

    // --- DEBUG PRINT ---
    // std::cout << "DEBUG: DCFR Constructor: player_range size=" << (player_range ? player_range->size() : 0)
    //           << ", action_node actions=" << action_node.GetActions().size() << std::endl;
    // -------------------

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
        current_strategy_.resize(total_size, initial_prob);
        average_strategy_.resize(total_size, initial_prob);
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

    // --- DEBUG PRINT ---
    std::cout << "DEBUG: CalculateCurrentStrategy: num_actions=" << num_actions_ << ", num_hands=" << num_hands_ << std::endl;
    // -------------------

    if (num_actions_ == 0 || num_hands_ == 0) {
        current_strategy_.clear();
        current_strategy_valid_ = true;
        return;
    }

    double default_prob = 1.0 / static_cast<double>(num_actions_);
    size_t total_size = num_actions_ * num_hands_;
    if(current_strategy_.size() != total_size) current_strategy_.resize(total_size);

    std::cout << std::fixed << std::setprecision(8); // Set precision for debug output

    for (size_t h = 0; h < num_hands_; ++h) {
        double regret_sum = 0.0;
        // --- DEBUG PRINT ---
        // std::cout << "  DEBUG: Hand " << h << ": Regrets = {";
        // -------------------
        for (size_t a = 0; a < num_actions_; ++a) {
            size_t index = a * num_hands_ + h;
            if(index < cumulative_regrets_.size()) {
                regret_sum += std::max(0.0, cumulative_regrets_[index]);
                // --- DEBUG PRINT ---
                // if (a > 0) std::cout << ", ";
                // std::cout << cumulative_regrets_[index];
                // -------------------
            }
        }
        // --- DEBUG PRINT ---
        // std::cout << "}, PosRegretSum = " << regret_sum << std::endl;
        // -------------------

        for (size_t a = 0; a < num_actions_; ++a) {
            size_t index = a * num_hands_ + h;
            if (index < current_strategy_.size() && index < cumulative_regrets_.size()) {
                if (regret_sum > 1e-12) {
                    current_strategy_[index] =
                        std::max(0.0, cumulative_regrets_[index]) / regret_sum;
                } else {
                    current_strategy_[index] = default_prob;
                }
                // --- DEBUG PRINT ---
                // std::cout << "    DEBUG: Hand " << h << ", Action " << a << ": Strategy = " << current_strategy_[index] << std::endl;
                // -------------------
            }
        }
    }
    current_strategy_valid_ = true;
    // --- DEBUG PRINT ---
    // std::cout << "DEBUG: CalculateCurrentStrategy FINISHED. Final Strategy: {";
    // for(size_t i=0; i<current_strategy_.size(); ++i) std::cout << (i>0?", ":"") << current_strategy_[i];
    // std::cout << "}" << std::endl;
    // -------------------
}



void DiscountedCfrTrainable::CalculateAverageStrategy() const {
    if (average_strategy_valid_) return;

     // --- DEBUG PRINT ---
    std::cout << "DEBUG: CalculateAverageStrategy: num_actions=" << num_actions_ << ", num_hands=" << num_hands_ << std::endl;
    // -------------------


    if (num_actions_ == 0 || num_hands_ == 0) {
        average_strategy_.clear();
        average_strategy_valid_ = true;
        return;
    }

    double default_prob = 1.0 / static_cast<double>(num_actions_);
    size_t total_size = num_actions_ * num_hands_;
    if(average_strategy_.size() != total_size) average_strategy_.resize(total_size);

    std::cout << std::fixed << std::setprecision(8); // Set precision for debug output

    for (size_t h = 0; h < num_hands_; ++h) {
        double strategy_sum_total = 0.0;
         // --- DEBUG PRINT ---
        // std::cout << "  DEBUG: Hand " << h << ": Strategy Sums = {";
        // -------------------
        for (size_t a = 0; a < num_actions_; ++a) {
             size_t index = a * num_hands_ + h;
             if (index < cumulative_strategy_sum_.size()) {
                 strategy_sum_total += cumulative_strategy_sum_[index];
                 // --- DEBUG PRINT ---
                 // if (a > 0) std::cout << ", ";
                 // std::cout << cumulative_strategy_sum_[index];
                 // -------------------
             }
        }
         // --- DEBUG PRINT ---
        // std::cout << "}, TotalSum = " << strategy_sum_total << std::endl;
        // -------------------

        for (size_t a = 0; a < num_actions_; ++a) {
            size_t index = a * num_hands_ + h;
            if (index < average_strategy_.size() && index < cumulative_strategy_sum_.size()) {
                 if (strategy_sum_total > 1e-12) {
                     average_strategy_[index] =
                        cumulative_strategy_sum_[index] / strategy_sum_total;
                 } else {
                     average_strategy_[index] = default_prob;
                 }
                 // --- DEBUG PRINT ---
                std::cout << "    DEBUG: Hand " << h << ", Action " << a << ": AvgStrategy = " << average_strategy_[index] << std::endl;
                 // -------------------
            }
        }
    }
    average_strategy_valid_ = true;
     // --- DEBUG PRINT ---
     std::cout << "DEBUG: CalculateAverageStrategy FINISHED. Final Avg Strategy: {";
     for(size_t i=0; i<average_strategy_.size(); ++i) std::cout << (i>0?", ":"") << average_strategy_[i];
     std::cout << "}" << std::endl;
    // -------------------
}


// --- Overridden Interface Methods ---

const std::vector<double>& DiscountedCfrTrainable::GetCurrentStrategy() const {
    // --- DEBUG PRINT ---
     std::cout << "DEBUG: GetCurrentStrategy called (valid=" << current_strategy_valid_ << ")" << std::endl;
    // -------------------
    if (!current_strategy_valid_) {
        const_cast<DiscountedCfrTrainable*>(this)->CalculateCurrentStrategy();
    }
    return current_strategy_;
}

const std::vector<double>& DiscountedCfrTrainable::GetAverageStrategy() const {
     // --- DEBUG PRINT ---
     std::cout << "DEBUG: GetAverageStrategy called (valid=" << average_strategy_valid_ << ")" << std::endl;
    // -------------------
    if (!average_strategy_valid_) {
        CalculateAverageStrategy();
    }
    return average_strategy_;
}


void DiscountedCfrTrainable::UpdateRegrets(const std::vector<double>& regrets,
    int iteration,
    double reach_prob_opponent_chance_scalar) {

    // --- DEBUG PRINT ---
    std::cout << std::fixed << std::setprecision(8);
    std::cout << "DEBUG: UpdateRegrets: Iteration=" << iteration << ", OpponentReachScalar=" << reach_prob_opponent_chance_scalar << std::endl;
     std::cout << "  DEBUG: Incoming Regrets: {";
     for(size_t i=0; i<regrets.size(); ++i) std::cout << (i>0?", ":"") << regrets[i];
     std::cout << "}" << std::endl;
     std::cout << "  DEBUG: CumRegrets BEFORE: {";
     for(size_t i=0; i<cumulative_regrets_.size(); ++i) std::cout << (i>0?", ":"") << cumulative_regrets_[i];
     std::cout << "}" << std::endl;
    // -------------------


    size_t total_size = num_actions_ * num_hands_;
    if (regrets.size() != total_size) { /* ... throw ... */ }
    if (iteration <= 0) { /* ... throw ... */ }
    double weight = std::max(0.0, reach_prob_opponent_chance_scalar);

    double iter_d = static_cast<double>(iteration);
    double alpha_discount = std::pow(iter_d, kAlpha) / (std::pow(iter_d, kAlpha) + 1.0);
    double beta_discount = std::pow(iter_d, kBeta) / (std::pow(iter_d, kBeta) + 1.0);

    // --- DEBUG PRINT ---
     std::cout << "  DEBUG: alpha_discount=" << alpha_discount << ", beta_discount=" << beta_discount << ", weight=" << weight << std::endl;
    // -------------------

    for (size_t i = 0; i < total_size; ++i) {
        double current_cum_regret = cumulative_regrets_[i]; // Store before modification
        double discount_factor = (current_cum_regret > 0) ? alpha_discount : beta_discount;
        double discounted_regret = current_cum_regret * discount_factor;
        double weighted_new_regret = regrets[i] * weight;

        cumulative_regrets_[i] = discounted_regret + weighted_new_regret;

        // --- DEBUG PRINT ---
         std::cout << "    DEBUG: Index " << i << ": PrevReg=" << current_cum_regret
                   << ", DiscountFactor=" << discount_factor
                   << ", DiscountedReg=" << discounted_regret
                   << ", NewReg=" << regrets[i]
                   << ", WeightedNewReg=" << weighted_new_regret
                   << ", FinalReg=" << cumulative_regrets_[i] << std::endl;
        // -------------------
    }

    // --- DEBUG PRINT ---
     std::cout << "  DEBUG: CumRegrets AFTER: {";
     for(size_t i=0; i<cumulative_regrets_.size(); ++i) std::cout << (i>0?", ":"") << cumulative_regrets_[i];
     std::cout << "}" << std::endl;
    // -------------------

    current_strategy_valid_ = false;
    average_strategy_valid_ = false; // Regret update might implicitly change average strategy expectation
}


void DiscountedCfrTrainable::AccumulateAverageStrategy(
    const std::vector<double>& current_strategy,
    int iteration,
    double reach_prob_player_chance_scalar) {

    // --- DEBUG PRINT ---
    std::cout << std::fixed << std::setprecision(8);
    std::cout << "DEBUG: AccumulateAverageStrategy: Iteration=" << iteration << ", PlayerReachScalar=" << reach_prob_player_chance_scalar << std::endl;
     std::cout << "  DEBUG: Incoming Strategy: {";
     for(size_t i=0; i<current_strategy.size(); ++i) std::cout << (i>0?", ":"") << current_strategy[i];
     std::cout << "}" << std::endl;
     std::cout << "  DEBUG: StrategySum BEFORE: {";
     for(size_t i=0; i<cumulative_strategy_sum_.size(); ++i) std::cout << (i>0?", ":"") << cumulative_strategy_sum_[i];
     std::cout << "}" << std::endl;
    // -------------------

    size_t total_size = num_actions_ * num_hands_;
     if (current_strategy.size() != total_size) { /* ... throw ... */ }
     if (iteration <= 0) { /* ... throw ... */ }
     double weight = std::max(0.0, reach_prob_player_chance_scalar);

     double iter_d = static_cast<double>(iteration);
     double gamma_discount_factor = std::pow(iter_d, kGamma); // Using t^gamma
     double final_weight = weight * gamma_discount_factor;

     // --- DEBUG PRINT ---
     std::cout << "  DEBUG: gamma_discount=" << gamma_discount_factor << ", final_weight=" << final_weight << std::endl;
    // -------------------

     for (size_t i = 0; i < total_size; ++i) {
         double value_to_add = final_weight * current_strategy[i];
         // --- DEBUG PRINT ---
          std::cout << "    DEBUG: Index " << i << ": PrevSum=" << cumulative_strategy_sum_[i]
                    << ", Adding=" << value_to_add;
         // -------------------
         cumulative_strategy_sum_[i] += value_to_add;
          // --- DEBUG PRINT ---
          std::cout << ", NewSum=" << cumulative_strategy_sum_[i] << std::endl;
         // -------------------
     }

    // --- DEBUG PRINT ---
     std::cout << "  DEBUG: StrategySum AFTER: {";
     for(size_t i=0; i<cumulative_strategy_sum_.size(); ++i) std::cout << (i>0?", ":"") << cumulative_strategy_sum_[i];
     std::cout << "}" << std::endl;
    // -------------------

     average_strategy_valid_ = false;
}


void DiscountedCfrTrainable::SetEv(const std::vector<double>& evs) {
    // ... (Implementation remains the same) ...
    size_t total_size = num_actions_ * num_hands_;
    if (evs.size() != total_size) { throw std::invalid_argument("EV vector size mismatch in SetEv."); }
    expected_values_ = evs;
}

json DiscountedCfrTrainable::DumpStrategy(bool with_ev) const {
    // ... (Implementation remains the same) ...
    json result = json::object(); json strategy_map = json::object(); json ev_map = json::object();
    const auto& avg_strategy = GetAverageStrategy();
    if (!player_range_) { result["error"] = "Player range not set"; return result; }
    if (num_hands_ == 0) { result["warning"] = "Player range is empty"; }
    std::vector<std::string> action_strings; action_strings.reserve(num_actions_);
    for(const auto& action : action_node_.GetActions()) { action_strings.push_back(action.ToString()); }
    result["actions"] = action_strings;
    for (size_t h = 0; h < num_hands_; ++h) {
        const core::PrivateCards& hand = (*player_range_)[h]; std::string hand_str = hand.ToString();
        std::vector<double> hand_avg_strategy(num_actions_); std::vector<double> hand_evs(num_actions_, std::numeric_limits<double>::quiet_NaN());
        for (size_t a = 0; a < num_actions_; ++a) {
             size_t index = a * num_hands_ + h;
             if (index < avg_strategy.size()) { hand_avg_strategy[a] = avg_strategy[index]; }
             if (with_ev && index < expected_values_.size()) { hand_evs[a] = expected_values_[index]; }
        }
        strategy_map[hand_str] = hand_avg_strategy;
        if(with_ev) { ev_map[hand_str] = hand_evs; }
    }
    result["strategy"] = strategy_map; if (with_ev) { result["evs"] = ev_map; } return result;
}

json DiscountedCfrTrainable::DumpEvs() const {
    // ... (Implementation remains the same) ...
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
             size_t index = a * num_hands_ + h;
             if (index < expected_values_.size()) { hand_evs[a] = expected_values_[index]; }
        }
        ev_map[hand_str] = hand_evs;
     }
     result["evs"] = ev_map; return result;
}



void DiscountedCfrTrainable::CopyStateFrom(const Trainable& other) {
    // ... (Implementation remains the same) ...
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
