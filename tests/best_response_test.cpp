#include "gtest/gtest.h"
#include "solver/best_response.h"           // Adjust path
#include "trainable/DiscountedCfrTrainable.h"// Adjust path
#include "GameTree.h"                 // Adjust path
#include "tools/Rule.h"                    // Adjust path
#include "tools/GameTreeBuildingSettings.h" // Adjust path
#include "tools/StreetSetting.h"        // Adjust path
#include "nodes/ActionNode.h"            // Adjust path
#include "nodes/TerminalNode.h"          // Adjust path
#include "nodes/ChanceNode.h"            // Adjust path
#include "nodes/ShowdownNode.h"          // Adjust path
#include "ranges/PrivateCardsManager.h" // Adjust path
#include "ranges/RiverRangeManager.h"   // Adjust path
#include "compairer/Dic5Compairer.h"          // Adjust path
#include "Deck.h"                    // Adjust path
#include "Card.h"                    // Adjust path
#include "ranges/PrivateCards.h"           // Adjust path
#include "nodes/GameActions.h" 
#include <vector>
#include <memory>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <optional>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <json.hpp>
#include <typeinfo>

// Use namespaces
using namespace poker_solver::core;
using namespace poker_solver::config;
using namespace poker_solver::nodes;
using namespace poker_solver::ranges;
using namespace poker_solver::eval;
using namespace poker_solver::solver;
using poker_solver::tree::GameTree;
using json = nlohmann::json;

// Test fixture for BestResponse tests
class BestResponseTest : public ::testing::Test {
 protected:
  // --- Setup common objects ---
  Deck deck_;
  std::shared_ptr<Dic5Compairer> compairer_;
  std::unique_ptr<RiverRangeManager> rrm_;
  std::unique_ptr<PrivateCardsManager> pcm_;
  std::unique_ptr<BestResponse> best_response_;

  // Sample ranges (very simple: P0=AA, P1=KK)
  std::vector<PrivateCards> range_p0_;
  std::vector<PrivateCards> range_p1_;
  std::vector<std::vector<PrivateCards>> ranges_vec_;

  // Sample board & pot
  uint64_t board_mask_ = 0; // Start with empty board for simplicity
  double initial_pot_ = 10.0; // P0=5, P1=5 initially committed
  double stack_ = 100.0;

  // Root node of our minimal tree (created in test, not SetUp)
  std::shared_ptr<ActionNode> root_node_;


  // Helper to set a fixed pure strategy via UpdateRegrets
  void SetFixedPureStrategy(
      std::shared_ptr<ActionNode>& node, // Pass node by ref
      size_t action_to_choose) {
      // ... (Keep the helper function as corrected before) ...
      ASSERT_NE(node, nullptr); size_t num_actions = node->GetActions().size();
      const auto* range_ptr = (node->GetPlayerIndex() == 0) ? &range_p0_ : &range_p1_;
      ASSERT_NE(range_ptr, nullptr); size_t num_hands = range_ptr->size();
      size_t total_size = num_actions * num_hands;
      if (action_to_choose >= num_actions) FAIL() << "Invalid action index";
      if (total_size == 0) return;
      std::vector<float> regrets(total_size, -1000.0f); std::vector<float> reach_probs(num_hands, 1.0f);
      for (size_t h = 0; h < num_hands; ++h) { size_t index = action_to_choose * num_hands + h; if (index < regrets.size()) regrets[index] = 1000.0f; else FAIL(); }
      auto trainable = node->GetTrainable(0); ASSERT_NE(trainable, nullptr);
      trainable->UpdateRegrets(regrets, 1, reach_probs);
      auto dcfr_trainable = std::dynamic_pointer_cast<DiscountedCfrTrainable>(trainable);
      if(dcfr_trainable) { [[maybe_unused]] const auto& avg_strat = dcfr_trainable->GetAverageStrategy(); }
  }


