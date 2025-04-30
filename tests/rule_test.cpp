#include "gtest/gtest.h"
#include "tools/Rule.h"                          // Adjust path
#include "tools/GameTreeBuildingSettings.h" // Adjust path
#include "tools/StreetSetting.h"              // Adjust path
#include "Deck.h"                          // Adjust path
#include "nodes/GameTreeNode.h"                // For GameRound
#include <vector>
#include <stdexcept>

// Use namespaces
using namespace poker_solver::core;
using namespace poker_solver::config;

// Test fixture for Rule tests
class RuleTest : public ::testing::Test {
 protected:
  // Setup common objects needed for Rule construction
  Deck deck_; // Standard 52-card deck

  // Sample StreetSettings
  StreetSetting flop_ip_{{33.0}, {50.0}, {}, false};
  StreetSetting turn_ip_{{50.0}, {75.0}, {}, true};
  StreetSetting river_ip_{{75.0}, {100.0}, {}, true};
  StreetSetting flop_oop_{{25.0}, {}, {50.0}, false};
  StreetSetting turn_oop_{{50.0}, {100.0}, {}, true};
  StreetSetting river_oop_{{100.0}, {150.0}, {}, true};

  // Sample GameTreeBuildingSettings
  GameTreeBuildingSettings build_settings_{
      flop_ip_, turn_ip_, river_ip_,
      flop_oop_, turn_oop_, river_oop_
  };

  // Common rule parameters
  double initial_oop_commit_ = 50.0;
  double initial_ip_commit_ = 100.0; // Test unequal commits
  GameRound starting_round_ = GameRound::kFlop;
  int raise_limit_ = 3;
  double small_blind_ = 1.0;
  double big_blind_ = 2.0;
  double stack_ = 200.0;
  double threshold_ = 0.95;

  // Rule object (can be created in SetUp or directly in tests)
  std::unique_ptr<Rule> rule_;

  void SetUp() override {
      // Create rule in setup for convenience
       rule_ = std::make_unique<Rule>(
           deck_, initial_oop_commit_, initial_ip_commit_, starting_round_,
           raise_limit_, small_blind_, big_blind_, stack_,
           build_settings_, threshold_
       );
  }
};

// Test constructor and basic getters
TEST_F(RuleTest, ConstructorAndGetters) {
  ASSERT_NE(rule_, nullptr);

  EXPECT_EQ(rule_->GetInitialOopCommit(), initial_oop_commit_);
  EXPECT_EQ(rule_->GetInitialIpCommit(), initial_ip_commit_);
  EXPECT_EQ(rule_->GetStartingRound(), starting_round_);
  EXPECT_EQ(rule_->GetRaiseLimitPerStreet(), raise_limit_);
  EXPECT_EQ(rule_->GetSmallBlind(), small_blind_);
  EXPECT_EQ(rule_->GetBigBlind(), big_blind_);
  EXPECT_EQ(rule_->GetInitialEffectiveStack(), stack_);
  EXPECT_EQ(rule_->GetAllInThresholdRatio(), threshold_);

  // Check the deck size
  EXPECT_EQ(rule_->GetDeck().GetCards().size(), kNumCardsInDeck);

  // Check build settings (compare one element for verification)
  EXPECT_EQ(rule_->GetBuildSettings().flop_ip_setting.bet_sizes_percent,
            build_settings_.flop_ip_setting.bet_sizes_percent);
}

// Test pot and commitment calculations
TEST_F(RuleTest, PotAndCommitmentGetters) {
   ASSERT_NE(rule_, nullptr);

   EXPECT_DOUBLE_EQ(rule_->GetInitialPot(), initial_oop_commit_ + initial_ip_commit_);
   EXPECT_DOUBLE_EQ(rule_->GetInitialCommitment(0), initial_ip_commit_); // Player 0 = IP
   EXPECT_DOUBLE_EQ(rule_->GetInitialCommitment(1), initial_oop_commit_); // Player 1 = OOP

   // Test invalid player index
   EXPECT_THROW(rule_->GetInitialCommitment(2), std::out_of_range);
}

// Test constructor validation
TEST(RuleConstructorValidationTest, InvalidInputs) {
  Deck deck;
  StreetSetting ss;
  GameTreeBuildingSettings bs(ss, ss, ss, ss, ss, ss);

  // Negative amounts
  EXPECT_THROW(Rule(deck, -1, 100, GameRound::kFlop, 3, 1, 2, 200, bs), std::invalid_argument);
  EXPECT_THROW(Rule(deck, 50, -1, GameRound::kFlop, 3, 1, 2, 200, bs), std::invalid_argument);
  EXPECT_THROW(Rule(deck, 50, 100, GameRound::kFlop, 3, -1, 2, 200, bs), std::invalid_argument);
  EXPECT_THROW(Rule(deck, 50, 100, GameRound::kFlop, 3, 1, -2, 200, bs), std::invalid_argument);
  EXPECT_THROW(Rule(deck, 50, 100, GameRound::kFlop, 3, 1, 2, -200, bs), std::invalid_argument);
  EXPECT_THROW(Rule(deck, 50, 100, GameRound::kFlop, 3, 1, 2, 0, bs), std::invalid_argument); // Zero stack

  // Negative raise limit
  EXPECT_THROW(Rule(deck, 50, 100, GameRound::kFlop, -1, 1, 2, 200, bs), std::invalid_argument);

  // Invalid threshold
  EXPECT_THROW(Rule(deck, 50, 100, GameRound::kFlop, 3, 1, 2, 200, bs, -0.1), std::invalid_argument);
  EXPECT_THROW(Rule(deck, 50, 100, GameRound::kFlop, 3, 1, 2, 200, bs, 1.1), std::invalid_argument);

  // Valid construction should not throw
  EXPECT_NO_THROW(Rule(deck, 50, 100, GameRound::kFlop, 3, 1, 2, 200, bs, 0.98));
}
