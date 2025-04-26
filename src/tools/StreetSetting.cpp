#ifndef POKER_SOLVER_CONFIG_STREET_SETTING_H_
#define POKER_SOLVER_CONFIG_STREET_SETTING_H_

#include <vector>
#include <cstdint> // Included for consistency, though not directly used here

namespace poker_solver {
namespace config {

// Holds the allowed betting configurations for a specific street (Flop, Turn, River)
// for a particular player position (IP or OOP).
// Bet sizes are typically represented as percentages of the pot.
struct StreetSetting {
  // Creates a StreetSetting with specified bet/raise/donk sizes.
  // Args:
  //   bet_sizes_percent: Vector of allowed bet sizes as percentages of the pot.
  //   raise_sizes_percent: Vector of allowed raise sizes as percentages of the pot.
  //   donk_sizes_percent: Vector of allowed donk bet sizes as percentages of the pot.
  //   allow_all_in: Whether an all-in action is explicitly allowed (in addition
  //                 to potentially being reached via large bet/raise sizes).
  StreetSetting(std::vector<double> bet_sizes_percent,
                std::vector<double> raise_sizes_percent,
                std::vector<double> donk_sizes_percent,
                bool allow_all_in);

  // Default constructor (creates empty settings).
  StreetSetting() = default;

  // Allowed bet sizes as percentages of the current pot (e.g., 50.0 for 50%).
  std::vector<double> bet_sizes_percent;

  // Allowed raise sizes as percentages of the current pot (e.g., 100.0 for pot-sized raise).
  std::vector<double> raise_sizes_percent;

  // Allowed donk bet sizes (betting out of position into the previous street's aggressor)
  // as percentages of the current pot.
  std::vector<double> donk_sizes_percent;

  // Flag indicating if an explicit "all-in" action should be added,
  // regardless of calculated bet/raise sizes potentially reaching the stack limit.
  bool allow_all_in = false;
};

} // namespace config
} // namespace poker_solver

#endif // POKER_SOLVER_CONFIG_STREET_SETTING_H_
