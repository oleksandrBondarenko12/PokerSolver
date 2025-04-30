#include "gtest/gtest.h"
#include "tools/GameTreeBuildingSettings.h" // Adjust path if needed
#include "tools/StreetSetting.h"             // Adjust path if needed
#include "nodes/GameTreeNode.h"               // For GameRound enum
#include <vector>
#include <stdexcept>
#include <memory> // For std::unique_ptr

// Use namespaces
using namespace poker_solver::core;
using namespace poker_solver::config;

// Test fixture for GameTreeBuildingSettings tests
class GameTreeBuildingSettingsTest : public ::testing::Test {
 protected:
  // Create distinct StreetSetting objects for testing
  StreetSetting flop_ip_{{33.0}, {50.0}, {}, false};
  StreetSetting turn_ip_{{50.0}, {75.0}, {}, true};
  StreetSetting river_ip_{{75.0}, {100.0}, {}, true};
  StreetSetting flop_oop_{{25.0}, {}, {50.0}, false}; // Has donk
  StreetSetting turn_oop_{{50.0}, {100.0}, {}, true};
  StreetSetting river_oop_{{100.0}, {150.0}, {}, true};

  // Initialize GameTreeBuildingSettings in SetUp
  std::unique_ptr<GameTreeBuildingSettings> settings_;

  void SetUp() override {
    // Pass copies to the constructor
    settings_ = std::make_unique<GameTreeBuildingSettings>(
        flop_ip_, turn_ip_, river_ip_,
        flop_oop_, turn_oop_, river_oop_
    );
  }
};

// Test the GetSetting method for valid inputs
TEST_F(GameTreeBuildingSettingsTest, GetSettingValid) {
  ASSERT_NE(settings_, nullptr);

  // --- Verify by comparing content, not addresses ---

  // Player 0 (IP)
  const StreetSetting& actual_flop_ip = settings_->GetSetting(0, GameRound::kFlop);
  EXPECT_EQ(actual_flop_ip.bet_sizes_percent, flop_ip_.bet_sizes_percent);
  EXPECT_EQ(actual_flop_ip.raise_sizes_percent, flop_ip_.raise_sizes_percent);
  EXPECT_EQ(actual_flop_ip.allow_all_in, flop_ip_.allow_all_in);

  const StreetSetting& actual_turn_ip = settings_->GetSetting(0, GameRound::kTurn);
  EXPECT_EQ(actual_turn_ip.bet_sizes_percent, turn_ip_.bet_sizes_percent);
  EXPECT_EQ(actual_turn_ip.raise_sizes_percent, turn_ip_.raise_sizes_percent);
  EXPECT_EQ(actual_turn_ip.allow_all_in, turn_ip_.allow_all_in);

  const StreetSetting& actual_river_ip = settings_->GetSetting(0, GameRound::kRiver);
  EXPECT_EQ(actual_river_ip.bet_sizes_percent, river_ip_.bet_sizes_percent);
  EXPECT_EQ(actual_river_ip.raise_sizes_percent, river_ip_.raise_sizes_percent);
  EXPECT_EQ(actual_river_ip.allow_all_in, river_ip_.allow_all_in);


  // Player 1 (OOP)
  const StreetSetting& actual_flop_oop = settings_->GetSetting(1, GameRound::kFlop);
  EXPECT_EQ(actual_flop_oop.bet_sizes_percent, flop_oop_.bet_sizes_percent);
  EXPECT_EQ(actual_flop_oop.donk_sizes_percent, flop_oop_.donk_sizes_percent); // Check donk
  EXPECT_EQ(actual_flop_oop.allow_all_in, flop_oop_.allow_all_in);

  const StreetSetting& actual_turn_oop = settings_->GetSetting(1, GameRound::kTurn);
  EXPECT_EQ(actual_turn_oop.bet_sizes_percent, turn_oop_.bet_sizes_percent);
  EXPECT_EQ(actual_turn_oop.raise_sizes_percent, turn_oop_.raise_sizes_percent);
  EXPECT_EQ(actual_turn_oop.allow_all_in, turn_oop_.allow_all_in);

  const StreetSetting& actual_river_oop = settings_->GetSetting(1, GameRound::kRiver);
  EXPECT_EQ(actual_river_oop.bet_sizes_percent, river_oop_.bet_sizes_percent);
  EXPECT_EQ(actual_river_oop.raise_sizes_percent, river_oop_.raise_sizes_percent);
  EXPECT_EQ(actual_river_oop.allow_all_in, river_oop_.allow_all_in);
}

// Test GetSetting for invalid inputs
TEST_F(GameTreeBuildingSettingsTest, GetSettingInvalid) {
  ASSERT_NE(settings_, nullptr);

  // Invalid player index
  EXPECT_THROW(settings_->GetSetting(2, GameRound::kFlop), std::out_of_range);
  EXPECT_THROW(settings_->GetSetting(99, GameRound::kTurn), std::out_of_range);

  // Invalid round (Preflop)
  EXPECT_THROW(settings_->GetSetting(0, GameRound::kPreflop), std::invalid_argument);
  EXPECT_THROW(settings_->GetSetting(1, GameRound::kPreflop), std::invalid_argument);
}

// Test default constructor (optional, if needed)
TEST(GameTreeBuildingSettingsDefaultTest, DefaultConstructor) {
    GameTreeBuildingSettings default_settings;
    // Default settings should be empty or have default StreetSetting values
    EXPECT_TRUE(default_settings.flop_ip_setting.bet_sizes_percent.empty());
    EXPECT_FALSE(default_settings.turn_oop_setting.allow_all_in);
}
