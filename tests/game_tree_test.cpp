#include "gtest/gtest.h"
#include "GameTree.h"           // Adjust path
#include "tools/Rule.h"              // Adjust path
#include "tools/GameTreeBuildingSettings.h" // Adjust path
#include "tools/StreetSetting.h"    // Adjust path
#include "Deck.h"                // Adjust path
#include "nodes/GameTreeNode.h"      // Adjust path
#include "nodes/ActionNode.h"        // Adjust path
#include "nodes/ChanceNode.h"        // Adjust path
#include "nodes/TerminalNode.h"      // Adjust path
#include "nodes/ShowdownNode.h"      // Added
#include "Card.h"                // Added for Card::StringToInt
#include <fstream>
#include <vector>
#include <memory>
#include <stdexcept>
#include <json.hpp>
#include <cstdio> // For std::remove


// Use namespaces
using namespace poker_solver::core;
using namespace poker_solver::config;
using namespace poker_solver::nodes;
using namespace poker_solver::tree;

// Test fixture for GameTree tests using dynamic building
class GameTreeBuildTest : public ::testing::Test {
 protected:
  Deck deck_; 

  StreetSetting simple_setting_{{50.0}, {100.0}, {}, true};
  GameTreeBuildingSettings build_settings_{
      simple_setting_, simple_setting_, simple_setting_,
      simple_setting_, simple_setting_, simple_setting_
  };

  // Define initial board cards for the Flop scenario
  std::vector<int> initial_flop_board_cards_;

  std::unique_ptr<Rule> rule_;
  std::unique_ptr<GameTree> game_tree_;

  void SetUp() override {
      try {
        // Initialize initial_flop_board_cards_
        initial_flop_board_cards_ = {
            Card::StringToInt("Ac").value(),
            Card::StringToInt("Kd").value(),
            Card::StringToInt("5h").value()
        };

        rule_ = std::make_unique<Rule>(
            deck_,
            5.0, 5.0,
            GameRound::kFlop,
            initial_flop_board_cards_, // <<< PASS THE INITIAL BOARD CARDS HERE
            3,
            0.5, 1.0,
            100.0,
            build_settings_,
            0.98
        );

        game_tree_ = std::make_unique<GameTree>(*rule_);

      } catch (const std::exception& e) {
          FAIL() << "Setup failed: " << e.what();
      }
  }
};

// Test the dynamic building constructor and root node properties
TEST_F(GameTreeBuildTest, DynamicBuildRoot) {
    ASSERT_NE(game_tree_, nullptr);
    auto root = game_tree_->GetRoot();
    ASSERT_NE(root, nullptr);

    EXPECT_EQ(root->GetNodeType(), GameTreeNodeType::kAction);
    EXPECT_EQ(root->GetRound(), GameRound::kFlop);
    EXPECT_DOUBLE_EQ(root->GetPot(), 10.0); 

    auto action_node = std::dynamic_pointer_cast<ActionNode>(root);
    ASSERT_NE(action_node, nullptr);
    EXPECT_EQ(action_node->GetPlayerIndex(), 1); 
}

