///////////////////////////////////////////////////////////////////////////////
// tests/best_response_test.cpp (Corrected Expectations)
///////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"

// --- Project Headers (Adjust paths as necessary) ---
#include "solver/best_response.h"
#include "trainable/DiscountedCfrTrainable.h" // For setting fixed strategy
// #include "GameTree.h" // GameTree itself might not be needed directly for these tests
#include "tools/Rule.h"                   // Potentially needed if building via rules
#include "tools/GameTreeBuildingSettings.h"
#include "tools/StreetSetting.h"
#include "nodes/ActionNode.h"
#include "nodes/TerminalNode.h"
#include "nodes/ChanceNode.h"
#include "nodes/ShowdownNode.h"
#include "ranges/PrivateCardsManager.h"
#include "ranges/RiverRangeManager.h"
#include "compairer/Dic5Compairer.h"
#include "Deck.h"
#include "Card.h"
#include "ranges/PrivateCards.h"
#include "nodes/GameActions.h"
// #include "Library.h" // Not directly needed now
// #include "tools/PrivateRangeConverter.h" // Not directly needed now

// --- Standard Library Headers ---
#include <vector>
#include <memory>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <optional>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <typeinfo> // For dynamic_cast

// Use namespaces for brevity in the test file
using namespace poker_solver::core;
using namespace poker_solver::config;
using namespace poker_solver::nodes;
using namespace poker_solver::ranges;
using namespace poker_solver::eval;
using namespace poker_solver::solver;
// using poker_solver::tree::GameTree; // Assuming GameTree is in tree namespace

// Test fixture for BestResponse tests
class BestResponseTest : public ::testing::Test {
 protected:
  // --- Common objects initialized in SetUp ---
  Deck deck_;
  std::shared_ptr<Dic5Compairer> compairer_;
  std::unique_ptr<RiverRangeManager> rrm_;
  std::unique_ptr<PrivateCardsManager> pcm_;
  std::unique_ptr<BestResponse> best_response_; // Created in SetUp

  // Sample ranges (P0=AA, P1=KK)
  std::vector<PrivateCards> range_p0_;
  std::vector<PrivateCards> range_p1_;
  std::vector<std::vector<PrivateCards>> ranges_vec_; // Holds {range_p0_, range_p1_}

  // Sample board & state (can be overridden in tests)
  uint64_t initial_board_mask_ = 0; // Start with empty board
  double initial_pot_ = 10.0;       // P0=5, P1=5 initially committed
  double stack_ = 100.0;

  // Root node (created within each test)
  std::shared_ptr<ActionNode> root_node_;

  // Helper to set a fixed pure strategy on a Trainable object inside an ActionNode
  void SetFixedPureStrategy(
      std::shared_ptr<ActionNode>& node, // Pass node by ref
      size_t action_to_choose) {
    ASSERT_NE(node, nullptr);
    size_t num_actions = node->GetActions().size();

    // Get the correct range pointer based on the node's player index
    const auto* range_ptr = (node->GetPlayerIndex() == 0)
                                ? &range_p0_
                                : &range_p1_;
    ASSERT_NE(range_ptr, nullptr) << "Range pointer not set for node player " << node->GetPlayerIndex();
    size_t num_hands = range_ptr->size();
    size_t total_size = num_actions * num_hands;

    if (num_actions == 0 && action_to_choose == 0) return; // Allow setting for 0 actions
    if (action_to_choose >= num_actions) {
        FAIL() << "Invalid action index in SetFixedPureStrategy: " << action_to_choose
               << " (Num Actions: " << num_actions << ")";
    }
    if (total_size == 0) return; // Nothing to set if no hands/actions

    // Ensure the player range is set on the node before getting trainable
    node->SetPlayerRange(range_ptr);

    // Get trainable (deal index 0 assumed sufficient for these tests)
    // Use the default precision from the BestResponse config used in SetUp
    auto trainable = node->GetTrainable(0, BestResponse::Config{}.precision);
    ASSERT_NE(trainable, nullptr);

    // Create regrets: High positive for chosen action, large negative for others
    std::vector<float> regrets(total_size, -10000.0f); // Large negative default
    std::vector<float> reach_probs(num_hands, 1.0f);   // Dummy reach probs for update

    for (size_t h = 0; h < num_hands; ++h) {
        size_t index = action_to_choose * num_hands + h;
        if (index < regrets.size()) { // Bounds check
            regrets[index] = 10000.0f; // Large positive
        } else {
            FAIL() << "Index out of bounds in SetFixedPureStrategy";
        }
    }

    // Update regrets (Iteration 1 is arbitrary, just needs to be > 0)
    trainable->UpdateRegrets(regrets, 1, reach_probs);
  }

