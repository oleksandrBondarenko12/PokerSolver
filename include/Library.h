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
    //
    // The Combinations class generates all possible combinations of size m
    // from a given vector of type T. This is useful, for example, when iterating
    // over possible board or hand combinations.

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
                indices[i] = i;
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
                combination.push_back(set_[indices[i]);
            }
            return combination;
        }

        // Advances to the next combination (in lexicographic order).
        void next() {
            if (done_) {
                return;
            }
            int i = m_ - 1;
            while (i >= 0 && indices[i] == i + n_ - m_) {
                --i;
            }
            if (i < 0) {
                done_ = true;
                return;
            }
            ++indices_[i];
            // Reset all subsequent indices.
            for (int j = i + 1; j < m_; j++) {
                indices[j] = indices[i] + j - i;
            }

        }


    private:
        std::vector<T> set_;      // Original set of elements.
        int n_;                   // Total number of elements in the set.
        int m_;                   // Size of each combination.
        std::vector<int> indices_; // Current indices for the combination.
        bool done_;               // Flag indicating completion.

    };

    // 2. String Splitting Function
    //
    // Splits the given string based on the specified delimiter and returns
    // a vector of tokens.
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
    //
    // Returns the current time in milliseconds since January 1, 1970 (epoch).
    //////////////////////////////////////////////////////////////////////////////
    inline uint64_t timeSinceEpochMillisec() {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        return static_cast<uint64_t>(millis);
    }

    // 4. Random Number Generator Function
    //
    // Returns a random integer in the range [min, max] using a uniform
    // distribution. This uses a thread-local Mersenne Twister.
    inline int random_int(int min, int max) {
        thread_local std::mt19937 rng{ std::random_device{}() };
        std::uniform_int_distribution<int> dist(min, max);
        return dist(rng);
    }

    // 5. Normalization Function (using tanh)
    //
    // Normalizes the expected value (ev) by dividing by the stack and scaling
    // using the hyperbolic tangent function. The ratio parameter adjusts the
    // scaling.
    inline float normalization_tanh(float stack, float ev, float ratio = 7.0f) {
        if (stack == 0.0f) return 0.0f;
        float normalized = ev / stack;
        return std::tanh(normalized * ratio);
    }
}
