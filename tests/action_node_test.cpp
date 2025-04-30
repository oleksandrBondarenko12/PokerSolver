#include "gtest/gtest.h"
#include "nodes/ActionNode.h"      // Adjust path
#include "nodes/TerminalNode.h"    // Adjust path (for creating dummy children)
#include "nodes/GameTreeNode.h"    // Adjust path
#include "nodes/GameActions.h"      // Adjust path
#include "ranges/PrivateCards.h"     // Adjust path
#include "Card.h"              // Adjust path
#include "trainable/Trainable.h"       // Adjust path (for interface)
#include "trainable/DiscountedCfrTrainable.h" // Adjust path (for type checking)
#include <vector>
#include <memory>
#include <stdexcept>

// Use namespaces
using namespace poker_solver::core;
using namespace poker_solver::nodes;
using namespace poker_solver::solver; // For Trainable types

// Test fixture for ActionNode tests
class ActionNodeTest : public ::testing::Test {
 protected:
  // Sample data
  size_t player_index_ = 1; // OOP
  GameRound round_ = GameRound::kFlop;
  double pot_ = 50.0;
  size_t num_deals_ = 1; // Assume perfect recall for most tests

  // Sample range (needed for Trainable creation)
  std::vector<PrivateCards> player_range_;

  // Parent pointer (can be null for root)
  std::shared_ptr<GameTreeNode> parent_ = nullptr; // Or create a dummy node if needed

  // ActionNode instance
  std::shared_ptr<ActionNode> action_node_; // Use shared_ptr to test SetParent easily

  void SetUp() override {
      try {
        // Create a minimal range
        player_range_.emplace_back(Card::StringToInt("Ac").value(), Card::StringToInt("Kc").value());
        player_range_.emplace_back(Card::StringToInt("Ad").value(), Card::StringToInt("Kd").value());

        // Create the ActionNode using make_shared for shared_from_this
        action_node_ = std::make_shared<ActionNode>(
            player_index_, round_, pot_,
            std::weak_ptr<GameTreeNode>(), // Initially no parent
            num_deals_
        );
         action_node_->SetPlayerRange(&player_range_); // Set the range

      } catch (const std::exception& e) {
          FAIL() << "Setup failed: " << e.what();
      }
  }
};

// Test constructor and basic getters
TEST_F(ActionNodeTest, ConstructorAndGetters) {
  ASSERT_NE(action_node_, nullptr);
  EXPECT_EQ(action_node_->GetPlayerIndex(), player_index_);
  EXPECT_EQ(action_node_->GetRound(), round_);
  EXPECT_EQ(action_node_->GetPot(), pot_);
  EXPECT_EQ(action_node_->GetNodeType(), GameTreeNodeType::kAction);
  EXPECT_TRUE(action_node_->GetParent() == nullptr); // No parent set initially
  EXPECT_TRUE(action_node_->GetActions().empty());
  EXPECT_TRUE(action_node_->GetChildren().empty());
}

// Test adding children and actions
TEST_F(ActionNodeTest, AddChild) {
  ASSERT_NE(action_node_, nullptr);

  // Create dummy child nodes (TerminalNodes are simple)
  auto child1 = std::make_shared<TerminalNode>(std::vector<double>{10.0, -10.0}, round_, pot_, action_node_);
  auto child2 = std::make_shared<TerminalNode>(std::vector<double>{-5.0, 5.0}, round_, pot_ + 10.0, action_node_);

  // Create actions
  GameAction action1(PokerAction::kFold);
  GameAction action2(PokerAction::kBet, 10.0);

  // Add children
  action_node_->AddChild(action1, child1);
  action_node_->AddChild(action2, child2);

  // Verify actions
  const auto& actions = action_node_->GetActions();
  ASSERT_EQ(actions.size(), 2);
  EXPECT_EQ(actions[0].GetAction(), PokerAction::kFold);
  EXPECT_EQ(actions[1].GetAction(), PokerAction::kBet);
  EXPECT_DOUBLE_EQ(actions[1].GetAmount(), 10.0);

  // Verify children
  const auto& children = action_node_->GetChildren();
  ASSERT_EQ(children.size(), 2);
  EXPECT_EQ(children[0], child1);
  EXPECT_EQ(children[1], child2);

  // Verify parent pointers were set on children
  ASSERT_NE(children[0]->GetParent(), nullptr);
  EXPECT_EQ(children[0]->GetParent(), action_node_);
  ASSERT_NE(children[1]->GetParent(), nullptr);
  EXPECT_EQ(children[1]->GetParent(), action_node_);
}

