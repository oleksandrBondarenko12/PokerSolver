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

// Helper function to print vector contents
template<typename T>
void PrintVector(const std::string& name, const std::vector<T>& vec) {
    std::cout << "  " << name << ": [";
    for (size_t i = 0; i < vec.size(); ++i) {
        std::cout << vec[i] << (i == vec.size() - 1 ? "" : ", ");
    }
    std::cout << "]" << std::endl;
}

// Simplified Test fixture for DiscountedCfrTrainable tests
class DiscountedCfrTrainableDirectTest : public ::testing::Test {
 protected:
  // Constants for this test
  static constexpr size_t kNumActions = 2;
  static constexpr size_t kNumHands = 2;
  static constexpr size_t kTotalSize = kNumActions * kNumHands; // 4

  // Sample data
  size_t player_index_ = 0;
  GameRound round_ = GameRound::kFlop;
  double pot_ = 10.0;
  size_t num_deals_ = 1; // Perfect recall

  // Sample range (2 hands)
  std::vector<PrivateCards> player_range_;
  // Sample actions (Check, Bet)
  GameAction action_check_{PokerAction::kCheck};
  GameAction action_bet_{PokerAction::kBet, 5.0};
  // ActionNode (needs to be created dynamically for Trainable constructor)
  std::shared_ptr<ActionNode> action_node_ptr_;
  // Trainable instance
  std::unique_ptr<DiscountedCfrTrainable> trainable_;

  void SetUp() override {
    try {
      player_range_.emplace_back(Card::StringToInt("Ac").value(), Card::StringToInt("Kc").value()); // Hand 0
      player_range_.emplace_back(Card::StringToInt("Ad").value(), Card::StringToInt("Kd").value()); // Hand 1
      ASSERT_EQ(player_range_.size(), kNumHands); // Verify range size

      action_node_ptr_ = std::make_shared<ActionNode>(
          player_index_, round_, pot_, std::weak_ptr<GameTreeNode>(), num_deals_);

      // Add exactly kNumActions children/actions
      auto child1 = std::make_shared<TerminalNode>(std::vector<double>{0,0}, round_, pot_, action_node_ptr_);
      auto child2 = std::make_shared<TerminalNode>(std::vector<double>{0,0}, round_, pot_+5.0, action_node_ptr_);
      action_node_ptr_->AddChild(action_check_, child1);
      action_node_ptr_->AddChild(action_bet_, child2);
      ASSERT_EQ(action_node_ptr_->GetActions().size(), kNumActions); // Verify action count

      action_node_ptr_->SetPlayerRange(&player_range_);

      // Create the trainable AFTER the node is fully configured
      trainable_ = std::make_unique<DiscountedCfrTrainable>(
          &player_range_, *action_node_ptr_
      );

    } catch (const std::exception& e) {
      FAIL() << "Setup failed: " << e.what();
    }
  }
};

// Test initial state
TEST_F(DiscountedCfrTrainableDirectTest, InitialState) {
    ASSERT_NE(trainable_, nullptr);

    // Initial strategy should be uniform
    const auto& current_strategy = trainable_->GetCurrentStrategy();
    const auto& avg_strategy = trainable_->GetAverageStrategy();
    ASSERT_EQ(current_strategy.size(), kTotalSize);
    ASSERT_EQ(avg_strategy.size(), kTotalSize);

    float expected_prob = 1.0f / static_cast<float>(kNumActions); // 0.5
    for(float prob : current_strategy) {
        EXPECT_FLOAT_EQ(prob, expected_prob);
    }
    for(float prob : avg_strategy) {
        EXPECT_FLOAT_EQ(prob, expected_prob);
    }
}

