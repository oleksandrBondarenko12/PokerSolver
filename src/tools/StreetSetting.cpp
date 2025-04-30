#include "tools/StreetSetting.h" // Adjust path if necessary

#include <utility> // For std::move

namespace poker_solver {
namespace config {

StreetSetting::StreetSetting(std::vector<double> bet_sizes_percent,
                             std::vector<double> raise_sizes_percent,
                             std::vector<double> donk_sizes_percent,
                             bool allow_all_in)
    : bet_sizes_percent(std::move(bet_sizes_percent)),
      raise_sizes_percent(std::move(raise_sizes_percent)),
      donk_sizes_percent(std::move(donk_sizes_percent)),
      allow_all_in(allow_all_in) {
  // Constructor body is empty as initialization is done via member initializer list.
}

} // namespace config
} // namespace poker_solver
