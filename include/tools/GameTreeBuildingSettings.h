#ifndef POKER_SOLVER_CONFIG_GAME_TREE_BUILDING_SETTINGS_H_
#define POKER_SOLVER_CONFIG_GAME_TREE_BUILDING_SETTINGS_H_

#include "StreetSetting.h" // For StreetSetting struct
#include "nodes/GameTreeNode.h" // For GameRound enum
#include <stdexcept> // For exceptions
#include <string>    // For error messages
#include <sstream>   // For error messages

namespace poker_solver {
namespace config {

// Aggregates StreetSetting configurations for different streets and player positions.
// Used to define the allowed actions during game tree construction.
struct GameTreeBuildingSettings {
  // Constructor initializing all street/position settings.
  // Takes settings by value and moves them for potential efficiency.
  GameTreeBuildingSettings(
      StreetSetting flop_ip_setting,
      StreetSetting turn_ip_setting,
      StreetSetting river_ip_setting,
      StreetSetting flop_oop_setting,
      StreetSetting turn_oop_setting,
      StreetSetting river_oop_setting);

  // Default constructor (creates empty settings).
  GameTreeBuildingSettings() = default;

  // Retrieves the appropriate StreetSetting based on player position and round.
  // Args:
  //   player_index: The player's index (0 for IP, 1 for OOP).
  //   round: The current game round (Flop, Turn, River).
  // Returns:
  //   A const reference to the corresponding StreetSetting.
  // Throws:
  //   std::out_of_range if player_index is invalid.
  //   std::invalid_argument if round is Preflop (settings are postflop).
  const StreetSetting& GetSetting(size_t player_index,
                                  core::GameRound round) const;

  // --- Public Member Variables (Configuration Data) ---
  // Using struct with public members for simple data holding.

  StreetSetting flop_ip_setting;
  StreetSetting turn_ip_setting;
  StreetSetting river_ip_setting;
  StreetSetting flop_oop_setting;
  StreetSetting turn_oop_setting;
  StreetSetting river_oop_setting;
};

} // namespace config
} // namespace poker_solver

#endif // POKER_SOLVER_CONFIG_GAME_TREE_BUILDING_SETTINGS_H_
