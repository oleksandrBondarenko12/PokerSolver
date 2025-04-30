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
// Use alias from trainable.h
using json = nlohmann::json;


namespace poker_solver {
namespace solver {

// Concrete implementation of the Trainable interface using the
// Discounted Counterfactual Regret Minimization (DCFR) algorithm.
class DiscountedCfrTrainable : public Trainable {
 public:
  // Constructor.
  // Args:
  //   player_range: Pointer to the vector of PrivateCards for the acting player
  //                 at this node. Lifetime must be managed externally.
  //   action_node: Const reference to the ActionNode this Trainable belongs to.
  //                Used to get the number of actions.
  DiscountedCfrTrainable(
      const std::vector<core::PrivateCards>* player_range,
      const nodes::ActionNode& action_node); // Pass ActionNode by const ref

  // Virtual destructor.
  ~DiscountedCfrTrainable() override = default;

  // --- Overridden Interface Methods ---

  const std::vector<float>& GetCurrentStrategy() const override;
  const std::vector<float>& GetAverageStrategy() const override;

  void UpdateRegrets(const std::vector<float>& regrets, int iteration,
                     const std::vector<float>& reach_probs) override;

  void SetEv(const std::vector<float>& evs) override;

  json DumpStrategy(bool with_ev) const override;
  json DumpEvs() const override;

  // Use matching signature with override
  void CopyStateFrom(const Trainable& other) override; // Corrected signature


 private:
  // Helper methods for lazy calculation
  void CalculateCurrentStrategy(); // Non-const as it modifies mutable members
  void CalculateAverageStrategy() const; // Const is appropriate

  // --- DCFR Parameters ---
  // These control the discounting rates. Values can be tuned.
  // Using double for potentially better precision in calculations.
  static constexpr double kAlpha = 1.5; // Regret discounting exponent (positive regrets)
  static constexpr double kBeta = 0.5;  // Regret discounting exponent (negative regrets)
  static constexpr double kGamma = 2.0; // Strategy discounting exponent
  // static constexpr double kTheta = 1.0; // Optional additional discount factor for strategy sum (original used 0.9)

  // --- Member Variables ---

  // Const reference to the associated ActionNode (to get action count).
  // Assumes ActionNode lifetime exceeds Trainable lifetime.
  const nodes::ActionNode& action_node_;

  // Pointer to the relevant player range (vector of hands). Not owned.
  const std::vector<core::PrivateCards>* player_range_;

  // Store sizes for convenience.
  size_t num_actions_;
  size_t num_hands_;

  // State variables
  std::vector<float> cumulative_regrets_;
  std::vector<double> cumulative_strategy_sum_; // Use double for sums

  // Cached strategies
  // Good: mutable allows modification within const getter methods for lazy eval.
  mutable std::vector<float> current_strategy_;
  mutable std::vector<float> average_strategy_;
  mutable bool current_strategy_valid_ = false;
  mutable bool average_strategy_valid_ = false;

  // Stores the calculated EVs (optional, populated by SetEv).
  std::vector<float> expected_values_;

  // Deleted copy/move operations.
  DiscountedCfrTrainable(const DiscountedCfrTrainable&) = delete;
  DiscountedCfrTrainable& operator=(const DiscountedCfrTrainable&) = delete;
  DiscountedCfrTrainable(DiscountedCfrTrainable&&) = delete;
  DiscountedCfrTrainable& operator=(DiscountedCfrTrainable&&) = delete;

};

} // namespace solver
} // namespace poker_solver

#endif // POKER_SOLVER_SOLVER_DISCOUNTED_CFR_TRAINABLE_H_
