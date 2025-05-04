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

  virtual const std::vector<float>& GetCurrentStrategy() const = 0;
  virtual const std::vector<float>& GetAverageStrategy() const = 0;

  // *** REVERT: Use scalar weight for regret update ***
  virtual void UpdateRegrets(const std::vector<float>& regrets, int iteration,
                             double reach_prob_opponent_chance_scalar) = 0; // Renamed for clarity

  // *** REVERT: Use scalar weight for average strategy update ***
  // *** FIX 6: Rename based on user feedback/common practice ***
  virtual void AccumulateAverageStrategy(const std::vector<float>& current_strategy,
                                         int iteration,
                                         double reach_prob_player_chance_scalar) = 0; // Renamed & scalar

  virtual void SetEv(const std::vector<float>& evs) = 0;
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