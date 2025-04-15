#ifndef POKERSOLVER_CFRPLUS_H
#define POKERSOLVER_CFRPLUS_H

#include "trainable/Trainable.h"
#include <vector>
#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace PokerSolver {

/**
 * @brief Concrete implementation of a training strategy based on CFR+.
 *
 * This class implements the Counterfactual Regret Minimization Plus (CFR+)
 * algorithm. It maintains cumulative (positive) regrets, strategy sums for
 * averaging, and computes the current strategy by normalizing the positive regrets.
 */
class CFRPlus : public Trainable {
public:
    /**
     * @brief Constructor for CFRPlus.
     * @param numActions The number of available actions at the decision node.
     */
    explicit CFRPlus(int numActions);

    /// Virtual destructor.
    virtual ~CFRPlus() override = default;

    /**
     * @brief Returns the average strategy (normalized strategySum) as a vector of probabilities.
     * @return A vector of floats representing the average strategy.
     */
    virtual std::vector<float> getAverageStrategy() const override;

    /**
     * @brief Returns the current (instantaneous) strategy as a vector of probabilities.
     * @return A vector of floats representing the current strategy.
     */
    virtual std::vector<float> getCurrentStrategy() const override;

    /**
     * @brief Updates the cumulative regrets and recomputes the current strategy.
     *
     * For each action, the cumulative regret is updated using:
     *   newRegret = max(cumulativeRegret + regret, 0)
     * Then the current strategy is computed by normalizing the positive regrets.
     *
     * The strategy sum is updated using reach probabilities so that the average strategy
     * can be computed later.
     *
     * @param regrets The vector of immediate regrets for each action.
     * @param iterationNumber The current iteration (unused in this basic implementation).
     * @param reachProbs The vector of reach probabilities to weight the current strategy.
     */
    virtual void updateRegrets(const std::vector<float>& regrets,
                               int iterationNumber,
                               const std::vector<float>& reachProbs) override;

    /**
     * @brief Sets the expected values for each action.
     *
     * In this implementation we simply store the provided EVs.
     *
     * @param evs A vector of floats representing the expected values.
     */
    virtual void setEv(const std::vector<float>& evs) override;

    /**
     * @brief Copies the internal strategy from another Trainable instance.
     *
     * Only works if the other Trainable is a CFRPlus instance.
     *
     * @param other A shared pointer to another Trainable instance.
     */
    virtual void copyStrategy(const std::shared_ptr<Trainable>& other) override;

    /**
     * @brief Dumps the current strategy and internal state to a JSON object.
     *
     * @param withState If true, include detailed state (cumulative regrets, strategy sum).
     * @param depth The level of detail desired.
     * @return A JSON object containing the dumped state.
     */
    virtual json dumpStrategy(bool withState, int depth) const override;

    /**
     * @brief Dumps the expected values (EVs) as a JSON object.
     * @return A JSON object containing the EVs.
     */
    virtual json dumpEvs() const override;

    /**
     * @brief Returns the type of this Trainable instance.
     * @return The enumerated type Trainable::Type::CFR_PLUS.
     */
    virtual Type getType() const override;

private:
    int numActions_;
    // For each action we store:
    std::vector<float> cumulativeRegrets_;   // CFR+: Only nonnegative cumulative regrets.
    std::vector<float> strategySum_;         // Accumulated weighted strategy over iterations.
    std::vector<float> currentStrategy_;       // The current strategy computed on this iteration.
    std::vector<float> evs_;                   // Expected values for each action (if applicable).

    /**
     * @brief Recalculates the current strategy by normalizing cumulative regrets.
     *
     * If the sum of positive regrets is zero, a uniform distribution is used.
     */
    void recalcCurrentStrategy();
};

} // namespace PokerSolver

#endif // POKERSOLVER_CFRPLUS_H
