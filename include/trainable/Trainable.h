#ifndef POKERSOLVER_TRAINABLE_H
#define POKERSOLVER_TRAINABLE_H

#include <vector>
#include <memory>
#include <include/json.hpp>

namespace PokerSolver {

// Alias for JSON (using nlohmann::json)
using json = nlohmann::json;

// The Trainable interface provides a set of methods that every
// training module must implement. This allows different training
// strategies (e.g. CFR-Plus or Discounted CFR) to be interchanged.
class Trainable {
public:
    // Enumerated type to identify the specific training algorithm.
    enum class Type {
        CFR_PLUS,
        DISCOUNTED_CFR
    };

    // Virtual destructor ensures that derived classes are correctly cleaned up.
    virtual ~Trainable();

    // Returns the average strategy as a vector of probabilities.
    virtual std::vector<float> getAverageStrategy() const = 0;

    // Returns the current (instantaneous) strategy as a vector of probabilities.
    virtual std::vector<float> getCurrentStrategy() const = 0;

    // Updates the internal regret values based on the given regrets,
    // the iteration number, and the reach probabilities.
    virtual void updateRegrets(const std::vector<float>& regrets,
                               int iterationNumber,
                               const std::vector<float>& reachProbs) = 0;

    // Sets the expected values (EVs) for the node or action.
    virtual void setEv(const std::vector<float>& evs) = 0;

    // Copies strategy information from another Trainable instance.
    virtual void copyStrategy(const std::shared_ptr<Trainable>& other) = 0;

    // Dumps the current strategy (and optionally extra state) to JSON.
    virtual json dumpStrategy(bool withState, int depth) const = 0;

    // Dumps the EVs to JSON.
    virtual json dumpEvs() const = 0;

    // Returns the type of this trainable (e.g. CFR_PLUS or DISCOUNTED_CFR).
    virtual Type getType() const = 0;
};

} // namespace PokerSolver

#endif // POKERSOLVER_TRAINABLE_H
