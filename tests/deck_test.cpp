#include "gtest/gtest.h"
#include "Deck.h" // Adjust path if needed
#include "Card.h" // Include Card for constants/validation

// Use the poker_solver::core namespace
using namespace poker_solver::core;

// Test fixture for Deck tests
class DeckTest : public ::testing::Test {};

// Test the default constructor (standard 52-card deck)
TEST_F(DeckTest, DefaultConstructorCreatesStandardDeck) {
  Deck d;
  const auto& cards = d.GetCards();

  // Check size
  ASSERT_EQ(cards.size(), kNumCardsInDeck);

  // Check if cards are present and in order (0 to 51)
  for (int i = 0; i < kNumCardsInDeck; ++i) {
    ASSERT_FALSE(cards[i].IsEmpty());
    ASSERT_TRUE(cards[i].card_int().has_value());
    EXPECT_EQ(cards[i].card_int().value(), i) << "Card at index " << i << " is wrong.";
  }
}

// Test the custom constructor
TEST_F(DeckTest, CustomConstructor) {
  std::vector<std::string_view> ranks = {"A", "K"};
  std::vector<std::string_view> suits = {"s", "h"};
  Deck d(ranks, suits);
  const auto& cards = d.GetCards();

  ASSERT_EQ(cards.size(), 4); // A, K x s, h = 4 cards

  // Check specific cards (order depends on loop in constructor)
  // As = 51, Ah = 50, Ks = 47, Kh = 46
  bool found_as = false, found_ah = false, found_ks = false, found_kh = false;
  for (const auto& card : cards) {
      ASSERT_FALSE(card.IsEmpty());
      int val = card.card_int().value();
      if (val == 51) found_as = true;
      else if (val == 50) found_ah = true;
      else if (val == 47) found_ks = true;
      else if (val == 46) found_kh = true;
  }
  EXPECT_TRUE(found_as);
  EXPECT_TRUE(found_ah);
  EXPECT_TRUE(found_ks);
  EXPECT_TRUE(found_kh);
}

TEST_F(DeckTest, CustomConstructorInvalidCard) {
  std::vector<std::string_view> ranks = {"A", "X"}; // Invalid rank 'X'
  std::vector<std::string_view> suits = {"s", "h"};
  // Expect runtime_error because Card constructor will throw invalid_argument
  EXPECT_THROW(Deck d(ranks, suits), std::runtime_error);
}

// Test FindCard methods
TEST_F(DeckTest, FindCardByInt) {
  Deck d; // Standard deck
  Card c0 = d.FindCard(0);   // Should be 2c
  Card c51 = d.FindCard(51); // Should be As
  Card c_invalid = d.FindCard(52);
  Card c_neg = d.FindCard(-1);

  ASSERT_FALSE(c0.IsEmpty());
  EXPECT_EQ(c0.card_int().value(), 0);
  EXPECT_EQ(c0.ToString(), "2c");

  ASSERT_FALSE(c51.IsEmpty());
  EXPECT_EQ(c51.card_int().value(), 51);
  EXPECT_EQ(c51.ToString(), "As");

  EXPECT_TRUE(c_invalid.IsEmpty());
  EXPECT_TRUE(c_neg.IsEmpty());
}

TEST_F(DeckTest, FindCardByString) {
  Deck d; // Standard deck
  Card c_as = d.FindCard("As");
  Card c_2c = d.FindCard("2c");
  Card c_td = d.FindCard("Td");
  Card c_invalid_str = d.FindCard("Xy");
  Card c_empty_str = d.FindCard("");

  ASSERT_FALSE(c_as.IsEmpty());
  EXPECT_EQ(c_as.card_int().value(), 51);

  ASSERT_FALSE(c_2c.IsEmpty());
  EXPECT_EQ(c_2c.card_int().value(), 0);

  ASSERT_FALSE(c_td.IsEmpty());
  EXPECT_EQ(c_td.card_int().value(), 33); // T=8, d=1 -> 8*4+1 = 33

  EXPECT_TRUE(c_invalid_str.IsEmpty());
  EXPECT_TRUE(c_empty_str.IsEmpty());
}

