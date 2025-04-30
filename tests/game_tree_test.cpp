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
#include "nodes/ShowdownNode.h"  
#include <fstream>   // <<< ADD THIS INCLUDE
#include <vector>
#include <memory>
#include <stdexcept>
#include <json.hpp>
#include <cstdio>


// Use namespaces
using namespace poker_solver::core;
using namespace poker_solver::config;
using namespace poker_solver::nodes;
using namespace poker_solver::tree;

// Test fixture for GameTree tests using dynamic building
class GameTreeBuildTest : public ::testing::Test {
 protected:
  // Setup common objects for dynamic building
  Deck deck_; // Standard 52-card deck

  // Simple betting structure: Bet 50% pot, Raise 100% pot, All-in allowed
  StreetSetting simple_setting_{{50.0}, {100.0}, {}, true};
  GameTreeBuildingSettings build_settings_{
      simple_setting_, simple_setting_, simple_setting_, // IP settings
      simple_setting_, simple_setting_, simple_setting_  // OOP settings
  };

  // Rule for a simple Flop scenario: 100bb stacks, 10bb pot preflop
  std::unique_ptr<Rule> rule_;
  std::unique_ptr<GameTree> game_tree_;

  void SetUp() override {
      try {
        rule_ = std::make_unique<Rule>(
            deck_,
            5.0, 5.0,            // Initial commitments (e.g., blinds posted = 10 total)
            GameRound::kFlop,    // Start on the Flop
            3,                   // Raise limit per street
            0.5, 1.0,            // Blinds (not directly used postflop, but part of Rule)
            100.0,               // Initial effective stack
            build_settings_,
            0.98                 // All-in threshold ratio
        );

        // Build the tree
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

    // Expecting Flop start, OOP (Player 1) to act first
    EXPECT_EQ(root->GetNodeType(), GameTreeNodeType::kAction);
    EXPECT_EQ(root->GetRound(), GameRound::kFlop);
    EXPECT_DOUBLE_EQ(root->GetPot(), 10.0); // 5+5 initial commit

    auto action_node = std::dynamic_pointer_cast<ActionNode>(root);
    ASSERT_NE(action_node, nullptr);
    EXPECT_EQ(action_node->GetPlayerIndex(), 1); // OOP acts first postflop
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

    // Expect Check and Bet actions (OOP first action)
    // Bet amount = 50% of 10 pot = 5
    bool found_check = false;
    bool found_bet_5 = false;
    bool found_all_in = false; // All-in might also be an option if allowed

    ASSERT_GE(actions.size(), 2); // Should have at least check and one bet size

    for (const auto& action : actions) {
        if (action.GetAction() == PokerAction::kCheck) {
            found_check = true;
        } else if (action.GetAction() == PokerAction::kBet) {
            if (std::abs(action.GetAmount() - 5.0) < 1e-9) {
                found_bet_5 = true;
            }
            // Check if stack size allows all-in and if it's present
            double stack = rule_->GetInitialEffectiveStack();
            double commit = rule_->GetInitialCommitment(action_node->GetPlayerIndex());
            if (build_settings_.flop_oop_setting.allow_all_in &&
                std::abs(action.GetAmount() - (stack - commit)) < 1e-9) {
                found_all_in = true;
            }
        }
    }

    EXPECT_TRUE(found_check) << "Check action not found";
    EXPECT_TRUE(found_bet_5) << "Bet 5 action not found";
    // Check for all-in only if it was explicitly allowed by the settings
    if (build_settings_.flop_oop_setting.allow_all_in) {
       EXPECT_TRUE(found_all_in) << "All-in action not found (or stack too small)";
    }


    // Check child node types after Check and Bet 5
    for(size_t i=0; i < actions.size(); ++i) {
        if(actions[i].GetAction() == PokerAction::kCheck) {
            ASSERT_LT(i, children.size());
            ASSERT_NE(children[i], nullptr);
            // After OOP checks, IP (P0) acts
            EXPECT_EQ(children[i]->GetNodeType(), GameTreeNodeType::kAction);
            auto child_action_node = std::dynamic_pointer_cast<ActionNode>(children[i]);
            ASSERT_NE(child_action_node, nullptr);
            EXPECT_EQ(child_action_node->GetPlayerIndex(), 0); // IP's turn
            EXPECT_DOUBLE_EQ(child_action_node->GetPot(), 10.0); // Pot unchanged
        } else if (actions[i].GetAction() == PokerAction::kBet && std::abs(actions[i].GetAmount() - 5.0) < 1e-9) {
             ASSERT_LT(i, children.size());
             ASSERT_NE(children[i], nullptr);
             // After OOP bets, IP (P0) acts
             EXPECT_EQ(children[i]->GetNodeType(), GameTreeNodeType::kAction);
             auto child_action_node = std::dynamic_pointer_cast<ActionNode>(children[i]);
             ASSERT_NE(child_action_node, nullptr);
             EXPECT_EQ(child_action_node->GetPlayerIndex(), 0); // IP's turn
             EXPECT_DOUBLE_EQ(child_action_node->GetPot(), 15.0); // Pot increased by bet
        }
    }
}

// Test Metadata Calculation
TEST_F(GameTreeBuildTest, CalculateMetadata) {
     ASSERT_NE(game_tree_, nullptr);
     auto root = game_tree_->GetRoot();
     ASSERT_NE(root, nullptr);

     // Calculate metadata
     game_tree_->CalculateTreeMetadata();

     // Check root metadata
     EXPECT_EQ(root->GetDepth(), 0);
     EXPECT_GT(root->GetSubtreeSize(), 1) << "Subtree size should be > 1 for non-trivial tree";

     // Check metadata of a child node (e.g., after OOP checks)
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
     EXPECT_GT(child_after_check->GetSubtreeSize(), 0); // Should have at least itself
     EXPECT_LT(child_after_check->GetSubtreeSize(), root->GetSubtreeSize());
}

// Test Memory Estimation (Basic Check)
TEST_F(GameTreeBuildTest, EstimateMemory) {
    ASSERT_NE(game_tree_, nullptr);
    size_t p0_range = 100;
    size_t p1_range = 150;
    // Expect a non-zero estimate for a built tree
    EXPECT_GT(game_tree_->EstimateTrainableMemory(p0_range, p1_range), 0);
}

// Test JSON Loading Constructor
TEST(GameTreeJsonTest, JsonLoadThrows) {
    Deck deck;
    // Create a dummy empty JSON file or use a non-existent one
    std::string dummy_json_path = "dummy_tree.json";
    // Ensure file doesn't exist or is empty/invalid
    std::ofstream outfile(dummy_json_path); // Create empty file
    outfile.close();
    // Expect std::runtime_error because parsing empty file fails first
    EXPECT_THROW(GameTree tree(dummy_json_path, deck), std::runtime_error);
    std::remove(dummy_json_path.c_str()); // Clean up dummy file
}
