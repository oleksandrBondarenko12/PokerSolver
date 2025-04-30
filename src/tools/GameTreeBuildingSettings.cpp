#include "tools/GameTreeBuildingSettings.h" // Adjust path if necessary

#include <utility> // For std::move

namespace poker_solver {
namespace config {

// --- Constructor ---

GameTreeBuildingSettings::GameTreeBuildingSettings(
    StreetSetting flop_ip_setting,
    StreetSetting turn_ip_setting,
    StreetSetting river_ip_setting,
    StreetSetting flop_oop_setting,
    StreetSetting turn_oop_setting,
    StreetSetting river_oop_setting)
    : flop_ip_setting(std::move(flop_ip_setting)), // Use initializer list with move
      turn_ip_setting(std::move(turn_ip_setting)),
      river_ip_setting(std::move(river_ip_setting)),
      flop_oop_setting(std::move(flop_oop_setting)),
      turn_oop_setting(std::move(turn_oop_setting)),
      river_oop_setting(std::move(river_oop_setting)) {}


// --- GetSetting Method ---

const StreetSetting& GameTreeBuildingSettings::GetSetting(
    size_t player_index,
    core::GameRound round) const {

    // Assuming player 0 is IP, player 1 is OOP
    if (player_index == 0) { // In Position (IP)
        switch (round) {
            case core::GameRound::kFlop:  return flop_ip_setting;
            case core::GameRound::kTurn:  return turn_ip_setting;
            case core::GameRound::kRiver: return river_ip_setting;
            case core::GameRound::kPreflop:
                 throw std::invalid_argument(
                    "GameTreeBuildingSettings are for postflop rounds only.");
            default: // Should not happen with valid GameRound enum
                 throw std::logic_error("Invalid GameRound encountered in GetSetting.");
        }
    } else if (player_index == 1) { // Out Of Position (OOP)
         switch (round) {
            case core::GameRound::kFlop:  return flop_oop_setting;
            case core::GameRound::kTurn:  return turn_oop_setting;
            case core::GameRound::kRiver: return river_oop_setting;
            case core::GameRound::kPreflop:
                 throw std::invalid_argument(
                    "GameTreeBuildingSettings are for postflop rounds only.");
             default:
                 throw std::logic_error("Invalid GameRound encountered in GetSetting.");
        }
    } else {
        std::ostringstream oss;
        oss << "Invalid player index in GetSetting: " << player_index
            << ". Expected 0 (IP) or 1 (OOP).";
        throw std::out_of_range(oss.str());
    }
}

} // namespace config
} // namespace poker_solver
