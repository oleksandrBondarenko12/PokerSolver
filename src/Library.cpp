#include "Library.h" // Adjust path if necessary

#include <sstream>   // For std::stringstream
#include <string>    // Include for std::getline overload with std::string
#include <random>    // For std::mt19937, std::random_device, std::uniform_int_distribution
#include <chrono>    // For std::chrono::system_clock, etc.
#include <cmath>     // For std::tanh
#include <stdexcept> // For std::invalid_argument (used in GetRandomInt potentially)
#include <algorithm> // For std::swap (used in GetRandomInt potentially)

namespace poker_solver {
namespace utils {

std::vector<std::string> StringSplit(std::string_view text, char delimiter) {
  std::vector<std::string> result;
  std::string current_token;
  // Fix Vexing Parse: Use {} initialization for stringstream
  // Construct string explicitly first, then pass to stringstream
  std::string text_str{text}; // Create std::string from string_view
  std::stringstream ss{text_str};

  // Use getline to extract tokens separated by the delimiter
  while (std::getline(ss, current_token, delimiter)) {
    result.push_back(current_token);
  }
  return result;
}

uint64_t TimeSinceEpochMillisec() {
  // Get the current time point from the system clock
  auto now = std::chrono::system_clock::now();
  // Calculate the duration since the epoch
  auto duration = now.time_since_epoch();
  // Convert the duration to milliseconds
  auto millisec = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
  // Return the count of milliseconds as uint64_t
  return millisec.count();
}

int GetRandomInt(int min_val, int max_val) {
  // Use thread_local to ensure each thread has its own random number generator
  // and seed state. This avoids potential data races and improves performance
  // compared to a single global generator with locking.
  thread_local std::mt19937 generator = []() {
    // Seed the generator for this thread using a random_device for
    // non-deterministic seeding where possible.
    std::random_device rd;
    return std::mt19937(rd());
  }(); // Immediately invoked lambda to initialize the generator

  // Ensure min_val is not greater than max_val
  if (min_val > max_val) {
    // Option 1: Throw an error
     std::ostringstream oss;
     oss << "min_val (" << min_val << ") cannot be greater than max_val ("
         << max_val << ") in GetRandomInt.";
     throw std::invalid_argument(oss.str());
    // Option 2: Swap them (less safe, hides potential logic errors)
    // std::swap(min_val, max_val);
  }

  // Create a uniform distribution for the desired range (inclusive)
  std::uniform_int_distribution<int> distribution(min_val, max_val);

  // Generate and return a random number using the thread's generator
  return distribution(generator);
}

double NormalizeTanh(double stack, double ev, double ratio) {
  // Avoid division by zero or negative stack
  if (stack <= 0) {
      // Return a default value or handle error appropriately
      // Returning 0.5 (neutral) might be a reasonable default.
      return 0.5;
  }
  // Calculate the scaled input for tanh
  double x = (ev / stack) * ratio;
  // Apply tanh, which maps (-inf, +inf) to (-1, 1)
  double tanh_result = std::tanh(x);
  // Scale and shift the result to map (-1, 1) to (0, 1)
  return (tanh_result / 2.0) + 0.5;
}

} // namespace utils
} // namespace poker_solver
