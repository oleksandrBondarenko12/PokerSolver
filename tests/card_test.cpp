#include "gtest/gtest.h" // Google Test header
#include "Card.h"   // Header for the Card class (adjust path if needed)

#include <vector>
#include <cstdint>
#include <optional>
#include <stdexcept> // For exception testing

// Use the poker_solver::core namespace
using namespace poker_solver::core;

// Test fixture for Card tests (optional, but good practice)
class CardTest : public ::testing::Test {
 protected:
  // You can add setup/teardown logic here if needed
};

// Test Card constructors
TEST_F(CardTest, ConstructorValidInt) {
  EXPECT_NO_THROW(Card c(0));   // 2c
  EXPECT_NO_THROW(Card c(51));  // As
  Card c0(0);
  Card c51(51);
  ASSERT_TRUE(c0.card_int().has_value());
  EXPECT_EQ(c0.card_int().value(), 0);
  ASSERT_TRUE(c51.card_int().has_value());
  EXPECT_EQ(c51.card_int().value(), 51);
}

TEST_F(CardTest, ConstructorInvalidInt) {
  EXPECT_THROW(Card c(-1), std::out_of_range);
  EXPECT_THROW(Card c(52), std::out_of_range);
}

TEST_F(CardTest, ConstructorValidString) {
  EXPECT_NO_THROW(Card c("As"));
  EXPECT_NO_THROW(Card c("2c"));
  EXPECT_NO_THROW(Card c("Td"));
  Card as("As");
  Card tc("Tc");
  ASSERT_TRUE(as.card_int().has_value());
  EXPECT_EQ(as.card_int().value(), 51); // A=12, s=3 -> 12*4+3 = 51
  ASSERT_TRUE(tc.card_int().has_value());
  EXPECT_EQ(tc.card_int().value(), 32); // T=8, c=0 -> 8*4+0 = 32
}

TEST_F(CardTest, ConstructorInvalidString) {
  EXPECT_THROW(Card c(""), std::invalid_argument);
  EXPECT_THROW(Card c("A"), std::invalid_argument);
  EXPECT_THROW(Card c("Xs"), std::invalid_argument);
  EXPECT_THROW(Card c("Ax"), std::invalid_argument);
  EXPECT_THROW(Card c("A s"), std::invalid_argument);
  EXPECT_THROW(Card c("AceSpades"), std::invalid_argument);
}

TEST_F(CardTest, DefaultConstructorIsEmpty) {
  Card c;
  EXPECT_TRUE(c.IsEmpty());
  EXPECT_FALSE(c.card_int().has_value());
  EXPECT_EQ(c.ToString(), "Empty");
}

// Test static conversions
TEST_F(CardTest, StaticStringToInt) {
  EXPECT_EQ(Card::StringToInt("2c").value_or(-1), 0);
  EXPECT_EQ(Card::StringToInt("As").value_or(-1), 51);
  EXPECT_EQ(Card::StringToInt("Kd").value_or(-1), 45); // K=11, d=1 -> 11*4+1=45
  EXPECT_EQ(Card::StringToInt("Th").value_or(-1), 34); // T=8, h=2 -> 8*4+2=34
  EXPECT_FALSE(Card::StringToInt("").has_value());
  EXPECT_FALSE(Card::StringToInt("Xy").has_value());
  EXPECT_FALSE(Card::StringToInt("2x").has_value());
  EXPECT_FALSE(Card::StringToInt("Yc").has_value());
}

TEST_F(CardTest, StaticIntToString) {
  EXPECT_EQ(Card::IntToString(0), "2c");
  EXPECT_EQ(Card::IntToString(51), "As");
  EXPECT_EQ(Card::IntToString(45), "Kd");
  EXPECT_EQ(Card::IntToString(34), "Th");
  EXPECT_EQ(Card::IntToString(-1), "Invalid");
  EXPECT_EQ(Card::IntToString(52), "Invalid");
}

// Test bitmask utilities
TEST_F(CardTest, StaticCardIntToUint64) {
  EXPECT_EQ(Card::CardIntToUint64(0), 1ULL << 0);
  EXPECT_EQ(Card::CardIntToUint64(51), 1ULL << 51);
  EXPECT_EQ(Card::CardIntToUint64(30), 1ULL << 30);
  EXPECT_THROW(Card::CardIntToUint64(-1), std::out_of_range);
  EXPECT_THROW(Card::CardIntToUint64(52), std::out_of_range);
}

TEST_F(CardTest, StaticCardToUint64) {
  Card c0(0);
  Card c51(51);
  Card empty;
  EXPECT_EQ(Card::CardToUint64(c0), 1ULL << 0);
  EXPECT_EQ(Card::CardToUint64(c51), 1ULL << 51);
  EXPECT_EQ(Card::CardToUint64(empty), 0ULL);
}

