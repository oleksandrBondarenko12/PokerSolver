#include "gtest/gtest.h"
#include "nodes/TerminalNode.h"    // Adjust path
#include "nodes/ActionNode.h"      // Adjust path (for dummy parent)
#include "nodes/GameTreeNode.h"    // Adjust path
#include <vector>
#include <memory>
#include <stdexcept>

// Use namespaces
using namespace poker_solver::core;
using namespace poker_solver::nodes;

// Test fixture for TerminalNode tests
class TerminalNodeTest : public ::testing::Test {
 protected:
  // Sample data
  GameRound round_ = GameRound::kTurn;
  double pot_ = 75.0; // Pot size when fold occurred
  // Payoffs if P1 folds to P0's bet (P0 wins P1's commitment)
  // Assume P0 committed 100, P1 committed 50 before folding.
  // Net for P0 = +50, Net for P1 = -50
  std::vector<double> payoffs_p0_wins_ = {50.0, -50.0};
  // Payoffs if P0 folds to P1's bet (P1 wins P0's commitment)
  // Assume P0 committed 50, P1 committed 100 before folding.
  // Net for P0 = -50, Net for P1 = +50
  std::vector<double> payoffs_p1_wins_ = {-50.0, 50.0};


  // Dummy parent
  std::shared_ptr<ActionNode> parent_node_;

  void SetUp() override {
      try {
        parent_node_ = std::make_shared<ActionNode>(
            0, round_, pot_, std::weak_ptr<GameTreeNode>(), 1);
      } catch (const std::exception& e) {
          FAIL() << "Setup failed: " << e.what();
      }
  }
};

// Test constructor and basic getters
TEST_F(TerminalNodeTest, ConstructorAndGettersP0Wins) {
    TerminalNode node(payoffs_p0_wins_, round_, pot_, parent_node_);

    EXPECT_EQ(node.GetRound(), round_);
    EXPECT_EQ(node.GetPot(), pot_);
    EXPECT_EQ(node.GetNodeType(), GameTreeNodeType::kTerminal);
    ASSERT_NE(node.GetParent(), nullptr);
    EXPECT_EQ(node.GetParent(), parent_node_);

    // Verify payoffs
    const auto& payoffs = node.GetPayoffs();
    ASSERT_EQ(payoffs.size(), 2);
    EXPECT_DOUBLE_EQ(payoffs[0], payoffs_p0_wins_[0]);
    EXPECT_DOUBLE_EQ(payoffs[1], payoffs_p0_wins_[1]);
}

TEST_F(TerminalNodeTest, ConstructorAndGettersP1Wins) {
    TerminalNode node(payoffs_p1_wins_, round_, pot_, parent_node_);

    EXPECT_EQ(node.GetRound(), round_);
    EXPECT_EQ(node.GetPot(), pot_);
    EXPECT_EQ(node.GetNodeType(), GameTreeNodeType::kTerminal);
    ASSERT_NE(node.GetParent(), nullptr);
    EXPECT_EQ(node.GetParent(), parent_node_);

    // Verify payoffs
    const auto& payoffs = node.GetPayoffs();
    ASSERT_EQ(payoffs.size(), 2);
    EXPECT_DOUBLE_EQ(payoffs[0], payoffs_p1_wins_[0]);
    EXPECT_DOUBLE_EQ(payoffs[1], payoffs_p1_wins_[1]);
}


// Test constructor validation
TEST(TerminalNodeConstructorValidation, InvalidInputs) {
    auto parent = std::make_shared<ActionNode>(0, GameRound::kTurn, 100.0, std::weak_ptr<GameTreeNode>(), 1);
    std::vector<double> valid_payoffs = {50.0, -50.0};
    std::vector<double> empty_payoffs = {};

    // Empty payoffs vector
    EXPECT_THROW(TerminalNode(empty_payoffs, GameRound::kTurn, 100.0, parent), std::invalid_argument);

    // Valid construction should not throw
    EXPECT_NO_THROW(TerminalNode(valid_payoffs, GameRound::kTurn, 100.0, parent));
}
