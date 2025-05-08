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

    // Helper vector for AccumulateAverageStrategy tests
    std::vector<double> reach_probs_player_vector; // Holds per-hand weights

    void SetUp() override {
        try {
            // 1. Create Range (remains the same)
            player_range_.emplace_back(Card::StringToInt("Ac").value(), Card::StringToInt("Kc").value()); // Hand 0
            player_range_.emplace_back(Card::StringToInt("Ad").value(), Card::StringToInt("Kd").value()); // Hand 1
            ASSERT_EQ(player_range_.size(), kNumHands);

            // Initialize helper reach prob vector (e.g., uniform for simplicity in test)
            reach_probs_player_vector.assign(kNumHands, 1.0 / static_cast<double>(kNumHands)); // Example: 0.5, 0.5

            // 2. Create ActionNode (remains the same)
            action_node_ = std::make_shared<ActionNode>(
                0, GameRound::kFlop, 10.0, std::weak_ptr<GameTreeNode>(), 1);

            // Add dummy children/actions (remains the same)
            auto dummy_child_terminal = std::make_shared<TerminalNode>(std::vector<double>{0.0, 0.0}, GameRound::kFlop, 10.0, std::weak_ptr<GameTreeNode>(action_node_));
            action_node_->AddChild(GameAction(PokerAction::kCheck), dummy_child_terminal); // Action 0
            action_node_->AddChild(GameAction(PokerAction::kBet, 5.0), dummy_child_terminal); // Action 1
            ASSERT_EQ(action_node_->GetActions().size(), kNumActions);

            // 3. Set Range on Node (remains the same)
            action_node_->SetPlayerRange(&player_range_);

            // 4. Create Trainable
            trainable_ = std::make_unique<DiscountedCfrTrainable>(
                &player_range_, // Pass pointer to range
                *action_node_   // Pass reference to node
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

    double expected_prob = (kNumActions > 0) ? 1.0 / static_cast<double>(kNumActions) : 0.0; // Use double
    for(double prob : current_strategy) { EXPECT_DOUBLE_EQ(prob, expected_prob); } // Use double
    for(double prob : avg_strategy) { EXPECT_DOUBLE_EQ(prob, expected_prob); } // Use double
}

TEST_F(DiscountedCfrTrainableTest, UpdateAndGetStrategies) {
    ASSERT_NE(trainable_, nullptr);

    // Iteration 1
    // Use Hand-Major Regrets: H0=[1,-1], H1=[-2,2] -> Flat = [1.0, -1.0, -2.0, 2.0]
    std::vector<double> regrets1 = {1.0, -1.0, -2.0, 2.0};
    ASSERT_EQ(regrets1.size(), kTotalSize);

    const double test_scalar_weight_regret = 1.0;
    ASSERT_NO_THROW(trainable_->UpdateRegrets(regrets1, 1, test_scalar_weight_regret));
    ASSERT_NO_THROW(trainable_->AccumulateAverageStrategy(trainable_->GetCurrentStrategy(), 1, reach_probs_player_vector)); // Pass vector

    // Expected current (Hand-Major: H0A0, H0A1, H1A0, H1A1): [1, 0, 0, 1]
    const auto& current_strategy1 = trainable_->GetCurrentStrategy();
    ASSERT_EQ(current_strategy1.size(), kTotalSize);
    EXPECT_NEAR(current_strategy1[0], 1.0, 1e-9); // H0A0
    EXPECT_NEAR(current_strategy1[1], 0.0, 1e-9); // H0A1
    EXPECT_NEAR(current_strategy1[2], 0.0, 1e-9); // H1A0
    EXPECT_NEAR(current_strategy1[3], 1.0, 1e-9); // H1A1


    // Expected average (Hand-Major: H0A0, H0A1, H1A0, H1A1): [1, 0, 0, 1]
    const auto& avg_strategy1 = trainable_->GetAverageStrategy();
    ASSERT_EQ(avg_strategy1.size(), kTotalSize);
    EXPECT_NEAR(avg_strategy1[0], 1.0, 1e-9); // H0A0
    EXPECT_NEAR(avg_strategy1[1], 0.0, 1e-9); // H0A1
    EXPECT_NEAR(avg_strategy1[2], 0.0, 1e-9); // H1A0
    EXPECT_NEAR(avg_strategy1[3], 1.0, 1e-9); // H1A1

    // Iteration 2
    // Use Hand-Major Regrets: H0=[1,1], H1=[1,1] -> Flat = [1.0, 1.0, 1.0, 1.0]
    std::vector<double> regrets2 = {1.0, 1.0, 1.0, 1.0};
    ASSERT_EQ(regrets2.size(), kTotalSize);
    ASSERT_NO_THROW(trainable_->UpdateRegrets(regrets2, 2, test_scalar_weight_regret));
    ASSERT_NO_THROW(trainable_->AccumulateAverageStrategy(trainable_->GetCurrentStrategy(), 2, reach_probs_player_vector)); // Pass vector

    // Expected current (Hand-Major: H0A0, H0A1, H1A0, H1A1): [0.8076, 0.1924, 0.0, 1.0]
    const auto& current_strategy2 = trainable_->GetCurrentStrategy();
    ASSERT_EQ(current_strategy2.size(), kTotalSize);
    EXPECT_NEAR(current_strategy2[0], 0.807605, 1e-5); // H0A0
    EXPECT_NEAR(current_strategy2[1], 0.192395, 1e-5); // H0A1 <- Corrected expectation index
    EXPECT_NEAR(current_strategy2[2], 0.0,      1e-5); // H1A0 <- Corrected expectation index
    EXPECT_NEAR(current_strategy2[3], 1.0,      1e-5); // H1A1

    // Expected average (Hand-Major: H0A0, H0A1, H1A0, H1A1): [0.8461, 0.1539, 0.0, 1.0]
    const auto& avg_strategy2 = trainable_->GetAverageStrategy();
    ASSERT_EQ(avg_strategy2.size(), kTotalSize);
    EXPECT_NEAR(avg_strategy2[0], 0.846090, 1e-5); // H0A0
    EXPECT_NEAR(avg_strategy2[1], 0.153910, 1e-5); // H0A1 <- Corrected expectation index
    EXPECT_NEAR(avg_strategy2[2], 0.0,      1e-5); // H1A0 <- Corrected expectation index
    EXPECT_NEAR(avg_strategy2[3], 1.0,      1e-5); // H1A1
}

TEST_F(DiscountedCfrTrainableTest, SetAndDumpEvs) {
     ASSERT_NE(trainable_, nullptr);
     std::vector<double> evs(kTotalSize); // Use double
     for(size_t i=0; i<kTotalSize; ++i) evs[i] = static_cast<double>(i) * 1.5; // Use double
     ASSERT_NO_THROW(trainable_->SetEv(evs));
     json ev_dump = trainable_->DumpEvs();
     ASSERT_TRUE(ev_dump.is_object());
     ASSERT_TRUE(ev_dump.contains("evs"));
     ASSERT_TRUE(ev_dump.contains("actions"));
     ASSERT_TRUE(ev_dump["evs"].is_object());

     // Check if EVs match for each hand
     ASSERT_EQ(ev_dump["evs"].size(), kNumHands);
     for(size_t h=0; h<kNumHands; ++h) {
         const auto& hand = player_range_[h];
         std::string hand_str = hand.ToString();
         ASSERT_TRUE(ev_dump["evs"].contains(hand_str));
         ASSERT_TRUE(ev_dump["evs"][hand_str].is_array());
         ASSERT_EQ(ev_dump["evs"][hand_str].size(), kNumActions);

         // Using Hand-Major: index = h * num_actions + a
         for(size_t a = 0; a < kNumActions; ++a) {
             size_t flat_idx = h * kNumActions + a; // Use kNumActions fixture constant
             ASSERT_LT(flat_idx, evs.size()) << "Flat index out of bounds"; // Add bounds check
             EXPECT_DOUBLE_EQ(ev_dump["evs"][hand_str][a].get<double>(), evs[flat_idx]) << "EV mismatch at hand " << h << ", action " << a << ", flat_idx " << flat_idx; // Compare double
         }
     }
}

TEST_F(DiscountedCfrTrainableTest, DumpStrategy) {
     ASSERT_NE(trainable_, nullptr);
     json strategy_dump_no_ev = trainable_->DumpStrategy(false);
     json strategy_dump_with_ev = trainable_->DumpStrategy(true);

     ASSERT_TRUE(strategy_dump_no_ev.is_object());
     ASSERT_TRUE(strategy_dump_no_ev.contains("strategy"));
     ASSERT_TRUE(strategy_dump_no_ev.contains("actions"));
     ASSERT_FALSE(strategy_dump_no_ev.contains("evs"));
     ASSERT_TRUE(strategy_dump_no_ev["strategy"].is_object());
     ASSERT_EQ(strategy_dump_no_ev["strategy"].size(), kNumHands);

     ASSERT_TRUE(strategy_dump_with_ev.is_object());
     ASSERT_TRUE(strategy_dump_with_ev.contains("strategy"));
     ASSERT_TRUE(strategy_dump_with_ev.contains("actions"));
     ASSERT_TRUE(strategy_dump_with_ev.contains("evs"));
     ASSERT_TRUE(strategy_dump_with_ev["strategy"].is_object());
     ASSERT_TRUE(strategy_dump_with_ev["evs"].is_object());
     ASSERT_EQ(strategy_dump_with_ev["strategy"].size(), kNumHands);
     ASSERT_EQ(strategy_dump_with_ev["evs"].size(), kNumHands);
}

TEST_F(DiscountedCfrTrainableTest, CopyStateFrom) {
    // ARRANGE
    auto source_trainable = std::make_unique<DiscountedCfrTrainable>(
        &player_range_,
        *action_node_
    );

    size_t total_size = kNumActions * kNumHands;
    std::vector<double> regrets1(total_size);
    std::vector<double> regrets2(total_size);
    std::vector<double> current_strategy1(total_size);
    const auto& initial_strat_ref = source_trainable->GetCurrentStrategy();
    std::copy(initial_strat_ref.begin(), initial_strat_ref.end(), current_strategy1.begin());

    std::vector<double> evs1(total_size);

    for (size_t i = 0; i < total_size; ++i) {
        regrets1[i] = static_cast<double>(i % 5) - 2.0;
        regrets2[i] = static_cast<double>(i % 3) * 0.5 - 0.5;
        evs1[i] = static_cast<double>(i) * 0.1;
    }
    const double test_scalar_weight_regret = 1.0;
    ASSERT_NO_THROW(source_trainable->UpdateRegrets(regrets1, 1, test_scalar_weight_regret));
    ASSERT_NO_THROW(source_trainable->AccumulateAverageStrategy(current_strategy1, 1, reach_probs_player_vector)); // Use fixture vector
    ASSERT_NO_THROW(source_trainable->UpdateRegrets(regrets2, 2, test_scalar_weight_regret));
    ASSERT_NO_THROW(source_trainable->AccumulateAverageStrategy(source_trainable->GetCurrentStrategy(), 2, reach_probs_player_vector)); // Use fixture vector
    ASSERT_NO_THROW(source_trainable->SetEv(evs1));


    // Get the expected state from the source *after* updates
    std::vector<double> expected_current_strategy = source_trainable->GetCurrentStrategy();
    std::vector<double> expected_average_strategy = source_trainable->GetAverageStrategy();
    json expected_evs_json = source_trainable->DumpEvs();

    // ACT
    ASSERT_NO_THROW(trainable_->CopyStateFrom(*source_trainable)); // trainable_ is the fixture member (destination)

    // ASSERT
    // 1. Compare Current Strategy
    std::vector<double> actual_current_strategy = trainable_->GetCurrentStrategy();
    ASSERT_EQ(actual_current_strategy.size(), expected_current_strategy.size());
    for (size_t i = 0; i < actual_current_strategy.size(); ++i) {
        EXPECT_DOUBLE_EQ(actual_current_strategy[i], expected_current_strategy[i])
            << "Current strategy differs at index " << i;
    }
    // 2. Compare Average Strategy
    std::vector<double> actual_average_strategy = trainable_->GetAverageStrategy();
    ASSERT_EQ(actual_average_strategy.size(), expected_average_strategy.size());
    for (size_t i = 0; i < actual_average_strategy.size(); ++i) {
        EXPECT_DOUBLE_EQ(actual_average_strategy[i], expected_average_strategy[i])
            << "Average strategy differs at index " << i;
    }
    // 3. Compare EVs
    json actual_evs_json = trainable_->DumpEvs();
    EXPECT_EQ(actual_evs_json, expected_evs_json) << "EVs differ after copying state";
}


TEST_F(DiscountedCfrTrainableTest, CopyStateFromThrowsOnIncompatibleType) {
    // ARRANGE
    class DummyTrainable : public Trainable {
        public:
         const std::vector<double>& GetCurrentStrategy() const override { static std::vector<double> v; return v;}
         const std::vector<double>& GetAverageStrategy() const override { static std::vector<double> v; return v;}
         void UpdateRegrets(const std::vector<double>&, int, double) override {}
         void AccumulateAverageStrategy(const std::vector<double>&, int, const std::vector<double>&) override {}
         void SetEv(const std::vector<double>&) override {}
         json DumpStrategy(bool) const override { return nullptr; }
         json DumpEvs() const override { return nullptr; }
         void CopyStateFrom(const Trainable&) override {}
    };
    DummyTrainable incompatible_source;

    // ACT & ASSERT
    EXPECT_THROW(trainable_->CopyStateFrom(incompatible_source), std::invalid_argument);
}