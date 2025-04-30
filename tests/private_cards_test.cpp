#include "gtest/gtest.h"
#include "ranges/PrivateCards.h" // Adjust path if needed
#include "Card.h"          // For Card validation/conversion

#include <unordered_set>
#include <vector>
#include <stdexcept>

// Use the poker_solver::core namespace
using namespace poker_solver::core;

// Test fixture for PrivateCards tests
class PrivateCardsTest : public ::testing::Test {};

// Test constructor with valid inputs
TEST_F(PrivateCardsTest, ConstructorValid) {
  // Test canonical ordering
  PrivateCards pc1(0, 51, 1.0); // 2c, As
  EXPECT_EQ(pc1.Card1Int(), 0);
  EXPECT_EQ(pc1.Card2Int(), 51);
  EXPECT_DOUBLE_EQ(pc1.Weight(), 1.0);

  PrivateCards pc2(51, 0, 0.5); // As, 2c (should be ordered)
  EXPECT_EQ(pc2.Card1Int(), 0);
  EXPECT_EQ(pc2.Card2Int(), 51);
  EXPECT_DOUBLE_EQ(pc2.Weight(), 0.5);

  // Test default weight
  PrivateCards pc3(10, 20); // 4s, 7c
  EXPECT_EQ(pc3.Card1Int(), 10);
  EXPECT_EQ(pc3.Card2Int(), 20);
  EXPECT_DOUBLE_EQ(pc3.Weight(), 1.0);
}

// Test constructor with invalid card integers
TEST_F(PrivateCardsTest, ConstructorInvalidInt) {
  EXPECT_THROW(PrivateCards pc(-1, 51), std::out_of_range);
  EXPECT_THROW(PrivateCards pc(0, 52), std::out_of_range);
  EXPECT_THROW(PrivateCards pc(-1, 52), std::out_of_range);
}

// Test constructor with identical card integers
TEST_F(PrivateCardsTest, ConstructorIdenticalInts) {
  EXPECT_THROW(PrivateCards pc(10, 10), std::invalid_argument);
}

// Test default constructor state
TEST_F(PrivateCardsTest, DefaultConstructor) {
  PrivateCards pc;
  EXPECT_EQ(pc.Card1Int(), -1); // Indicates invalid state
  EXPECT_EQ(pc.Card2Int(), -1);
  EXPECT_DOUBLE_EQ(pc.Weight(), 0.0);
  EXPECT_EQ(pc.GetBoardMask(), 0ULL);
  EXPECT_EQ(pc.ToString(), "InvalidPrivateCards");
}

// Test GetBoardMask
TEST_F(PrivateCardsTest, GetBoardMask) {
  PrivateCards pc1(0, 51); // 2c, As
  uint64_t expected_mask1 = (1ULL << 0) | (1ULL << 51);
  EXPECT_EQ(pc1.GetBoardMask(), expected_mask1);

  PrivateCards pc2(9, 47); // 4d, Ks
  uint64_t expected_mask2 = (1ULL << 9) | (1ULL << 47);
  EXPECT_EQ(pc2.GetBoardMask(), expected_mask2);
}

// Test ToString
TEST_F(PrivateCardsTest, ToString) {
  PrivateCards pc1(0, 51); // 2c, As -> "2cAs"
  EXPECT_EQ(pc1.ToString(), "2cAs");

  PrivateCards pc2(51, 0); // As, 2c -> "2cAs" (canonical)
  EXPECT_EQ(pc2.ToString(), "2cAs");

  PrivateCards pc3(9, 47); // 4d, Ks -> "4dKs"
  EXPECT_EQ(pc3.ToString(), "4dKs");
}

// Test comparison operators
TEST_F(PrivateCardsTest, EqualityOperators) {
  PrivateCards pc1(0, 51, 1.0); // 2c, As
  PrivateCards pc2(51, 0, 0.5); // As, 2c (same cards, different weight)
  PrivateCards pc3(1, 51, 1.0); // 2d, As
  PrivateCards pc4(0, 50, 1.0); // 2c, Ah

  EXPECT_TRUE(pc1 == pc2); // Equality ignores weight
  EXPECT_FALSE(pc1 == pc3);
  EXPECT_FALSE(pc1 == pc4);

  EXPECT_FALSE(pc1 != pc2);
  EXPECT_TRUE(pc1 != pc3);
  EXPECT_TRUE(pc1 != pc4);
}

TEST_F(PrivateCardsTest, LessThanOperator) {
  PrivateCards pc1(0, 51); // 2c, As
  PrivateCards pc2(1, 10); // 2d, 4s
  PrivateCards pc3(0, 50); // 2c, Ah

  EXPECT_TRUE(pc1 < pc2); // 0 < 1
  EXPECT_TRUE(pc3 < pc1); // 0 == 0, but 50 < 51
  EXPECT_FALSE(pc2 < pc1);
  EXPECT_FALSE(pc1 < pc3);
  EXPECT_FALSE(pc1 < pc1); // Not less than self
}

// Test hashing
TEST_F(PrivateCardsTest, Hashing) {
  PrivateCards pc1(0, 51, 1.0); // 2c, As
  PrivateCards pc2(51, 0, 0.5); // As, 2c (same cards)
  PrivateCards pc3(1, 51, 1.0); // 2d, As

  std::hash<PrivateCards> hasher;

  EXPECT_EQ(hasher(pc1), hasher(pc2)); // Equal objects must have equal hashes
  EXPECT_NE(hasher(pc1), hasher(pc3)); // Unequal objects ideally have different hashes

  // Test usability in unordered_set
  std::unordered_set<PrivateCards> card_set;
  EXPECT_TRUE(card_set.insert(pc1).second); // Insert should succeed
  EXPECT_FALSE(card_set.insert(pc2).second); // Insert should fail (already present)
  EXPECT_TRUE(card_set.insert(pc3).second); // Insert should succeed
  EXPECT_EQ(card_set.size(), 2);
}