// Test UpdateRegrets and strategy calculation
TEST_F(DiscountedCfrTrainableDirectTest, UpdateAndGetStrategies) {
    ASSERT_NE(trainable_, nullptr);

    // Iteration 1: Regrets favoring Action 1 (Bet) for Hand 0, Action 0 (Check) for Hand 1
    // Regrets = [Action0Hand0, Action0Hand1, Action1Hand0, Action1Hand1]
    std::vector<float> regrets1 = {-1.0f, 2.0f, 1.0f, -2.0f};
    std::vector<float> reach_probs1 = {1.0f, 1.0f}; // Simple reach probs
    ASSERT_EQ(regrets1.size(), kTotalSize); // Check input size
    ASSERT_EQ(reach_probs1.size(), kNumHands); // Check input size
    ASSERT_NO_THROW(trainable_->UpdateRegrets(regrets1, 1, reach_probs1));

    // Check Current Strategy (Iter 1)
    // Hand 0: Regrets (-1, 1). Positive sum = 1. Strategy = [0/1, 1/1] = [0.0, 1.0]
    // Hand 1: Regrets (2, -2). Positive sum = 2. Strategy = [2/2, 0/2] = [1.0, 0.0]
    // Expected current = [0.0, 1.0, 1.0, 0.0] (Order: A0H0, A0H1, A1H0, A1H1)
    const auto& current_strategy1 = trainable_->GetCurrentStrategy();
    ASSERT_EQ(current_strategy1.size(), kTotalSize);
    EXPECT_FLOAT_EQ(current_strategy1[0], 0.0f); // A0H0
    EXPECT_FLOAT_EQ(current_strategy1[1], 1.0f); // A0H1
    EXPECT_FLOAT_EQ(current_strategy1[2], 1.0f); // A1H0
    EXPECT_FLOAT_EQ(current_strategy1[3], 0.0f); // A1H1

    // Check Average Strategy (Iter 1) - based on cumulative sum update
    // gamma = 2. gamma_discount = (1/2)^2 = 0.25
    // weighted_gamma = 0.25 * reach_prob = 0.25
    // cum_sum[0] = 0.25 * 0.0 = 0.0
    // cum_sum[1] = 0.25 * 1.0 = 0.25
    // cum_sum[2] = 0.25 * 1.0 = 0.25
    // cum_sum[3] = 0.25 * 0.0 = 0.0
    // Hand 0 sum = 0.25. Avg = [0.0/0.25, 0.25/0.25] = [0.0, 1.0]
    // Hand 1 sum = 0.25. Avg = [0.25/0.25, 0.0/0.25] = [1.0, 0.0]
    // Expected avg = [0.0, 1.0, 1.0, 0.0] (Order: A0H0, A0H1, A1H0, A1H1)
    const auto& avg_strategy1 = trainable_->GetAverageStrategy();
    ASSERT_EQ(avg_strategy1.size(), kTotalSize);
    EXPECT_FLOAT_EQ(avg_strategy1[0], 0.0f);
    EXPECT_FLOAT_EQ(avg_strategy1[1], 1.0f);
    EXPECT_FLOAT_EQ(avg_strategy1[2], 1.0f);
    EXPECT_FLOAT_EQ(avg_strategy1[3], 0.0f);

    // Iteration 2: Regrets favoring Action 0 (Check) for both hands
    std::vector<float> regrets2 = {3.0f, 1.0f, -1.0f, -1.0f};
    std::vector<float> reach_probs2 = {0.5f, 0.5f}; // Different reach probs
    ASSERT_EQ(regrets2.size(), kTotalSize); // Check input size
    ASSERT_EQ(reach_probs2.size(), kNumHands); // Check input size
    ASSERT_NO_THROW(trainable_->UpdateRegrets(regrets2, 2, reach_probs2));

    // Check Current Strategy (Iter 2)
    // alpha = 1.5, beta = 0.5
    // alpha_discount = 2^1.5 / (1 + 2^1.5) ~= 2.828 / 3.828 ~= 0.7388
    // beta_discount = 2^0.5 / (1 + 2^0.5) ~= 1.414 / 2.414 ~= 0.5858
    // CumRegret[0] = -1.0 * 0.5858 + 3.0 = 2.4142  (A0H0)
    // CumRegret[1] = 2.0 * 0.7388 + 1.0 = 2.4776   (A0H1)
    // CumRegret[2] = 1.0 * 0.7388 - 1.0 = -0.2612  (A1H0)
    // CumRegret[3] = -2.0 * 0.5858 - 1.0 = -2.1716 (A1H1)
    // Hand 0: Regrets (2.4142, -0.2612). Pos sum = 2.4142. Strategy = [1.0, 0.0]
    // Hand 1: Regrets (2.4776, -2.1716). Pos sum = 2.4776. Strategy = [1.0, 0.0]
    // Expected current = [1.0, 1.0, 0.0, 0.0] (Order: A0H0, A0H1, A1H0, A1H1)
    const auto& current_strategy2 = trainable_->GetCurrentStrategy();
    ASSERT_EQ(current_strategy2.size(), kTotalSize);
    EXPECT_FLOAT_EQ(current_strategy2[0], 1.0f); // A0H0
    EXPECT_FLOAT_EQ(current_strategy2[1], 1.0f); // A0H1
    EXPECT_FLOAT_EQ(current_strategy2[2], 0.0f); // A1H0
    EXPECT_FLOAT_EQ(current_strategy2[3], 0.0f); // A1H1

    // Check Average Strategy (Iter 2)
    // gamma = 2. gamma_discount = (2/3)^2 ~= 0.4444
    // weighted_gamma (H0) = 0.4444 * 0.5 = 0.2222
    // weighted_gamma (H1) = 0.4444 * 0.5 = 0.2222
    // Prev cum_sum = [0.0, 0.25, 0.25, 0.0] (A0H0, A0H1, A1H0, A1H1)
    // New current strategy = [1.0, 1.0, 0.0, 0.0] (A0H0, A0H1, A1H0, A1H1)
    // cum_sum[0] = 0.0 + 0.2222 * 1.0 = 0.2222   (A0H0)
    // cum_sum[1] = 0.25 + 0.2222 * 1.0 = 0.4722  (A0H1)
    // cum_sum[2] = 0.25 + 0.2222 * 0.0 = 0.25    (A1H0)
    // cum_sum[3] = 0.0 + 0.2222 * 0.0 = 0.0     (A1H1)
    // Hand 0 sum = 0.2222 + 0.25 = 0.4722. Avg = [0.2222/0.4722, 0.25/0.4722] ~= [0.4706, 0.5294]
    // Hand 1 sum = 0.4722 + 0.0 = 0.4722. Avg = [0.4722/0.4722, 0.0/0.4722] ~= [1.0, 0.0]
    // Expected avg = [0.4706, 1.0, 0.5294, 0.0] (Order: A0H0, A0H1, A1H0, A1H1)
    const auto& avg_strategy2 = trainable_->GetAverageStrategy();
    ASSERT_EQ(avg_strategy2.size(), kTotalSize);
    EXPECT_NEAR(avg_strategy2[0], 0.470588f, 1e-5); // A0H0
    EXPECT_NEAR(avg_strategy2[1], 1.0f, 1e-5);       // A0H1
    EXPECT_NEAR(avg_strategy2[2], 0.529411f, 1e-5); // A1H0
    EXPECT_NEAR(avg_strategy2[3], 0.0f, 1e-5);       // A1H1
}

