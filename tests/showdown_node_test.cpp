#include "gtest/gtest.h"
#include "nodes/ShowdownNode.h"    // Adjust path
#include "nodes/ActionNode.h"      // Adjust path (for dummy parent)
#include "nodes/GameTreeNode.h"    // Adjust path
#include "compairer/Compairer.h"         // For ComparisonResult enum
#include <vector>
#include <memory>
#include <stdexcept>

// Use namespaces
using namespace poker_solver::core;
using namespace poker_solver::nodes;

// Test fixture for ShowdownNode tests
class ShowdownNodeTest : public ::testing::Test {
 protected:
  // Sample data
  GameRound round_ = GameRound::kRiver;
  double pot_ = 200.0; // Final pot
  size_t num_players_ = 2;
  // Initial commitments (e.g., both players put in 100)
  std::vector<double> commitments_ = {100.0, 100.0};

  // Dummy parent
  std::shared_ptr<ActionNode> parent_node_;

  // ShowdownNode instance
  std::unique_ptr<ShowdownNode> showdown_node_;

  void SetUp() override {
      try {
        parent_node_ = std::make_shared<ActionNode>(
            0, GameRound::kRiver, pot_, std::weak_ptr<GameTreeNode>(), 1);

        showdown_node_ = std::make_unique<ShowdownNode>(
            round_, pot_,
            std::weak_ptr<GameTreeNode>(parent_node_), // Pass weak_ptr
            num_players_,
            commitments_
        );
      } catch (const std::exception& e) {
          FAIL() << "Setup failed: " << e.what();
      }
  }
};

// Test constructor and basic getters
TEST_F(ShowdownNodeTest, ConstructorAndGetters) {
  ASSERT_NE(showdown_node_, nullptr);
  EXPECT_EQ(showdown_node_->GetRound(), round_);
  EXPECT_EQ(showdown_node_->GetPot(), pot_);
  EXPECT_EQ(showdown_node_->GetNodeType(), GameTreeNodeType::kShowdown);
  ASSERT_NE(showdown_node_->GetParent(), nullptr);
  EXPECT_EQ(showdown_node_->GetParent(), parent_node_);
}

// Test GetPayoffs method
TEST_F(ShowdownNodeTest, GetPayoffs) {
  ASSERT_NE(showdown_node_, nullptr);

  // Expected Payoffs (Net change from start of hand)
  // P0 wins: P0 gains P1's commitment (+100), P1 loses commitment (-100) -> {100.0, -100.0}
  // P1 wins: P0 loses commitment (-100), P1 gains P0's commitment (+100) -> {-100.0, 100.0}
  // Tie: Each gets pot/2 back. Net = pot/2 - commit. -> {(200/2)-100, (200/2)-100} = {0.0, 0.0}
  std::vector<double> expected_p0_wins = {100.0, -100.0};
  std::vector<double> expected_p1_wins = {-100.0, 100.0};
  std::vector<double> expected_tie = {0.0, 0.0};

  const auto& actual_p0_wins = showdown_node_->GetPayoffs(ComparisonResult::kPlayer1Wins);
  const auto& actual_p1_wins = showdown_node_->GetPayoffs(ComparisonResult::kPlayer2Wins);
  const auto& actual_tie = showdown_node_->GetPayoffs(ComparisonResult::kTie);

  ASSERT_EQ(actual_p0_wins.size(), num_players_);
  ASSERT_EQ(actual_p1_wins.size(), num_players_);
  ASSERT_EQ(actual_tie.size(), num_players_);

  // Compare vectors element by element using EXPECT_DOUBLE_EQ
  for(size_t i = 0; i < num_players_; ++i) {
      EXPECT_DOUBLE_EQ(actual_p0_wins[i], expected_p0_wins[i]) << "Mismatch at index " << i << " for P0 Wins";
      EXPECT_DOUBLE_EQ(actual_p1_wins[i], expected_p1_wins[i]) << "Mismatch at index " << i << " for P1 Wins";
      EXPECT_DOUBLE_EQ(actual_tie[i], expected_tie[i]) << "Mismatch at index " << i << " for Tie";
  }
}

// Test constructor validation (optional, covered partly by fixture)
TEST(ShowdownNodeConstructorValidation, InvalidInputs) {
    auto parent = std::make_shared<ActionNode>(0, GameRound::kRiver, 100.0, std::weak_ptr<GameTreeNode>(), 1);
    std::vector<double> commits = {50.0, 50.0};

    // Invalid number of players
    EXPECT_THROW(ShowdownNode(GameRound::kRiver, 100.0, parent, 1, {50.0}), std::invalid_argument);
    EXPECT_THROW(ShowdownNode(GameRound::kRiver, 100.0, parent, 3, {50.0, 50.0, 50.0}), std::invalid_argument);

    // Mismatched commitments size
    EXPECT_THROW(ShowdownNode(GameRound::kRiver, 100.0, parent, 2, {50.0}), std::invalid_argument);
    EXPECT_THROW(ShowdownNode(GameRound::kRiver, 100.0, parent, 2, {50.0, 50.0, 50.0}), std::invalid_argument);

    // Negative commitments
     EXPECT_THROW(ShowdownNode(GameRound::kRiver, 100.0, parent, 2, {-50.0, 150.0}), std::invalid_argument);
}
