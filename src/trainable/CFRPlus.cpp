#include "trainable/CFRPlus.h"
#include <cmath>
#include <limits>
#include <nlohmann/json.hpp>

namespace PokerSolver {

CFRPlus::CFRPlus(int numActions)
    : numActions_(numActions),
      cumulativeRegrets_(numActions, 0.0f),
      strategySum_(numActions, 0.0f),
      currentStrategy_(numActions, 1.0f / numActions),
      evs_(numActions, 0.0f)
{
    if (numActions_ <= 0) {
        throw std::invalid_argument("CFRPlus: Number of actions must be positive.");
    }
}

std::vector<float> CFRPlus::getAverageStrategy() const {
    std::vector<float> avgStrategy(numActions_, 0.0f);
    float sum = std::accumulate(strategySum_.begin(), strategySum_.end(), 0.0f);
    if (sum > std::numeric_limits<float>::epsilon()) {
        for (int i = 0; i < numActions_; ++i) {
            avgStrategy[i] = strategySum_[i] / sum;
        }
    } else {
        // If no strategy sum, return uniform distribution.
        std::fill(avgStrategy.begin(), avgStrategy.end(), 1.0f / numActions_);
    }
    return avgStrategy;
}

std::vector<float> CFRPlus::getCurrentStrategy() const {
    return currentStrategy_;
}

void CFRPlus::updateRegrets(const std::vector<float>& regrets,
                            int /*iterationNumber*/,
                            const std::vector<float>& reachProbs)
{
    if (regrets.size() != static_cast<size_t>(numActions_) || reachProbs.size() != static_cast<size_t>(numActions_)) {
        throw std::invalid_argument("CFRPlus::updateRegrets: Size mismatch for regrets or reach probabilities.");
    }

    // Update cumulative regrets using CFR+ update rule: only accumulate if positive.
    for (int i = 0; i < numActions_; ++i) {
        float newRegret = cumulativeRegrets_[i] + regrets[i];
        cumulativeRegrets_[i] = std::max(newRegret, 0.0f);
    }

    // Recalculate current strategy based on the positive cumulative regrets.
    recalcCurrentStrategy();

    // Update strategy sum weighted by the reach probabilities.
    for (int i = 0; i < numActions_; ++i) {
        strategySum_[i] += reachProbs[i] * currentStrategy_[i];
    }
}

void CFRPlus::setEv(const std::vector<float>& evs) {
    if (evs.size() != static_cast<size_t>(numActions_)) {
        throw std::invalid_argument("CFRPlus::setEv: EV vector size does not match number of actions.");
    }
    evs_ = evs;
}

void CFRPlus::copyStrategy(const std::shared_ptr<Trainable>& other) {
    // Attempt dynamic_cast to ensure that 'other' is a CFRPlus instance.
    auto otherCFR = std::dynamic_pointer_cast<CFRPlus>(other);
    if (!otherCFR) {
        throw std::invalid_argument("CFRPlus::copyStrategy: Incompatible Trainable type.");
    }
    cumulativeRegrets_ = otherCFR->cumulativeRegrets_;
    strategySum_ = otherCFR->strategySum_;
    currentStrategy_ = otherCFR->currentStrategy_;
    evs_ = otherCFR->evs_;
}

json CFRPlus::dumpStrategy(bool withState, int /*depth*/) const {
    json j;
    j["currentStrategy"] = currentStrategy_;
    j["averageStrategy"] = getAverageStrategy();
    if (withState) {
        j["cumulativeRegrets"] = cumulativeRegrets_;
        j["strategySum"] = strategySum_;
    }
    return j;
}

json CFRPlus::dumpEvs() const {
    json j;
    j["evs"] = evs_;
    return j;
}

Trainable::Type CFRPlus::getType() const {
    return Trainable::Type::CFR_PLUS;
}

void CFRPlus::recalcCurrentStrategy() {
    float sumPos = 0.0f;
    for (int i = 0; i < numActions_; ++i) {
        // Use the positive part of the cumulative regret.
        sumPos += cumulativeRegrets_[i];
    }
    if (sumPos > std::numeric_limits<float>::epsilon()) {
        for (int i = 0; i < numActions_; ++i) {
            currentStrategy_[i] = cumulativeRegrets_[i] / sumPos;
        }
    } else {
        // When no positive regrets exist, fallback to uniform distribution.
        std::fill(currentStrategy_.begin(), currentStrategy_.end(), 1.0f / numActions_);
    }
}

} // namespace PokerSolver
