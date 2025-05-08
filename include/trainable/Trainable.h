#ifndef POKER_SOLVER_SOLVER_TRAINABLE_H_
#define POKER_SOLVER_SOLVER_TRAINABLE_H_

#include <vector>
#include <string>
#include <memory>
#include <map>
#include <json.hpp>

using json = nlohmann::json;

namespace poker_solver {
namespace solver {

class Trainable {
 public:
  virtual ~Trainable() = default;

  virtual const std::vector<double>& GetCurrentStrategy() const = 0;
  virtual const std::vector<double>& GetAverageStrategy() const = 0;

  // Regrets arrive pre-weighted by opp_reach * chance_reach.
  // Implementations should apply iteration discounting to cumulative values
  // and add the incoming weighted regret.
  virtual void UpdateRegrets(const std::vector<double>& weighted_regrets, int iteration,
                             double reach_prob_opponent_chance_scalar) = 0; // Weight scalar might be unused by some impls

  // Accumulates the average strategy.
  // Implementations should apply iteration discounting and use the per-hand
  // weights provided in the vector.
  virtual void AccumulateAverageStrategy(const std::vector<double>& current_strategy,
                                         int iteration,
                                         const std::vector<double>& reach_probs_player_chance_vector) = 0; // VECTOR of weights

  virtual void SetEv(const std::vector<double>& evs) = 0;
  virtual json DumpStrategy(bool with_ev) const = 0;
  virtual json DumpEvs() const = 0;
  virtual void CopyStateFrom(const Trainable& other) = 0;

 protected:
  Trainable() = default;
  Trainable(const Trainable&) = delete;
  Trainable& operator=(const Trainable&) = delete;
  Trainable(Trainable&&) = delete;
  Trainable& operator=(Trainable&&) = delete;
};
} // namespace solver
} // namespace poker_solver

#endif // POKER_SOLVER_SOLVER_TRAINABLE_H_