  void SetUp() override { // Common setup for all tests in this fixture
    try {
      // 1. Compairer and RRM
      compairer_ = std::make_shared<Dic5Compairer>("five_card_strength.txt");
      rrm_ = std::make_unique<RiverRangeManager>(compairer_);

      // 2. Simple Ranges (AA vs KK)
      int ac = Card::StringToInt("Ac").value(); int ad = Card::StringToInt("Ad").value();
      int ah = Card::StringToInt("Ah").value(); int as = Card::StringToInt("As").value();
      int kc = Card::StringToInt("Kc").value(); int kd = Card::StringToInt("Kd").value();
      int kh = Card::StringToInt("Kh").value(); int ks = Card::StringToInt("Ks").value();
      range_p0_ = { PrivateCards(ac,ad), PrivateCards(ac,ah), PrivateCards(ac,as),
                    PrivateCards(ad,ah), PrivateCards(ad,as), PrivateCards(ah,as) };
      range_p1_ = { PrivateCards(kc,kd), PrivateCards(kc,kh), PrivateCards(kc,ks),
                    PrivateCards(kd,kh), PrivateCards(kd,ks), PrivateCards(kh,ks) };
      ranges_vec_ = {range_p0_, range_p1_};

      // 3. PrivateCardsManager (Empty initial board)
      pcm_ = std::make_unique<PrivateCardsManager>(ranges_vec_, board_mask_);


      // 4. Create BestResponse instance (Tree built inside each test now)
      BestResponse::Config config;
      config.use_suit_isomorphism = false;
      config.num_threads = 1;
      best_response_ = std::make_unique<BestResponse>(ranges_vec_, *pcm_, *rrm_, deck_, config);

    } catch (const std::exception& e) {
      FAIL() << "Setup failed: " << e.what();
    }
  }

   // Helper to convert hand vector back to string for messages
    std::string HandVectorToString(const std::vector<std::string>& hand_strs) {
        std::ostringstream oss; // Use ostringstream
        oss << "{";
        for(size_t i = 0; i < hand_strs.size(); ++i) {
            oss << hand_strs[i] << (i == hand_strs.size() - 1 ? "" : ", ");
        }
        oss << "}";
        return oss.str();
    }
}; // End of fixture definition


// --- Tests using the fixture ---

// Test CalculateBestResponseEv for a simple river scenario: P1 checks, P0 checks -> Showdown
TEST_F(BestResponseTest, RiverCheckCheck) { // Renamed from MinimalTree_CalcBREV_P0
    ASSERT_NE(best_response_, nullptr);

    // --- Manually build the simple tree ---
    GameRound current_round = GameRound::kRiver; // Start on River
    // Define River Board
    std::vector<int> river_board_ints = {
        Card::StringToInt("2h").value(), Card::StringToInt("5c").value(),
        Card::StringToInt("7d").value(), Card::StringToInt("Ts").value(),
        Card::StringToInt("Jc").value()
    };
    uint64_t river_board_mask = Card::CardIntsToUint64(river_board_ints);

    // Root: P1 (OOP) action on river, pot 10.0
    root_node_ = std::make_shared<ActionNode>(1, current_round, initial_pot_, std::weak_ptr<GameTreeNode>(), 1);
    root_node_->SetPlayerRange(&range_p1_);

    // Child 1: P1 Checks -> P0 (IP) action node
    auto p0_action_node = std::make_shared<ActionNode>(0, current_round, initial_pot_, root_node_, 1);
    p0_action_node->SetPlayerRange(&range_p0_);
    root_node_->AddChild(GameAction(PokerAction::kCheck), p0_action_node);

    // Child 2: P0 Checks -> Showdown node
    // Commits are still initial 5.0 each for this path.
    auto showdown_node = std::make_shared<ShowdownNode>(current_round, initial_pot_, p0_action_node, 2, std::vector<double>{5.0, 5.0});
    p0_action_node->AddChild(GameAction(PokerAction::kCheck), showdown_node);

    // --- Set Fixed Strategies ---
    SetFixedPureStrategy(root_node_, 0); // P1 always Checks
    SetFixedPureStrategy(p0_action_node, 0); // P0 always Checks
    // --- End Tree Build ---

    // P0 (AA) vs P1 (KK) on 2h 5c 7d Ts Jc. P0 wins.
    // Path: P1 Checks, P0 Checks -> Showdown.
    // P0 Best Response: Check (fixed strat matches BR). Showdown. P0 wins pot=10. Net EV = +5.
    double expected_ev_p0 = 5.0;
    double actual_ev_p0 = 0.0;
    // Pass the actual river board mask
    ASSERT_NO_THROW(actual_ev_p0 = best_response_->CalculateBestResponseEv(root_node_, 0, river_board_mask));
    EXPECT_NEAR(actual_ev_p0, expected_ev_p0, 1e-9);
}

