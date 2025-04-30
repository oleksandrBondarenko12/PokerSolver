#include "gtest/gtest.h"
#include "nodes/ChanceNode.h"      // Adjust path
#include "nodes/ActionNode.h"      // Adjust path (for creating dummy child/parent)
#include "nodes/GameTreeNode.h"    // Adjust path
#include "nodes/GameActions.h"      // Include for ActionNode constructor/methods if needed
#include "ranges/PrivateCards.h"     // Include for ActionNode constructor/methods if needed
#include "Card.h"              // Include for creating dummy dealt_cards
#include <vector>
#include <memory>
#include <stdexcept> // Include for std::exception
#include <iostream>  // For Debugging
#include <optional>  // For Card::StringToInt
#include <sstream>   // For HandVectorToString helper


// Use namespaces
using namespace poker_solver::core;
using namespace poker_solver::nodes;

// Test fixture for ChanceNode tests
class ChanceNodeTest : public ::testing::Test {
 protected:
  // Sample data
  GameRound round_ = GameRound::kFlop;
  double pot_ = 20.0;
  bool is_donk_ = true;

  // Dummy parent and child (ActionNode is a suitable concrete type)
  // Use make_shared for enable_shared_from_this
  std::shared_ptr<ActionNode> parent_node_;
  std::shared_ptr<ActionNode> child_node_;

  // ChanceNode instance
  std::shared_ptr<ChanceNode> chance_node_;

   void SetUp() override {
       try {
           // Create parent and child first
           parent_node_ = std::make_shared<ActionNode>(
               0, GameRound::kFlop, 10.0, std::weak_ptr<GameTreeNode>(), 1);
           child_node_ = std::make_shared<ActionNode>(
               1, GameRound::kTurn, 20.0, std::weak_ptr<GameTreeNode>(), 1);

           // Create ChanceNode in setup, initially potentially without child or dealt cards
           // Pass an empty dealt_cards vector for setup
           chance_node_ = std::make_shared<ChanceNode>(
               round_, pot_,
               std::weak_ptr<GameTreeNode>(parent_node_), // Create weak_ptr from shared_ptr
               std::vector<Card>{}, // Empty dealt cards for setup
               nullptr, // Start with null child
               is_donk_
           );

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
};

// Test constructor and basic getters
TEST_F(ChanceNodeTest, ConstructorAndGetters) {
  ASSERT_NE(chance_node_, nullptr);
  EXPECT_EQ(chance_node_->GetRound(), round_);
  EXPECT_EQ(chance_node_->GetPot(), pot_);
  EXPECT_EQ(chance_node_->GetNodeType(), GameTreeNodeType::kChance);
  EXPECT_TRUE(chance_node_->GetChild() == nullptr); // Child is initially null
  EXPECT_TRUE(chance_node_->GetDealtCards().empty()); // Dealt cards initially empty
  EXPECT_EQ(chance_node_->IsDonkOpportunity(), is_donk_);

  // Check parent was set correctly
  std::shared_ptr<GameTreeNode> parent_shared = chance_node_->GetParent(); // Returns shared_ptr
  ASSERT_NE(parent_shared, nullptr);
  EXPECT_EQ(parent_shared, parent_node_);
}

// Test SetChild method
TEST_F(ChanceNodeTest, SetChild) {
  ASSERT_NE(chance_node_, nullptr);
  ASSERT_NE(child_node_, nullptr);

  // Set the child
  chance_node_->SetChild(child_node_);

  // Verify GetChild returns the correct node
  EXPECT_EQ(chance_node_->GetChild(), child_node_);

  // Verify the child's parent pointer was set correctly
  std::shared_ptr<GameTreeNode> childs_parent_shared = child_node_->GetParent();
  ASSERT_NE(childs_parent_shared, nullptr);
  EXPECT_EQ(childs_parent_shared, chance_node_);

   // Test setting a null child throws
   EXPECT_THROW(chance_node_->SetChild(nullptr), std::invalid_argument);
}

// Test constructor and setting child separately
TEST(ChanceNodeConstructorTest, ConstructThenSetChild) { // Renamed Test
    GameRound round = GameRound::kTurn;
    double pot = 100.0;
    bool is_donk = false;
    auto parent = std::make_shared<ActionNode>(0, GameRound::kFlop, pot, std::weak_ptr<GameTreeNode>(), 1);
    auto child = std::make_shared<ActionNode>(1, GameRound::kTurn, pot, std::weak_ptr<GameTreeNode>(), 1);

    // Create dummy dealt cards
    std::vector<Card> dealt_cards;
    try {
        dealt_cards = { Card(Card::StringToInt("Ad").value()) };
    } catch (const std::exception& e) {
        FAIL() << "Failed to create dealt card Ad: " << e.what();
    }

    // *** Create ChanceNode with nullptr child first ***
    std::shared_ptr<ChanceNode> chance_node = nullptr;
    try {
        chance_node = std::make_shared<ChanceNode>(
            round,          // Argument 1: Round (Turn)
            pot,            // Argument 2: Pot
            std::weak_ptr<GameTreeNode>(parent), // Argument 3: Parent weak_ptr
            dealt_cards,    // Argument 4: Dealt cards vector
            nullptr,        // Argument 5: Child is initially null
            is_donk         // Argument 6: Donk flag
        );
    } catch (const std::exception& e) {
         FAIL() << "ChanceNode constructor threw exception: " << e.what();
    }
    ASSERT_NE(chance_node, nullptr); // Ensure construction succeeded

    // *** Now set the child explicitly ***
    ASSERT_NO_THROW(chance_node->SetChild(child));

    // Assertions
    EXPECT_EQ(chance_node->GetChild(), child); // Check child was set
    ASSERT_EQ(chance_node->GetDealtCards().size(), 1); // Check dealt cards
    if (!chance_node->GetDealtCards().empty() && !dealt_cards.empty()) {
        EXPECT_EQ(chance_node->GetDealtCards()[0], dealt_cards[0]);
    } else {
        FAIL() << "Dealt cards vector mismatch.";
    }
    // Check child's parent was set correctly by SetChild
    std::shared_ptr<GameTreeNode> childs_parent_shared = child->GetParent();
    ASSERT_NE(childs_parent_shared, nullptr);
    EXPECT_EQ(childs_parent_shared, chance_node);
    EXPECT_EQ(chance_node->IsDonkOpportunity(), is_donk);
}