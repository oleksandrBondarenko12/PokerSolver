#ifndef POKER_SOLVER_SOLVER_DISCOUNTED_CFR_TRAINABLE_H_
#define POKER_SOLVER_SOLVER_DISCOUNTED_CFR_TRAINABLE_H_

#include "trainable/Trainable.h"   // Base class interface
#include "ranges/PrivateCards.h" // For PrivateCards
// #include "nodes/action_node.h" // Use forward declaration below
#include <vector>
#include <memory> // For std::shared_ptr
#include <cmath>  // For std::pow, std::max
#include <numeric> // For std::accumulate
#include <stdexcept> // For exceptions
#include <limits>   // For numeric_limits
#include <map>      // Included by trainable.h for json fwd decl, good to have if needed
#include <json.hpp> // Included by trainable.h

// Forward declare ActionNode to break potential include cycle
namespace poker_solver { namespace nodes { class ActionNode; } }
namespace poker_solver { namespace core { class PrivateCards; } } // Also forward declare if needed
// Use alias from trainable.h
using json = nlohmann::json;


namespace poker_solver {
namespace solver {

// Concrete implementation of the Trainable interface using the
// Discounted Counterfactual Regret Minimization (DCFR) algorithm.
class DiscountedCfrTrainable : public Trainable {
 public:
  // Constructor.
  DiscountedCfrTrainable(
    const std::vector<core::PrivateCards>* player_range, // Pass range pointer
    const nodes::ActionNode& action_node);

  // Virtual destructor.
  ~DiscountedCfrTrainable() override = default;

  // --- Overridden Interface Methods ---

  const std::vector<double>& GetCurrentStrategy() const override;
  const std::vector<double>& GetAverageStrategy() const override;

  // Signature matches Trainable.h
  void UpdateRegrets(const std::vector<double>& weighted_regrets, int iteration,
                     double reach_prob_opponent_chance_scalar) override;

  // Signature matches Trainable.h
  void AccumulateAverageStrategy(const std::vector<double>& current_strategy,
                                 int iteration,
                                 const std::vector<double>& reach_probs_player_chance_vector) override; // VECTOR

  void SetEv(const std::vector<double>& evs) override;

  json DumpStrategy(bool with_ev) const override;
  json DumpEvs() const override;

  void CopyStateFrom(const Trainable& other) override;


 private:
  // Helper methods for lazy calculation
  void CalculateCurrentStrategy(); // Non-const as it modifies mutable members
  void CalculateAverageStrategy() const; // Const is appropriate

  // --- DCFR Parameters ---
  static constexpr double kAlpha = 1.5;
  static constexpr double kBeta = 0.5;
  static constexpr double kGamma = 2.0;

  // --- Member Variables ---
  const nodes::ActionNode& action_node_; // Store reference to get action count etc.
  const std::vector<core::PrivateCards>* player_range_; // Not owned
  size_t num_actions_;
  size_t num_hands_;
  std::vector<double> cumulative_regrets_;
  std::vector<double> cumulative_strategy_sum_;
  mutable std::vector<double> current_strategy_;
  mutable std::vector<double> average_strategy_;
  mutable bool current_strategy_valid_ = false;
  mutable bool average_strategy_valid_ = false;
  std::vector<double> expected_values_;

  // Deleted copy/move operations.
  DiscountedCfrTrainable(const DiscountedCfrTrainable&) = delete;
  DiscountedCfrTrainable& operator=(const DiscountedCfrTrainable&) = delete;
  DiscountedCfrTrainable(DiscountedCfrTrainable&&) = delete;
  DiscountedCfrTrainable& operator=(DiscountedCfrTrainable&&) = delete;

};

} // namespace solver
} // namespace poker_solver

#endif // POKER_SOLVER_SOLVER_DISCOUNTED_CFR_TRAINABLE_H_