// Test CalculateBestResponseEv for Player 1 (OOP)
TEST_F(BestResponseTest, RiverCheckCheck_P1) { // Renamed from MinimalTree_CalcBREV_P1
     ASSERT_NE(best_response_, nullptr);

    // --- Manually build the simple tree (River Check/Check) ---
    GameRound current_round = GameRound::kRiver;
    std::vector<int> river_board_ints = { Card::StringToInt("2h").value(), Card::StringToInt("5c").value(), Card::StringToInt("7d").value(), Card::StringToInt("Ts").value(), Card::StringToInt("Jc").value() };
    uint64_t river_board_mask = Card::CardIntsToUint64(river_board_ints);
    root_node_ = std::make_shared<ActionNode>(1, current_round, initial_pot_, std::weak_ptr<GameTreeNode>(), 1);
    root_node_->SetPlayerRange(&range_p1_);
    auto p0_action_node = std::make_shared<ActionNode>(0, current_round, initial_pot_, root_node_, 1);
    p0_action_node->SetPlayerRange(&range_p0_);
    root_node_->AddChild(GameAction(PokerAction::kCheck), p0_action_node);
    auto showdown_node = std::make_shared<ShowdownNode>(current_round, initial_pot_, p0_action_node, 2, std::vector<double>{5.0, 5.0});
    p0_action_node->AddChild(GameAction(PokerAction::kCheck), showdown_node);
    // --- Set Fixed Strategies ---
    SetFixedPureStrategy(root_node_, 0); // P1 always Checks
    SetFixedPureStrategy(p0_action_node, 0); // P0 always Checks
    // --- End Tree Build ---

    // P1 (KK) vs P0 (AA) on 2h 5c 7d Ts Jc. P0 wins.
    // Path: P1 Checks, P0 Checks -> Showdown.
    // P1 Best Response: Check (fixed strat matches BR). Showdown. P1 loses pot=10. Net EV = -5.
    double expected_ev_p1 = -5.0;
     double actual_ev_p1 = 0.0;
    // Pass the actual river board mask
    ASSERT_NO_THROW(actual_ev_p1 = best_response_->CalculateBestResponseEv(root_node_, 1, river_board_mask));
    EXPECT_NEAR(actual_ev_p1, expected_ev_p1, 1e-9);
}

// Test CalculateExploitability
TEST_F(BestResponseTest, RiverCheckCheck_Exploitability) { // Renamed
     ASSERT_NE(best_response_, nullptr);

     // --- Manually build the simple tree (River Check/Check) ---
    GameRound current_round = GameRound::kRiver;
    std::vector<int> river_board_ints = { Card::StringToInt("2h").value(), Card::StringToInt("5c").value(), Card::StringToInt("7d").value(), Card::StringToInt("Ts").value(), Card::StringToInt("Jc").value() };
    uint64_t river_board_mask = Card::CardIntsToUint64(river_board_ints);
    root_node_ = std::make_shared<ActionNode>(1, current_round, initial_pot_, std::weak_ptr<GameTreeNode>(), 1);
    root_node_->SetPlayerRange(&range_p1_);
    auto p0_action_node = std::make_shared<ActionNode>(0, current_round, initial_pot_, root_node_, 1);
    p0_action_node->SetPlayerRange(&range_p0_);
    root_node_->AddChild(GameAction(PokerAction::kCheck), p0_action_node);
    auto showdown_node = std::make_shared<ShowdownNode>(current_round, initial_pot_, p0_action_node, 2, std::vector<double>{5.0, 5.0});
    p0_action_node->AddChild(GameAction(PokerAction::kCheck), showdown_node);
    // --- Set Fixed Strategies ---
    SetFixedPureStrategy(root_node_, 0); // P1 always Checks
    SetFixedPureStrategy(p0_action_node, 0); // P0 always Checks
    // --- End Tree Build ---

     // Expected Exploitability = (EV_P0_BR + EV_P1_BR) / 2 = (+5.0 + (-5.0)) / 2 = 0.0
     double exploitability = 0.0;
     // Pass the actual river board mask
     ASSERT_NO_THROW(exploitability = best_response_->CalculateExploitability(root_node_, river_board_mask, initial_pot_));
     EXPECT_NEAR(exploitability, 0.0, 1e-9);
}