// Test SetEv and DumpEvs
TEST_F(DiscountedCfrTrainableDirectTest, SetAndDumpEvs) {
     ASSERT_NE(trainable_, nullptr);
     std::vector<float> evs = {1.1f, 2.2f, 3.3f, 4.4f}; // Order: A0H0, A0H1, A1H0, A1H1
     ASSERT_EQ(evs.size(), kTotalSize); // Check input size
     trainable_->SetEv(evs);

     json ev_dump = trainable_->DumpEvs();
     ASSERT_TRUE(ev_dump.contains("actions"));
     ASSERT_TRUE(ev_dump.contains("evs"));
     EXPECT_EQ(ev_dump["actions"].size(), kNumActions);
     EXPECT_EQ(ev_dump["evs"].size(), kNumHands); // Should have entries for 2 hands

     std::string hand0_str = player_range_[0].ToString(); // AcKc
     std::string hand1_str = player_range_[1].ToString(); // AdKd

     ASSERT_TRUE(ev_dump["evs"].contains(hand0_str));
     ASSERT_TRUE(ev_dump["evs"].contains(hand1_str));

     std::vector<float> evs_hand0 = ev_dump["evs"][hand0_str];
     std::vector<float> evs_hand1 = ev_dump["evs"][hand1_str];

     ASSERT_EQ(evs_hand0.size(), kNumActions);
     ASSERT_EQ(evs_hand1.size(), kNumActions);

     // Check values against input evs = [A0H0, A0H1, A1H0, A1H1]
     EXPECT_FLOAT_EQ(evs_hand0[0], evs[0]); // Action 0 Hand 0
     EXPECT_FLOAT_EQ(evs_hand0[1], evs[2]); // Action 1 Hand 0
     EXPECT_FLOAT_EQ(evs_hand1[0], evs[1]); // Action 0 Hand 1
     EXPECT_FLOAT_EQ(evs_hand1[1], evs[3]); // Action 1 Hand 1
}

