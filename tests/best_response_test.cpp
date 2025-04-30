#include "gtest/gtest.h"
#include "solver/best_response.h"           // Adjust path
#include "trainable/DiscountedCfrTrainable.h"// Adjust path (to create fixed strategies)
#include "GameTree.h"                 // Adjust path
#include "tools/Rule.h"                    // Adjust path
#include "tools/GameTreeBuildingSettings.h" // Adjust path
#include "tools/StreetSetting.h"        // Adjust path
#include "nodes/ActionNode.h"            // Adjust path
#include "nodes/TerminalNode.h"          // Adjust path
#include "ranges/PrivateCardsManager.h" // Adjust path
#include "ranges/RiverRangeManager.h"   // Adjust path
#include "compairer/Dic5Compairer.h"          // Adjust path
#include "Deck.h"                    // Adjust path
#include "Card.h"                    // Adjust path
#include "ranges/PrivateCards.h"           // Adjust path
#include "nodes/ChanceNode.h"
#include "nodes/ShowdownNode.h"
#include <vector>
#include <memory>
#include <cmath>
#include <numeric>

// Use namespaces
using namespace poker_solver::core;
using namespace poker_solver::config;
using namespace poker_solver::nodes;
using namespace poker_solver::ranges;
using namespace poker_solver::eval;
using namespace poker_solver::solver;
using poker_solver::tree::GameTree;

// Test fixture for BestResponse tests
class BestResponseTest : public ::testing::Test {
 protected:
  // --- Setup common objects ---
  Deck deck_;
  std::shared_ptr<Dic5Compairer> compairer_;
  std::unique_ptr<RiverRangeManager> rrm_;
  std::unique_ptr<PrivateCardsManager> pcm_;
  std::unique_ptr<BestResponse> best_response_;
  std::unique_ptr<GameTree> game_tree_; // Restore this line

  // Sample ranges (very simple: P0=AA, P1=KK)
  std::vector<PrivateCards> range_p0_;
  std::vector<PrivateCards> range_p1_;
  std::vector<std::vector<PrivateCards>> ranges_vec_;

  // Sample board
  uint64_t board_mask_ = 0; // Start with empty board for simplicity
  double initial_pot_ = 10.0;

  // Helper to create a simple fixed strategy Trainable
  std::shared_ptr<DiscountedCfrTrainable> CreateFixedStrategyTrainable(
      ActionNode& node, const std::vector<PrivateCards>* range,
      const std::vector<float>& strategy_per_hand) {
      // Use only public API: set uniform strategy by updating regrets with large positive/negative values
      auto trainable = std::make_shared<DiscountedCfrTrainable>(range, node);
      // This is a hack: set the average strategy by calling UpdateRegrets with a vector that will produce the desired strategy
      // (For a real test, DiscountedCfrTrainable should provide a public setter for strategy)
      size_t n = strategy_per_hand.size();
      std::vector<float> regrets(n, 0.0f);
      for (size_t i = 0; i < n; ++i) {
          regrets[i] = strategy_per_hand[i] > 0.99f ? 1000.0f : -1000.0f;
      }
      std::vector<float> reach_probs(range ? range->size() : 0, 1.0f);
      trainable->UpdateRegrets(regrets, 1, reach_probs);
      return trainable;
  }