TEST_F(CardTest, StaticCardIntsToUint64) {
  std::vector<int> ints = {0, 51, 10}; // 2c, As, 4d (4d = 2*4+1 = 9)
  uint64_t expected_mask = (1ULL << 0) | (1ULL << 51) | (1ULL << 9);
  EXPECT_EQ(Card::CardIntsToUint64({0, 51, 9}), expected_mask);
  EXPECT_EQ(Card::CardIntsToUint64({}), 0ULL);
  EXPECT_THROW(Card::CardIntsToUint64({0, 52}), std::out_of_range);
}

TEST_F(CardTest, StaticCardsToUint64) {
  std::vector<Card> cards = {Card("2c"), Card("As"), Card("4d"), Card()};
  uint64_t expected_mask = (1ULL << 0) | (1ULL << 51) | (1ULL << 9);
  EXPECT_EQ(Card::CardsToUint64(cards), expected_mask);
  EXPECT_EQ(Card::CardsToUint64({}), 0ULL);
  EXPECT_EQ(Card::CardsToUint64({Card(), Card()}), 0ULL);
}

TEST_F(CardTest, StaticUint64ToCardInts) {
  uint64_t mask = (1ULL << 0) | (1ULL << 51) | (1ULL << 9);
  std::vector<int> expected_ints = {0, 9, 51}; // Order depends on bit position
  std::vector<int> actual_ints = Card::Uint64ToCardInts(mask);
  // Sort for comparison as order isn't guaranteed by the function itself
  std::sort(actual_ints.begin(), actual_ints.end());
  EXPECT_EQ(actual_ints, expected_ints);
  EXPECT_EQ(Card::Uint64ToCardInts(0ULL), std::vector<int>{});
}

TEST_F(CardTest, StaticUint64ToCards) {
  uint64_t mask = (1ULL << 0) | (1ULL << 51) | (1ULL << 9);
  std::vector<Card> actual_cards = Card::Uint64ToCards(mask);
  ASSERT_EQ(actual_cards.size(), 3);
  // Check if the correct cards are present (order might vary)
  bool found0 = false, found9 = false, found51 = false;
  for(const auto& card : actual_cards) {
      if(card.card_int() == 0) found0 = true;
      if(card.card_int() == 9) found9 = true;
      if(card.card_int() == 51) found51 = true;
  }
  EXPECT_TRUE(found0);
  EXPECT_TRUE(found9);
  EXPECT_TRUE(found51);

  EXPECT_EQ(Card::Uint64ToCards(0ULL).size(), 0);
}

TEST_F(CardTest, StaticDoBoardsOverlap) {
  uint64_t board1 = Card::CardIntToUint64(0) | Card::CardIntToUint64(10); // 2c, 4s
  uint64_t board2 = Card::CardIntToUint64(1) | Card::CardIntToUint64(11); // 2d, 4h
  uint64_t board3 = Card::CardIntToUint64(0) | Card::CardIntToUint64(20); // 2c, 7c
  EXPECT_FALSE(Card::DoBoardsOverlap(board1, board2));
  EXPECT_TRUE(Card::DoBoardsOverlap(board1, board3)); // Overlap on 2c (bit 0)
  EXPECT_FALSE(Card::DoBoardsOverlap(board1, 0ULL));
  EXPECT_FALSE(Card::DoBoardsOverlap(0ULL, board2));
  EXPECT_FALSE(Card::DoBoardsOverlap(0ULL, 0ULL));
}

// Test Rank/Suit helpers
TEST_F(CardTest, StaticRankSuitHelpers) {
    EXPECT_EQ(Card::RankCharToIndex('A'), 12);
    EXPECT_EQ(Card::RankCharToIndex('2'), 0);
    EXPECT_EQ(Card::RankCharToIndex('T'), 8);
    EXPECT_EQ(Card::RankCharToIndex('X'), -1);

    EXPECT_EQ(Card::SuitCharToIndex('s'), 3);
    EXPECT_EQ(Card::SuitCharToIndex('c'), 0);
    EXPECT_EQ(Card::SuitCharToIndex('x'), -1);

    EXPECT_EQ(Card::RankIndexToChar(12), 'A');
    EXPECT_EQ(Card::RankIndexToChar(0), '2');
    EXPECT_EQ(Card::RankIndexToChar(8), 'T');
    EXPECT_EQ(Card::RankIndexToChar(13), '?');

    EXPECT_EQ(Card::SuitIndexToChar(3), 's');
    EXPECT_EQ(Card::SuitIndexToChar(0), 'c');
    EXPECT_EQ(Card::SuitIndexToChar(4), '?');
}

