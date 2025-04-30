#ifndef POKER_SOLVER_SOLVER_TRAINABLE_H_
#define POKER_SOLVER_SOLVER_TRAINABLE_H_

#include <vector>
#include <string>
#include <memory> // For std::shared_ptr
#include <map>    // <<< ADD THIS INCLUDE
#include <json.hpp> // <<< INCLUDE ACTUAL JSON HEADER

// Use the alias in our namespace for convenience
using json = nlohmann::json;


namespace poker_solver {
namespace solver {

// Abstract base class (interface) for storing and updating strategy and
// regret information at an ActionNode during CFR iterations.
class Trainable {
 public:
  // Virtual destructor is essential for base classes.
  virtual ~Trainable() = default;

  // Returns the current strategy profile based on positive regrets.
  virtual const std::vector<float>& GetCurrentStrategy() const = 0;

  // Returns the average strategy profile accumulated over all iterations.
  virtual const std::vector<float>& GetAverageStrategy() const = 0;

  // Updates the regrets and strategy sums based on the calculated utilities
  // for the current iteration.
  virtual void UpdateRegrets(const std::vector<float>& regrets, int iteration,
                             const std::vector<float>& reach_probs) = 0;

  // Stores the calculated expected values (EVs) for each action and hand.
  virtual void SetEv(const std::vector<float>& evs) = 0;

  // Creates a JSON representation of the strategy profile (usually average strategy).
  virtual json DumpStrategy(bool with_ev) const = 0;

  // Creates a JSON representation of the calculated expected values (EVs).
  virtual json DumpEvs() const = 0;

  // Copies the internal state from another Trainable object.
  // Takes const reference as we are copying *from* it.
  virtual void CopyStateFrom(const Trainable& other) = 0; // <<< CORRECTED SIGNATURE

 protected:
  // Protected default constructor.
  Trainable() = default;

  // Deleted copy/move operations.
  Trainable(const Trainable&) = delete;
  Trainable& operator=(const Trainable&) = delete;
  Trainable(Trainable&&) = delete;
  Trainable& operator=(Trainable&&) = delete;

};

} // namespace solver
} // namespace poker_solver

#endif // POKER_SOLVER_SOLVER_TRAINABLE_H_