  void SetUp() override {
    try {
      // 1. Compairer and RRM
      compairer_ = std::make_shared<Dic5Compairer>("five_card_strength.txt"); // Needs dictionary file
      rrm_ = std::make_unique<RiverRangeManager>(compairer_);

      // 2. Simple Ranges (AA vs KK)
      int ac = Card::StringToInt("Ac").value(); int ad = Card::StringToInt("Ad").value();
      int ah = Card::StringToInt("Ah").value(); int as = Card::StringToInt("As").value();
      int kc = Card::StringToInt("Kc").value(); int kd = Card::StringToInt("Kd").value();
      int kh = Card::StringToInt("Kh").value(); int ks = Card::StringToInt("Ks").value();
      range_p0_ = { PrivateCards(ac,ad), PrivateCards(ac,ah), PrivateCards(ac,as),
                    PrivateCards(ad,ah), PrivateCards(ad,as), PrivateCards(ah,as) }; // 6 combos AA
      range_p1_ = { PrivateCards(kc,kd), PrivateCards(kc,kh), PrivateCards(kc,ks),
                    PrivateCards(kd,kh), PrivateCards(kd,ks), PrivateCards(kh,ks) }; // 6 combos KK
      ranges_vec_ = {range_p0_, range_p1_};

      // 3. PrivateCardsManager
      pcm_ = std::make_unique<PrivateCardsManager>(ranges_vec_, board_mask_);

      // 4. Build a minimal GameTree using Rule and GameTree
      StreetSetting flop_setting({50.0}, {100.0}, {}, true); // Allow bet and raise, all-in allowed
      GameTreeBuildingSettings settings(flop_setting, flop_setting, flop_setting, flop_setting, flop_setting, flop_setting);
      Rule rule(deck_, 5.0, 5.0, GameRound::kFlop, 3, 0.5, 1.0, 100.0, settings);

      game_tree_ = std::make_unique<GameTree>(rule); // Use dynamic builder

      // Set player ranges and fixed strategies on the actual nodes in the tree
      auto root = std::dynamic_pointer_cast<ActionNode>(game_tree_->GetRoot());
      std::cerr << "[DEBUG] Root node actions size: " << root->GetActions().size() << std::endl;
      std::cerr << "[DEBUG] Root node children size: " << root->GetChildren().size() << std::endl;
      root->SetPlayerRange(&range_p1_);

      // Set fixed strategy for root (always check)
      size_t num_actions_root = root->GetActions().size();
      for (size_t i = 0; i < num_actions_root; ++i) {
          std::cerr << "[DEBUG] Root action " << i << ": " << root->GetActions()[i].ToString() << std::endl;
      }
      size_t num_hands_p1 = range_p1_.size();
      std::vector<float> p1_root_strat(num_actions_root * num_hands_p1, 0.0f);
      size_t check_idx = 0;
      for (size_t i = 0; i < num_actions_root; ++i) {
        if (root->GetActions()[i].GetAction() == PokerAction::kCheck) {
          check_idx = i;
          break;
        }
      }
      for (size_t h = 0; h < num_hands_p1; ++h)
        p1_root_strat[check_idx * num_hands_p1 + h] = 1.0f;
      std::cerr << "[DEBUG] Root: num_actions=" << num_actions_root
                << ", num_hands=" << num_hands_p1
                << ", strat_size=" << p1_root_strat.size() << std::endl;
      root->GetTrainable(0) = CreateFixedStrategyTrainable(*root, &range_p1_, p1_root_strat);

      // Next node: IP action after check
      auto ip_action_node = std::dynamic_pointer_cast<ActionNode>(root->GetChildren()[check_idx]);
      std::cerr << "[DEBUG] IP node actions size: " << ip_action_node->GetActions().size() << std::endl;
      std::cerr << "[DEBUG] IP node children size: " << ip_action_node->GetChildren().size() << std::endl;
      ip_action_node->SetPlayerRange(&range_p0_);
      size_t num_actions_ip = ip_action_node->GetActions().size();
      for (size_t i = 0; i < num_actions_ip; ++i) {
          std::cerr << "[DEBUG] IP action " << i << ": " << ip_action_node->GetActions()[i].ToString() << std::endl;
      }
      size_t num_hands_p0 = range_p0_.size();
      std::vector<float> p0_ip_strat(num_actions_ip * num_hands_p0, 0.0f);
      size_t bet_idx = 0;
      for (size_t i = 0; i < num_actions_ip; ++i) {
        if (ip_action_node->GetActions()[i].GetAction() == PokerAction::kBet) {
          bet_idx = i;
          break;
        }
      }
      for (size_t h = 0; h < num_hands_p0; ++h)
        p0_ip_strat[bet_idx * num_hands_p0 + h] = 1.0f;
      std::cerr << "[DEBUG] IP Action: num_actions=" << num_actions_ip
                << ", num_hands=" << num_hands_p0
                << ", strat_size=" << p0_ip_strat.size() << std::endl;
      ip_action_node->GetTrainable(0) = CreateFixedStrategyTrainable(*ip_action_node, &range_p0_, p0_ip_strat);

      // Next node: OOP response to bet
      auto oop_response_node = std::dynamic_pointer_cast<ActionNode>(ip_action_node->GetChildren()[bet_idx]);
      std::cerr << "[DEBUG] OOP node actions size: " << oop_response_node->GetActions().size() << std::endl;
      std::cerr << "[DEBUG] OOP node children size: " << oop_response_node->GetChildren().size() << std::endl;
      oop_response_node->SetPlayerRange(&range_p1_);
      size_t num_actions_oop = oop_response_node->GetActions().size();
      for (size_t i = 0; i < num_actions_oop; ++i) {
          std::cerr << "[DEBUG] OOP Response action " << i << ": " << oop_response_node->GetActions()[i].ToString() << std::endl;
      }
      std::vector<float> p1_response_strat(num_actions_oop * num_hands_p1, 0.0f);
      size_t fold_idx = 0;
      for (size_t i = 0; i < num_actions_oop; ++i) {
        if (oop_response_node->GetActions()[i].GetAction() == PokerAction::kFold) {
          fold_idx = i;
          break;
        }
      }
      for (size_t h = 0; h < num_hands_p1; ++h)
        p1_response_strat[fold_idx * num_hands_p1 + h] = 1.0f;
      std::cerr << "[DEBUG] OOP Response: num_actions=" << num_actions_oop
                << ", num_hands=" << num_hands_p1
                << ", strat_size=" << p1_response_strat.size() << std::endl;
      oop_response_node->GetTrainable(0) = CreateFixedStrategyTrainable(*oop_response_node, &range_p1_, p1_response_strat);

      // 6. Create BestResponse instance
      BestResponse::Config config;
      config.use_suit_isomorphism = false;
      config.num_threads = 1;
      best_response_ = std::make_unique<BestResponse>(ranges_vec_, *pcm_, *rrm_, deck_, config);

    } catch (const std::exception& e) {
      FAIL() << "Setup failed: " << e.what();
    }
  }
};