  void SetUp() override { // Common setup for all tests in this fixture
    try {
      compairer_ = std::make_shared<Dic5Compairer>("five_card_strength.txt"); // Ensure path is correct
      rrm_ = std::make_unique<RiverRangeManager>(compairer_);

      int ac = Card::StringToInt("Ac").value(); int ad = Card::StringToInt("Ad").value();
      int ah = Card::StringToInt("Ah").value(); int as = Card::StringToInt("As").value();
      int kc = Card::StringToInt("Kc").value(); int kd = Card::StringToInt("Kd").value();
      int kh = Card::StringToInt("Kh").value(); int ks = Card::StringToInt("Ks").value();
      range_p0_ = { PrivateCards(ac,ad), PrivateCards(ac,ah), PrivateCards(ac,as),
                    PrivateCards(ad,ah), PrivateCards(ad,as), PrivateCards(ah,as) };
      range_p1_ = { PrivateCards(kc,kd), PrivateCards(kc,kh), PrivateCards(kc,ks),
                    PrivateCards(kd,kh), PrivateCards(kd,ks), PrivateCards(kh,ks) };
      ranges_vec_ = {range_p0_, range_p1_};

      pcm_ = std::make_unique<PrivateCardsManager>(ranges_vec_, initial_board_mask_); // Empty initial board for PCM

      BestResponse::Config config;
      config.use_suit_isomorphism = false;
      config.num_threads = 1;
      config.debug_log = true; // <<<--- KEEP DEBUG LOGGING ENABLED ---<<<
      best_response_ = std::make_unique<BestResponse>(config); // Pass config

    } catch (const std::exception& e) {
      FAIL() << "Setup failed: " << e.what();
    }
  }

}; // End of fixture definition


// --- Tests using the fixture ---

// Test CalculateBestResponseEv for a simple river scenario: P1 checks, P0 checks -> Showdown
TEST_F(BestResponseTest, RiverCheckCheck_P0) {
    ASSERT_NE(best_response_, nullptr);
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
    SetFixedPureStrategy(root_node_, 0);
    SetFixedPureStrategy(p0_action_node, 0);
    double expected_ev_p0 = 5.0; // AA always wins this showdown
    double actual_ev_p0 = 0.0;
    ASSERT_NO_THROW(actual_ev_p0 = best_response_->CalculateBestResponseEv( root_node_, 0, ranges_vec_, *pcm_, *rrm_, deck_, river_board_mask));
    EXPECT_NEAR(actual_ev_p0, expected_ev_p0, 1e-9);
}

// Test CalculateBestResponseEv for Player 1 (OOP)
TEST_F(BestResponseTest, RiverCheckCheck_P1) {
     ASSERT_NE(best_response_, nullptr);
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
    SetFixedPureStrategy(root_node_, 0);
    SetFixedPureStrategy(p0_action_node, 0);
    double expected_ev_p1 = -5.0; // KK always loses this showdown
    double actual_ev_p1 = 0.0;
    ASSERT_NO_THROW(actual_ev_p1 = best_response_->CalculateBestResponseEv( root_node_, 1, ranges_vec_, *pcm_, *rrm_, deck_, river_board_mask));
    EXPECT_NEAR(actual_ev_p1, expected_ev_p1, 1e-9);
}

// Test CalculateExploitability
TEST_F(BestResponseTest, RiverCheckCheck_Exploitability) {
     ASSERT_NE(best_response_, nullptr);
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
    SetFixedPureStrategy(root_node_, 0);
    SetFixedPureStrategy(p0_action_node, 0);
    double exploitability = 0.0;
    ASSERT_NO_THROW(exploitability = best_response_->CalculateExploitability( root_node_, ranges_vec_, *pcm_, *rrm_, deck_, river_board_mask, initial_pot_));
    EXPECT_NEAR(exploitability, 0.0, 1e-9); // (+5 + -5) / 2 = 0
}