// Test DumpStrategy
TEST_F(DiscountedCfrTrainableDirectTest, DumpStrategy) {
     ASSERT_NE(trainable_, nullptr);
     // Run one update to make average strategy non-uniform
     std::vector<float> regrets1 = {-1.0f, 2.0f, 1.0f, -2.0f};
     std::vector<float> reach_probs1 = {1.0f, 1.0f};
     ASSERT_EQ(regrets1.size(), kTotalSize);
     ASSERT_EQ(reach_probs1.size(), kNumHands);
     ASSERT_NO_THROW(trainable_->UpdateRegrets(regrets1, 1, reach_probs1));

     json strat_dump = trainable_->DumpStrategy(false); // Without EVs
     ASSERT_TRUE(strat_dump.contains("actions"));
     ASSERT_TRUE(strat_dump.contains("strategy"));
     EXPECT_FALSE(strat_dump.contains("evs"));
     EXPECT_EQ(strat_dump["actions"].size(), kNumActions);
     EXPECT_EQ(strat_dump["strategy"].size(), kNumHands); // Entries for 2 hands

     std::string hand0_str = player_range_[0].ToString(); // AcKc
     ASSERT_TRUE(strat_dump["strategy"].contains(hand0_str));
     std::vector<float> strat_hand0 = strat_dump["strategy"][hand0_str];
     ASSERT_EQ(strat_hand0.size(), kNumActions);
     // Check against average strategy calculated in UpdateAndGetStrategies test
     EXPECT_FLOAT_EQ(strat_hand0[0], 0.0f); // Action 0
     EXPECT_FLOAT_EQ(strat_hand0[1], 1.0f); // Action 1

     // Test dumping with EVs
     std::vector<float> evs = {1.1f, 2.2f, 3.3f, 4.4f};
     ASSERT_EQ(evs.size(), kTotalSize);
     trainable_->SetEv(evs);
     json strat_dump_with_ev = trainable_->DumpStrategy(true);
     ASSERT_TRUE(strat_dump_with_ev.contains("evs"));
     ASSERT_TRUE(strat_dump_with_ev["evs"].contains(hand0_str));
     EXPECT_EQ(strat_dump_with_ev["evs"][hand0_str].size(), kNumActions);
     EXPECT_FLOAT_EQ(strat_dump_with_ev["evs"][hand0_str][0], evs[0]); // A0H0
     EXPECT_FLOAT_EQ(strat_dump_with_ev["evs"][hand0_str][1], evs[2]); // A1H0

}

// Test CopyStateFrom
TEST_F(DiscountedCfrTrainableDirectTest, CopyStateFrom) {
    ASSERT_NE(trainable_, nullptr);

    // Create a second trainable instance
    auto trainable2 = std::make_unique<DiscountedCfrTrainable>(
        &player_range_, *action_node_ptr_);

    // Update the first one
    std::vector<float> regrets1 = {-1.0f, 2.0f, 1.0f, -2.0f};
    std::vector<float> reach_probs1 = {1.0f, 1.0f};
    ASSERT_EQ(regrets1.size(), kTotalSize);
    ASSERT_EQ(reach_probs1.size(), kNumHands);
    ASSERT_NO_THROW(trainable_->UpdateRegrets(regrets1, 1, reach_probs1));
    std::vector<float> evs = {1.1f, 2.2f, 3.3f, 4.4f};
    ASSERT_EQ(evs.size(), kTotalSize);
    trainable_->SetEv(evs);

    // Verify they are different initially (optional, but good check)
    // EXPECT_NE(trainable_->GetCurrentStrategy(), trainable2->GetCurrentStrategy());
    // EXPECT_NE(trainable_->GetAverageStrategy(), trainable2->GetAverageStrategy());

    // Copy state from trainable_ to trainable2
    ASSERT_NO_THROW(trainable2->CopyStateFrom(*trainable_));

    // Verify they are now the same
    EXPECT_EQ(trainable_->GetCurrentStrategy(), trainable2->GetCurrentStrategy()) << "Current strategies differ after copy.";
    EXPECT_EQ(trainable_->GetAverageStrategy(), trainable2->GetAverageStrategy()) << "Average strategies differ after copy.";

    // Check if EVs were copied (they should be based on current impl)
    json ev_dump1 = trainable_->DumpEvs();
    json ev_dump2 = trainable2->DumpEvs();
    EXPECT_EQ(ev_dump1["evs"], ev_dump2["evs"]) << "EVs differ after copy.";

     // Test copying from incompatible type (should throw)
     class MockTrainable : public Trainable {
        public:
         const std::vector<float>& GetCurrentStrategy() const override { static std::vector<float> v; return v; }
         const std::vector<float>& GetAverageStrategy() const override { static std::vector<float> v; return v; }
         void UpdateRegrets(const std::vector<float>&, int, const std::vector<float>&) override {}
         void SetEv(const std::vector<float>&) override {}
         json DumpStrategy(bool) const override { return json{}; }
         json DumpEvs() const override { return json{}; }
         void CopyStateFrom(const Trainable&) override {}
     };
     MockTrainable mock;
     EXPECT_THROW(trainable_->CopyStateFrom(mock), std::invalid_argument);
}
