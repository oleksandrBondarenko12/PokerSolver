#include "gtest/gtest.h"
#include "trainable/DiscountedCfrTrainable.h" // Adjust path
#include "nodes/ActionNode.h"              // Adjust path
#include "nodes/TerminalNode.h"            // Adjust path
#include "ranges/PrivateCards.h"             // Adjust path
#include "Card.h"                      // Adjust path
#include "nodes/GameActions.h"              // Adjust path
#include "nodes/GameTreeNode.h"            // Adjust path (needed for TerminalNode constructor)
#include <vector>
#include <memory>
#include <cmath>   // For std::abs, std::pow
#include <numeric> // For std::accumulate
#include <json.hpp> // For JSON dumping tests
#include <limits>    // For numeric_limits
#include <stdexcept> // For exceptions
#include <iostream>  // For potential debug/error output
#include <typeinfo>  // For dynamic_cast in CopyStateFrom test
#include <iomanip>   // For std::fixed, std::setprecision

// Use namespaces
using namespace poker_solver::core;
using namespace poker_solver::nodes;
using namespace poker_solver::solver;
using json = nlohmann::json;

// --- Test Fixture for DiscountedCfrTrainable ---
class DiscountedCfrTrainableTest : public ::testing::Test {
 protected:
    // Create necessary objects for testing Trainable in isolation
    std::vector<PrivateCards> player_range_;
    std::shared_ptr<ActionNode> action_node_; // Use shared_ptr for node
    std::unique_ptr<DiscountedCfrTrainable> trainable_;

    // Define constants for test dimensions
    const size_t kNumHands = 2;
    const size_t kNumActions = 2;
    const size_t kTotalSize = kNumHands * kNumActions; // 4

    void SetUp() override {
        try {
            // 1. Create Range
            player_range_.emplace_back(Card::StringToInt("Ac").value(), Card::StringToInt("Kc").value());
            player_range_.emplace_back(Card::StringToInt("Ad").value(), Card::StringToInt("Kd").value());
            ASSERT_EQ(player_range_.size(), kNumHands);

            // 2. Create ActionNode with the correct number of actions
            action_node_ = std::make_shared<ActionNode>(
                0, GameRound::kFlop, 10.0, std::weak_ptr<GameTreeNode>(), 1);

            // Add dummy children/actions to match kNumActions
            auto dummy_child = std::make_shared<TerminalNode>(std::vector<double>{0,0}, GameRound::kFlop, 10.0, action_node_);
            action_node_->AddChild(GameAction(PokerAction::kCheck), dummy_child); // Action 0
            action_node_->AddChild(GameAction(PokerAction::kBet, 5.0), dummy_child); // Action 1
            ASSERT_EQ(action_node_->GetActions().size(), kNumActions);

            // 3. Set Range on Node
            action_node_->SetPlayerRange(&player_range_);

            // 4. Create Trainable
            trainable_ = std::make_unique<DiscountedCfrTrainable>(
                &player_range_, *action_node_
            );
             ASSERT_NE(trainable_, nullptr);

        } catch (const std::exception& e) {
            FAIL() << "Setup failed: " << e.what();
        }
    }
};


// --- Tests ---

TEST_F(DiscountedCfrTrainableTest, InitialState) {
    ASSERT_NE(trainable_, nullptr);
    const auto& current_strategy = trainable_->GetCurrentStrategy();
    const auto& avg_strategy = trainable_->GetAverageStrategy();
    ASSERT_EQ(current_strategy.size(), kTotalSize);
    ASSERT_EQ(avg_strategy.size(), kTotalSize);

    float expected_prob = 1.0f / static_cast<float>(kNumActions); // 0.5
    for(float prob : current_strategy) { EXPECT_FLOAT_EQ(prob, expected_prob); }
    for(float prob : avg_strategy) { EXPECT_FLOAT_EQ(prob, expected_prob); }
}

TEST_F(DiscountedCfrTrainableTest, UpdateAndGetStrategies) {
    ASSERT_NE(trainable_, nullptr);

    // Iteration 1
    std::vector<float> regrets1 = {-1.0f, 2.0f, 1.0f, -2.0f}; // Size = kTotalSize = 4
    std::vector<float> reach_probs1 = {1.0f, 1.0f}; // Size = kNumHands = 2
    ASSERT_EQ(regrets1.size(), kTotalSize);
    ASSERT_EQ(reach_probs1.size(), kNumHands);
    ASSERT_NO_THROW(trainable_->UpdateRegrets(regrets1, 1, reach_probs1)); // Check if this throws

    // Expected current = [0.0, 1.0, 1.0, 0.0] (Order: A0H0, A0H1, A1H0, A1H1)
    const auto& current_strategy1 = trainable_->GetCurrentStrategy();
    ASSERT_EQ(current_strategy1.size(), kTotalSize);
    EXPECT_FLOAT_EQ(current_strategy1[0], 0.0f); EXPECT_FLOAT_EQ(current_strategy1[1], 1.0f);
    EXPECT_FLOAT_EQ(current_strategy1[2], 1.0f); EXPECT_FLOAT_EQ(current_strategy1[3], 0.0f);

    // Expected avg = [0.0, 1.0, 1.0, 0.0]
    const auto& avg_strategy1 = trainable_->GetAverageStrategy();
    ASSERT_EQ(avg_strategy1.size(), kTotalSize);
    EXPECT_FLOAT_EQ(avg_strategy1[0], 0.0f); EXPECT_FLOAT_EQ(avg_strategy1[1], 1.0f);
    EXPECT_FLOAT_EQ(avg_strategy1[2], 1.0f); EXPECT_FLOAT_EQ(avg_strategy1[3], 0.0f);

    // Iteration 2
    std::vector<float> regrets2 = {3.0f, 1.0f, -1.0f, -1.0f}; // Size = 4
    std::vector<float> reach_probs2 = {0.5f, 0.5f}; // Size = 2
    ASSERT_EQ(regrets2.size(), kTotalSize);
    ASSERT_EQ(reach_probs2.size(), kNumHands);
    ASSERT_NO_THROW(trainable_->UpdateRegrets(regrets2, 2, reach_probs2)); // Check if this throws

    // Expected current = [1.0, 1.0, 0.0, 0.0]
    const auto& current_strategy2 = trainable_->GetCurrentStrategy();
    ASSERT_EQ(current_strategy2.size(), kTotalSize);
    EXPECT_FLOAT_EQ(current_strategy2[0], 1.0f); EXPECT_FLOAT_EQ(current_strategy2[1], 1.0f);
    EXPECT_FLOAT_EQ(current_strategy2[2], 0.0f); EXPECT_FLOAT_EQ(current_strategy2[3], 0.0f);

    // Expected avg = [0.4706, 1.0, 0.5294, 0.0]
    const auto& avg_strategy2 = trainable_->GetAverageStrategy();
    ASSERT_EQ(avg_strategy2.size(), kTotalSize);
    EXPECT_NEAR(avg_strategy2[0], 0.470588f, 1e-5); EXPECT_NEAR(avg_strategy2[1], 1.0f, 1e-5);
    EXPECT_NEAR(avg_strategy2[2], 0.529411f, 1e-5); EXPECT_NEAR(avg_strategy2[3], 0.0f, 1e-5);
}

TEST_F(DiscountedCfrTrainableTest, SetAndDumpEvs) { /* ... as before ... */ }
TEST_F(DiscountedCfrTrainableTest, DumpStrategy) { /* ... as before ... */ }
TEST_F(DiscountedCfrTrainableTest, CopyStateFrom) { /* ... as before ... */ }

