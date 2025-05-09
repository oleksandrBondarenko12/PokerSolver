#include "gtest/gtest.h"
#include "tools/Rule.h"                          // Adjust path
#include "tools/GameTreeBuildingSettings.h" // Adjust path
#include "tools/StreetSetting.h"              // Adjust path
#include "Deck.h"                          // Adjust path
#include "nodes/GameTreeNode.h"                // For GameRound
#include <vector>
#include <stdexcept>
#include <memory> // For std::unique_ptr

// Use namespaces
using namespace poker_solver::core;
using namespace poker_solver::config;

// Test fixture for Rule tests
class RuleTest : public ::testing::Test {
 protected:
  Deck deck_; 

  StreetSetting flop_ip_{{33.0}, {50.0}, {}, false};
  StreetSetting turn_ip_{{50.0}, {75.0}, {}, true};
  StreetSetting river_ip_{{75.0}, {100.0}, {}, true};
  StreetSetting flop_oop_{{25.0}, {}, {50.0}, false};
  StreetSetting turn_oop_{{50.0}, {100.0}, {}, true};
  StreetSetting river_oop_{{100.0}, {150.0}, {}, true};

  GameTreeBuildingSettings build_settings_{
      flop_ip_, turn_ip_, river_ip_,
      flop_oop_, turn_oop_, river_oop_
  };

  double initial_oop_commit_ = 50.0;
  double initial_ip_commit_ = 100.0;
  GameRound starting_round_ = GameRound::kFlop;
  std::vector<int> initial_board_for_flop_start_ = { // Example for kFlop
      Card::StringToInt("Ac").value(),
      Card::StringToInt("Kd").value(),
      Card::StringToInt("5h").value()
  };
  int raise_limit_ = 3;
  double small_blind_ = 1.0;
  double big_blind_ = 2.0;
  double stack_ = 200.0;
  double threshold_ = 0.95;

  std::unique_ptr<Rule> rule_;

  void SetUp() override {
       rule_ = std::make_unique<Rule>(
           deck_, initial_oop_commit_, initial_ip_commit_, starting_round_,
           initial_board_for_flop_start_, // <<< PASS INITIAL BOARD HERE
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
  EXPECT_EQ(rule_->GetInitialBoardCardsInt(), initial_board_for_flop_start_); // Test new getter
  EXPECT_EQ(rule_->GetRaiseLimitPerStreet(), raise_limit_);
  EXPECT_EQ(rule_->GetSmallBlind(), small_blind_);
  EXPECT_EQ(rule_->GetBigBlind(), big_blind_);
  EXPECT_EQ(rule_->GetInitialEffectiveStack(), stack_);
  EXPECT_EQ(rule_->GetAllInThresholdRatio(), threshold_);

  EXPECT_EQ(rule_->GetDeck().GetCards().size(), kNumCardsInDeck);

  EXPECT_EQ(rule_->GetBuildSettings().flop_ip_setting.bet_sizes_percent,
            build_settings_.flop_ip_setting.bet_sizes_percent);
}

// Test pot and commitment calculations
TEST_F(RuleTest, PotAndCommitmentGetters) {
   ASSERT_NE(rule_, nullptr);

   EXPECT_DOUBLE_EQ(rule_->GetInitialPot(), initial_oop_commit_ + initial_ip_commit_);
   EXPECT_DOUBLE_EQ(rule_->GetInitialCommitment(0), initial_ip_commit_);
   EXPECT_DOUBLE_EQ(rule_->GetInitialCommitment(1), initial_oop_commit_);

   EXPECT_THROW(rule_->GetInitialCommitment(2), std::out_of_range);
}

// Test constructor validation
TEST(RuleConstructorValidationTest, InvalidInputs) {
  Deck deck;
  StreetSetting ss;
  GameTreeBuildingSettings bs(ss, ss, ss, ss, ss, ss);
  std::vector<int> empty_board = {}; // For tests where board isn't the focus
  std::vector<int> flop_board = {0,1,2}; // Dummy 3 cards for flop start if needed

  // Negative amounts
  EXPECT_THROW(Rule(deck, -1, 100, GameRound::kFlop, flop_board, 3, 1, 2, 200, bs), std::invalid_argument);
  EXPECT_THROW(Rule(deck, 50, -1, GameRound::kFlop, flop_board, 3, 1, 2, 200, bs), std::invalid_argument);
  EXPECT_THROW(Rule(deck, 50, 100, GameRound::kFlop, flop_board, 3, -1, 2, 200, bs), std::invalid_argument);
  EXPECT_THROW(Rule(deck, 50, 100, GameRound::kFlop, flop_board, 3, 1, -2, 200, bs), std::invalid_argument);
  EXPECT_THROW(Rule(deck, 50, 100, GameRound::kFlop, flop_board, 3, 1, 2, -200, bs), std::invalid_argument);
  EXPECT_THROW(Rule(deck, 50, 100, GameRound::kFlop, flop_board, 3, 1, 2, 0, bs), std::invalid_argument); // Zero stack

  // Negative raise limit
  EXPECT_THROW(Rule(deck, 50, 100, GameRound::kFlop, flop_board, -1, 1, 2, 200, bs), std::invalid_argument);

  // Invalid threshold
  EXPECT_THROW(Rule(deck, 50, 100, GameRound::kFlop, flop_board, 3, 1, 2, 200, bs, -0.1), std::invalid_argument);
  EXPECT_THROW(Rule(deck, 50, 100, GameRound::kFlop, flop_board, 3, 1, 2, 200, bs, 1.1), std::invalid_argument);

  // Valid construction should not throw (assuming GameRound::kPreflop for empty_board)
  EXPECT_NO_THROW(Rule(deck, 50, 100, GameRound::kPreflop, empty_board, 3, 1, 2, 200, bs, 0.98));
  EXPECT_NO_THROW(Rule(deck, 50, 100, GameRound::kFlop, flop_board, 3, 1, 2, 200, bs, 0.98));
}