// Test CalculateBestResponseEv for Player 0
TEST_F(BestResponseTest, CalcBREV_P0) {
    ASSERT_NE(best_response_, nullptr);
    ASSERT_NE(game_tree_, nullptr);
    ASSERT_NE(game_tree_->GetRoot(), nullptr);

    // Manual EV Calculation for P0 (Best Responder) vs Fixed Strategy:
    // P1 always checks root.
    // P0 always bets 10 when checked to.
    // P1 always folds to P0's bet.
    // Therefore, P0 reaches the node after P1 checks (pot 10). P0 bets 10. P1 folds.
    // P0 wins the initial pot (10) + P1's initial commit (5) = 15 total pot.
    // P0's net gain = P1's initial commit = 5.
    // This happens for every hand in P0's range.
    double expected_ev_p0 = 5.0;
    double actual_ev_p0 = best_response_->CalculateBestResponseEv(game_tree_->GetRoot(), 0, board_mask_);
    EXPECT_NEAR(actual_ev_p0, expected_ev_p0, 1e-9);
}

// Test CalculateBestResponseEv for Player 1
TEST_F(BestResponseTest, CalcBREV_P1) {
     ASSERT_NE(best_response_, nullptr);
     ASSERT_NE(game_tree_, nullptr);
     ASSERT_NE(game_tree_->GetRoot(), nullptr);

    // Manual EV Calculation for P1 (Best Responder) vs Fixed Strategy:
    // P1 is at root (pot 10). Fixed strategy is Check.
    // P1 Best Response: Check (EV=?) or Bet (EV=?)
    //   If P1 Checks: IP node reached. P0 Fixed Strategy = Bet 10. P1 node reached (pot 20).
    //                 P1 Fixed Strategy = Fold. Terminal node reached. P1 payoff = -5. EV(Check) = -5.
    //   If P1 Bets 10 (assume this is the only bet option for simplicity): IP node reached.
    //                 P0 Fixed Strategy = ??? (Not defined in this simple tree). Let's assume P0 folds.
    //                 Terminal node reached. P1 payoff = +5 (P0's initial commit). EV(Bet) = +5.
    // Best Response for P1 is to Bet. EV = +5.
    // NOTE: This requires modifying the tree/strategies slightly to allow P1 to bet at root.
    // Let's test against the *existing* fixed strategy first.
    // P1 Fixed Strategy: Check. P0 Fixed Strategy: Bet. P1 Fixed Strategy: Fold.
    // P1's path: Check -> Face Bet -> Fold. Payoff = -5.
    // P1 Best Response against this: Check -> Face Bet -> Call? (Requires showdown logic). Fold yields -5.
    // Let's simplify the fixed strategy: P0 Always Checks Back if Checked to.
    // P1 Root: Check. P0 Node: Check. Chance Node -> Showdown.
    // EV(Check) = Showdown EV vs P0's AA range. P1 has KK. P1 loses. Net payoff = -5.
    // Let's stick to the original fixed strategy (P0 bets, P1 folds)
    // P1 BR EV should be -5, as their only path leads to folding.
    double expected_ev_p1 = -5.0; // P1 checks, faces bet, folds according to fixed strat.
                                 // BR doesn't change opponent's fixed strategy.
    double actual_ev_p1 = best_response_->CalculateBestResponseEv(game_tree_->GetRoot(), 1, board_mask_);

    EXPECT_NEAR(actual_ev_p1, expected_ev_p1, 1e-9);
}

// Test CalculateExploitability
TEST_F(BestResponseTest, Exploitability) {
     ASSERT_NE(best_response_, nullptr);
     ASSERT_NE(game_tree_, nullptr);
     ASSERT_NE(game_tree_->GetRoot(), nullptr);

     // Based on the above:
     // P0 BR EV = +5
     // P1 BR EV = -5 (because P1's fixed strategy forces them to fold)
     // Exploitability = (EV_P0 + EV_P1) / 2 = (+5 + (-5)) / 2 = 0
     // This seems too perfect, likely due to the oversimplified tree/strategy.
     // Let's change P1's response strategy: Call if facing bet.
     // Setup modification needed for this test.
     // --- Test with original fixed strategy (P1 folds) ---
     double exploitability = best_response_->CalculateExploitability(game_tree_->GetRoot(), board_mask_, initial_pot_);
     // Expecting (5 + (-5)) / 2 = 0
     EXPECT_NEAR(exploitability, 0.0, 1e-9);

     // --- TODO: Add test with a different fixed strategy (e.g., P1 calls) ---
     // This would require setting up a new tree/strategy profile within the test.
}