// --- Flop-To-River Test ---
TEST_F(BestResponseTest, FlopToRiverCheckdown) {
    ASSERT_NE(best_response_, nullptr);

    // --- Manually build Flop -> Turn -> River Checkdown Tree ---
    double current_pot = initial_pot_; // 10.0
    std::vector<double> commitments = {5.0, 5.0}; // P0=5, P1=5

    // Flop Nodes
    auto flop_root = std::make_shared<ActionNode>(1, GameRound::kFlop, current_pot, std::weak_ptr<GameTreeNode>(), 1);
    flop_root->SetPlayerRange(&range_p1_);
    auto flop_p0_action = std::make_shared<ActionNode>(0, GameRound::kFlop, current_pot, flop_root, 1);
    flop_p0_action->SetPlayerRange(&range_p0_);
    flop_root->AddChild(GameAction(PokerAction::kCheck), flop_p0_action); // P1 Checks Flop

    // Turn Nodes
    // *** FIX: Create Card objects first ***
    Card turn_card("Ad");
    ASSERT_FALSE(turn_card.IsEmpty());
    auto turn_chance = std::make_shared<ChanceNode>(GameRound::kTurn, current_pot, flop_p0_action, std::vector<Card>{turn_card}, nullptr, false);
    flop_p0_action->AddChild(GameAction(PokerAction::kCheck), turn_chance); // P0 Checks Flop
    auto turn_p1_action = std::make_shared<ActionNode>(1, GameRound::kTurn, current_pot, turn_chance, 1);
    turn_p1_action->SetPlayerRange(&range_p1_);
    turn_chance->SetChild(turn_p1_action);
    auto turn_p0_action = std::make_shared<ActionNode>(0, GameRound::kTurn, current_pot, turn_p1_action, 1);
    turn_p0_action->SetPlayerRange(&range_p0_);
    turn_p1_action->AddChild(GameAction(PokerAction::kCheck), turn_p0_action); // P1 Checks Turn

    // River Nodes
    // *** FIX: Create Card objects first ***
    Card river_card("Ks");
    ASSERT_FALSE(river_card.IsEmpty());
    auto river_chance = std::make_shared<ChanceNode>(GameRound::kRiver, current_pot, turn_p0_action, std::vector<Card>{river_card}, nullptr, false);
    turn_p0_action->AddChild(GameAction(PokerAction::kCheck), river_chance); // P0 Checks Turn
    auto river_p1_action = std::make_shared<ActionNode>(1, GameRound::kRiver, current_pot, river_chance, 1);
    river_p1_action->SetPlayerRange(&range_p1_);
    river_chance->SetChild(river_p1_action);
    auto river_p0_action = std::make_shared<ActionNode>(0, GameRound::kRiver, current_pot, river_p1_action, 1);
    river_p0_action->SetPlayerRange(&range_p0_);
    river_p1_action->AddChild(GameAction(PokerAction::kCheck), river_p0_action); // P1 Checks River

    // Showdown
    auto final_showdown = std::make_shared<ShowdownNode>(GameRound::kRiver, current_pot, river_p0_action, 2, commitments);
    river_p0_action->AddChild(GameAction(PokerAction::kCheck), final_showdown); // P0 Checks River

    // --- Set Fixed Strategies (Always Check) ---
    SetFixedPureStrategy(flop_root, 0);
    SetFixedPureStrategy(flop_p0_action, 0);
    SetFixedPureStrategy(turn_p1_action, 0);
    SetFixedPureStrategy(turn_p0_action, 0);
    SetFixedPureStrategy(river_p1_action, 0);
    SetFixedPureStrategy(river_p0_action, 0);

    // --- Calculate Expected EV ---
    // Board will be Ad Ks + 3 random flop cards (averaged over by BestResponse)
    // P0 has AA, P1 has KK. P0 always wins showdown.
    // Path is Check-Check on all streets. Final pot = 10. P0 Net EV = +5. P1 Net EV = -5.
    double expected_ev_p0 = 5.0;
    double expected_ev_p1 = -5.0;

    // --- Run Best Response ---
    // Start with EMPTY board mask, BestResponse handles chance nodes
    uint64_t empty_board_mask = 0;
    double actual_ev_p0 = 0.0;
    double actual_ev_p1 = 0.0;

    ASSERT_NO_THROW(actual_ev_p0 = best_response_->CalculateBestResponseEv(flop_root, 0, empty_board_mask));
    ASSERT_NO_THROW(actual_ev_p1 = best_response_->CalculateBestResponseEv(flop_root, 1, empty_board_mask));

    // Allow some tolerance due to averaging over chance nodes
    EXPECT_NEAR(actual_ev_p0, expected_ev_p0, 1e-5);
    EXPECT_NEAR(actual_ev_p1, expected_ev_p1, 1e-5);
}