// Test the first level of actions from the root (Check/Bet)
TEST_F(GameTreeBuildTest, DynamicBuildFlopLevel1) {
    ASSERT_NE(game_tree_, nullptr);
    auto root = game_tree_->GetRoot();
    ASSERT_NE(root, nullptr);
    auto action_node = std::dynamic_pointer_cast<ActionNode>(root);
    ASSERT_NE(action_node, nullptr);

    const auto& actions = action_node->GetActions();
    const auto& children = action_node->GetChildren();

    bool found_check = false;
    bool found_bet_5 = false;
    bool found_all_in = false; 

    ASSERT_GE(actions.size(), 2u); // Use unsigned literal for size comparison

    for (const auto& action : actions) {
        if (action.GetAction() == PokerAction::kCheck) {
            found_check = true;
        } else if (action.GetAction() == PokerAction::kBet) {
            if (std::abs(action.GetAmount() - 5.0) < 1e-9) { // Pot = 10, 50% bet = 5
                found_bet_5 = true;
            }
            double stack = rule_->GetInitialEffectiveStack();
            double commit = rule_->GetInitialCommitment(action_node->GetPlayerIndex());
             // Make sure commit is not greater than stack to avoid negative remaining_stack
            double remaining_stack = std::max(0.0, stack - commit);
            if (build_settings_.flop_oop_setting.allow_all_in &&
                std::abs(action.GetAmount() - remaining_stack) < 1e-9) {
                found_all_in = true;
            }
        }
    }

    EXPECT_TRUE(found_check) << "Check action not found";
    EXPECT_TRUE(found_bet_5) << "Bet 5 action not found";
    if (build_settings_.flop_oop_setting.allow_all_in && (rule_->GetInitialEffectiveStack() - rule_->GetInitialCommitment(1) > 5.0 + 1e-9) ) { // Only expect all-in if it's distinct from 50% bet
       EXPECT_TRUE(found_all_in) << "All-in action not found (or stack too small/indistinguishable from other bets)";
    }


    for(size_t i=0; i < actions.size(); ++i) {
        if(actions[i].GetAction() == PokerAction::kCheck) {
            ASSERT_LT(i, children.size());
            ASSERT_NE(children[i], nullptr);
            EXPECT_EQ(children[i]->GetNodeType(), GameTreeNodeType::kAction);
            auto child_action_node = std::dynamic_pointer_cast<ActionNode>(children[i]);
            ASSERT_NE(child_action_node, nullptr);
            EXPECT_EQ(child_action_node->GetPlayerIndex(), 0); 
            EXPECT_DOUBLE_EQ(child_action_node->GetPot(), 10.0); 
        } else if (actions[i].GetAction() == PokerAction::kBet && std::abs(actions[i].GetAmount() - 5.0) < 1e-9) {
             ASSERT_LT(i, children.size());
             ASSERT_NE(children[i], nullptr);
             EXPECT_EQ(children[i]->GetNodeType(), GameTreeNodeType::kAction);
             auto child_action_node = std::dynamic_pointer_cast<ActionNode>(children[i]);
             ASSERT_NE(child_action_node, nullptr);
             EXPECT_EQ(child_action_node->GetPlayerIndex(), 0); 
             EXPECT_DOUBLE_EQ(child_action_node->GetPot(), 15.0); 
        }
    }
}

// Test Metadata Calculation
TEST_F(GameTreeBuildTest, CalculateMetadata) {
     ASSERT_NE(game_tree_, nullptr);
     auto root = game_tree_->GetRoot();
     ASSERT_NE(root, nullptr);

     game_tree_->CalculateTreeMetadata();

     EXPECT_EQ(root->GetDepth(), 0);
     EXPECT_GT(root->GetSubtreeSize(), 1) << "Subtree size should be > 1 for non-trivial tree";

     auto action_node = std::dynamic_pointer_cast<ActionNode>(root);
     ASSERT_NE(action_node, nullptr);
     std::shared_ptr<GameTreeNode> child_after_check = nullptr;
     for(size_t i=0; i < action_node->GetActions().size(); ++i) {
         if(action_node->GetActions()[i].GetAction() == PokerAction::kCheck) {
             child_after_check = action_node->GetChildren()[i];
             break;
         }
     }
     ASSERT_NE(child_after_check, nullptr);
     EXPECT_EQ(child_after_check->GetDepth(), 1);
     EXPECT_GT(child_after_check->GetSubtreeSize(), 0); 
     EXPECT_LT(child_after_check->GetSubtreeSize(), root->GetSubtreeSize());
}

// Test Memory Estimation (Basic Check)
TEST_F(GameTreeBuildTest, EstimateMemory) {
    ASSERT_NE(game_tree_, nullptr);
    size_t p0_range = 100;
    size_t p1_range = 150;
    EXPECT_GT(game_tree_->EstimateTrainableMemory(p0_range, p1_range), 0);
}

// Test JSON Loading Constructor
TEST(GameTreeJsonTest, JsonLoadThrows) {
    Deck deck;
    std::string dummy_json_path = "dummy_tree_for_gametree_test.json"; // Unique name
    std::ofstream outfile(dummy_json_path); 
    outfile.close();
    EXPECT_THROW(GameTree tree(dummy_json_path, deck), std::runtime_error);
    std::remove(dummy_json_path.c_str()); 
}