// Test SetActionsAndChildren
TEST_F(ActionNodeTest, SetActionsAndChildren) {
   ASSERT_NE(action_node_, nullptr);

   auto child1 = std::make_shared<TerminalNode>(std::vector<double>{10.0, -10.0}, round_, pot_, action_node_);
   auto child2 = std::make_shared<TerminalNode>(std::vector<double>{-5.0, 5.0}, round_, pot_ + 10.0, action_node_);
   GameAction action1(PokerAction::kFold);
   GameAction action2(PokerAction::kBet, 10.0);

   std::vector<GameAction> actions_vec = {action1, action2};
   std::vector<std::shared_ptr<GameTreeNode>> children_vec = {child1, child2};

   action_node_->SetActionsAndChildren(actions_vec, children_vec);

   // Verify actions and children (similar checks as AddChild test)
   const auto& actions = action_node_->GetActions();
   ASSERT_EQ(actions.size(), 2);
   EXPECT_EQ(actions[0].GetAction(), PokerAction::kFold);
   EXPECT_EQ(actions[1].GetAction(), PokerAction::kBet);

   const auto& children = action_node_->GetChildren();
   ASSERT_EQ(children.size(), 2);
   EXPECT_EQ(children[0], child1);
   EXPECT_EQ(children[1], child2);
   EXPECT_EQ(children[0]->GetParent(), action_node_);
   EXPECT_EQ(children[1]->GetParent(), action_node_);

   // Test size mismatch throw
   actions_vec.pop_back(); // Make sizes different
   EXPECT_THROW(action_node_->SetActionsAndChildren(actions_vec, children_vec), std::invalid_argument);
}

// Test Trainable management
TEST_F(ActionNodeTest, TrainableManagement) {
    ASSERT_NE(action_node_, nullptr);
    size_t deal_index = 0;

    // 1. Test GetTrainableIfExists before creation
    EXPECT_EQ(action_node_->GetTrainableIfExists(deal_index), nullptr);

    // 2. Test GetTrainable lazy creation
    std::shared_ptr<Trainable> trainable1 = nullptr;
    EXPECT_NO_THROW(trainable1 = action_node_->GetTrainable(deal_index));
    ASSERT_NE(trainable1, nullptr);

    // 3. Check type (optional but good)
    auto concrete_trainable = std::dynamic_pointer_cast<DiscountedCfrTrainable>(trainable1);
    EXPECT_NE(concrete_trainable, nullptr) << "Trainable object has unexpected type.";

    // 4. Test GetTrainableIfExists after creation
    EXPECT_EQ(action_node_->GetTrainableIfExists(deal_index), trainable1);

    // 5. Test GetTrainable returns the same object
    std::shared_ptr<Trainable> trainable2 = action_node_->GetTrainable(deal_index);
    EXPECT_EQ(trainable1, trainable2) << "Second call to GetTrainable returned different object.";
    EXPECT_EQ(trainable1.get(), trainable2.get()) << "Pointer addresses differ."; // Stricter check

    // 6. Test invalid index throws
    EXPECT_THROW(action_node_->GetTrainable(num_deals_), std::out_of_range); // num_deals_ is the size, so index num_deals_ is out of bounds
    EXPECT_THROW(action_node_->GetTrainableIfExists(num_deals_), std::out_of_range);
}

// Test GetTrainable without setting range first
TEST(ActionNodeTrainableNoRange, GetTrainableThrows) {
     // Create node without setting range
     ActionNode node_no_range(0, GameRound::kTurn, 100.0, std::weak_ptr<GameTreeNode>(), 1);
     EXPECT_THROW(node_no_range.GetTrainable(0), std::runtime_error);
}

// Test constructor with multiple deals (imperfect recall)
TEST(ActionNodeMultiDeal, Constructor) {
    size_t num_deals = 5;
    ActionNode node_multi_deal(0, GameRound::kTurn, 100.0, std::weak_ptr<GameTreeNode>(), num_deals);
    // Check internal vector size (cannot access directly, test via GetTrainableIfExists bounds)
    EXPECT_THROW(node_multi_deal.GetTrainableIfExists(num_deals), std::out_of_range); // Index 5 is out of bounds
    EXPECT_NO_THROW(node_multi_deal.GetTrainableIfExists(num_deals - 1)); // Index 4 is in bounds
    EXPECT_EQ(node_multi_deal.GetTrainableIfExists(num_deals - 1), nullptr); // Should be null initially
}
