#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <chrono>
#include <random>
#include <cstdint>
#include <stdexcept>
#include <cmath>
#include <algorithm>

// All utility routines are placed in the PokerSolverUtils namespace.
namespace PokerSolverUtils {

    // 1. Combinations Iterator
    template <typename T>
    class Combinations {
    public:
        // Constructor takes the set and the size of the combination (m).
        Combinations(const std::vector<T>& set, int m)
            : set_(set), m_(m), n_(static_cast<int>(set.size())), done_(false)
        {
            if (m_ < 0 || m_ > n_) {
                throw std::invalid_argument("Invalid combination size");
            }
            indices_.resize(m_);
            for (int i = 0; i < m_; ++i) {
                // Corrected: Use indices_
                indices_[i] = i;
            }
            // If m_ is 0, we consider that we are done immediately.
            done_ = (m_ == 0);
        }

        // Returns true if all combinations have been generated.
        bool done() const {
            return done_;
        }

        // Returns the current combination.
        std::vector<T> current() const {
            std::vector<T> combination;
            combination.reserve(m_);
            for (int i = 0; i < m_; ++i) {
                // Corrected: Use indices_ and add missing ']'
                combination.push_back(set_[indices_[i]]);
            }
            return combination;
        }

        // Advances to the next combination (in lexicographic order).
        void next() {
            if (done_) {
                return;
            }
            int i = m_ - 1;
            // Corrected: Use indices_
            while (i >= 0 && indices_[i] == i + n_ - m_) {
                --i;
            }
            if (i < 0) {
                done_ = true;
                return;
            }
            // Corrected: Use indices_
            ++indices_[i];
            // Reset all subsequent indices.
            for (int j = i + 1; j < m_; j++) {
                // Corrected: Use indices_ (in both places)
                indices_[j] = indices_[i] + j - i;
            }
        }

    private:
        std::vector<T> set_;       // Original set of elements.
        int n_;                   // Total number of elements in the set.
        int m_;                   // Size of each combination.
        std::vector<int> indices_; // Current indices for the combination.
        bool done_;               // Flag indicating completion.
    };

    // 2. String Splitting Function
    inline std::vector<std::string> string_split(const std::string& input, char delimiter) {
        std::vector<std::string> tokens;
        std::istringstream stream(input);
        std::string token;
        while (std::getline(stream, token, delimiter)) {
            if (!token.empty()) {
                tokens.push_back(token);
            }
        }
        return tokens;
    }

    // 3. Time Since Epoch in Milliseconds
    inline uint64_t timeSinceEpochMillisec() {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        return static_cast<uint64_t>(millis);
    }

    // 4. Random Number Generator Function
    inline int random_int(int min, int max) {
        thread_local std::mt19937 rng{ std::random_device{}() };
        if (min > max) {
             throw std::invalid_argument("random_int: min cannot be greater than max");
        }
        std::uniform_int_distribution<int> dist(min, max);
        return dist(rng);
    }

    // 5. Normalization Function (using tanh)
    inline float normalization_tanh(float stack, float ev, float ratio = 7.0f) {
        if (stack == 0.0f) return 0.0f; // Avoid division by zero
        // Check for potential NaN/Inf before tanh if stack or ev could be problematic
        if (!std::isfinite(stack) || !std::isfinite(ev)) return 0.0f; // Or handle appropriately
        float normalized = ev / stack;
        return std::tanh(normalized * ratio);
    }
} // namespace PokerSolverUtils