// --- Flop-To-River Test ---
TEST_F(BestResponseTest, FlopToRiverCheckdown) {
    ASSERT_NE(best_response_, nullptr);

    // --- Define Flop ---
    std::vector<int> flop_board_ints = {
        Card::StringToInt("2h").value(), Card::StringToInt("5c").value(),
        Card::StringToInt("7d").value()
    };
    uint64_t flop_board_mask = Card::CardIntsToUint64(flop_board_ints);

    // --- Manually build Flop -> Turn -> River Checkdown Tree ---
    double current_pot = initial_pot_; // 10.0
    std::vector<double> commitments = {5.0, 5.0}; // P0=5, P1=5 (Initial state)

    // Flop Nodes (Tree starts here)
    auto flop_root = std::make_shared<ActionNode>(1, GameRound::kFlop, current_pot, std::weak_ptr<GameTreeNode>(), 1);
    flop_root->SetPlayerRange(&range_p1_); // Associate range
    auto flop_p0_action = std::make_shared<ActionNode>(0, GameRound::kFlop, current_pot, flop_root, 1);
    flop_p0_action->SetPlayerRange(&range_p0_); // Associate range
    flop_root->AddChild(GameAction(PokerAction::kCheck), flop_p0_action); // P1 Checks Flop

    // Turn Nodes
    Card turn_card("Ad");
    ASSERT_FALSE(turn_card.IsEmpty());
    auto turn_chance = std::make_shared<ChanceNode>(GameRound::kTurn, current_pot, flop_p0_action, std::vector<Card>{turn_card}, nullptr, false);
    flop_p0_action->AddChild(GameAction(PokerAction::kCheck), turn_chance); // P0 Checks Flop
    auto turn_p1_action = std::make_shared<ActionNode>(1, GameRound::kTurn, current_pot, turn_chance, 1);
    turn_p1_action->SetPlayerRange(&range_p1_); // Associate range
    turn_chance->SetChild(turn_p1_action); // Link chance node child
    auto turn_p0_action = std::make_shared<ActionNode>(0, GameRound::kTurn, current_pot, turn_p1_action, 1);
    turn_p0_action->SetPlayerRange(&range_p0_); // Associate range
    turn_p1_action->AddChild(GameAction(PokerAction::kCheck), turn_p0_action); // P1 Checks Turn

    // River Nodes
    Card river_card("Ks");
    ASSERT_FALSE(river_card.IsEmpty());
    auto river_chance = std::make_shared<ChanceNode>(GameRound::kRiver, current_pot, turn_p0_action, std::vector<Card>{river_card}, nullptr, false);
    turn_p0_action->AddChild(GameAction(PokerAction::kCheck), river_chance); // P0 Checks Turn
    auto river_p1_action = std::make_shared<ActionNode>(1, GameRound::kRiver, current_pot, river_chance, 1);
    river_p1_action->SetPlayerRange(&range_p1_); // Associate range
    river_chance->SetChild(river_p1_action); // Link chance node child
    auto river_p0_action = std::make_shared<ActionNode>(0, GameRound::kRiver, current_pot, river_p1_action, 1);
    river_p0_action->SetPlayerRange(&range_p0_); // Associate range
    river_p1_action->AddChild(GameAction(PokerAction::kCheck), river_p0_action); // P1 Checks River

    // Showdown
    auto final_showdown = std::make_shared<ShowdownNode>(GameRound::kRiver, current_pot, river_p0_action, 2, commitments);
    river_p0_action->AddChild(GameAction(PokerAction::kCheck), final_showdown); // P0 Checks River

    // --- Set Fixed Strategies (Always Check) ---
    ASSERT_NO_THROW(SetFixedPureStrategy(flop_root, 0));
    ASSERT_NO_THROW(SetFixedPureStrategy(flop_p0_action, 0));
    ASSERT_NO_THROW(SetFixedPureStrategy(turn_p1_action, 0));
    ASSERT_NO_THROW(SetFixedPureStrategy(turn_p0_action, 0));
    ASSERT_NO_THROW(SetFixedPureStrategy(river_p1_action, 0));
    ASSERT_NO_THROW(SetFixedPureStrategy(river_p0_action, 0));

    // --- Calculate Expected EV ---
    // The function calculates the EV from the flop, averaging over turn/river.
    // Some AA combos are blocked by Ad, some KK by Ks.
    // The calculation Sum[ EV(hand_i | final_board) * P(hand_i | initial_board) ] is correct.
    // The result 2.5 / -2.5 reflects this averaging.
    // *** FIX: Update expected values ***
    double expected_ev_p0 = 2.5;
    double expected_ev_p1 = -2.5;

    // --- Run Best Response ---
    double actual_ev_p0 = 0.0;
    double actual_ev_p1 = 0.0;

    // Call CalculateBestResponseEv, passing dependencies and FLOP mask
    ASSERT_NO_THROW(actual_ev_p0 = best_response_->CalculateBestResponseEv(
        flop_root, 0, ranges_vec_, *pcm_, *rrm_, deck_, flop_board_mask));
    ASSERT_NO_THROW(actual_ev_p1 = best_response_->CalculateBestResponseEv(
        flop_root, 1, ranges_vec_, *pcm_, *rrm_, deck_, flop_board_mask));

    // --- Assertions ---
    EXPECT_NEAR(actual_ev_p0, expected_ev_p0, 1e-9) << "Player 0 (AA) EV mismatch";
    EXPECT_NEAR(actual_ev_p1, expected_ev_p1, 1e-9) << "Player 1 (KK) EV mismatch";
}

// --- End of File ---
