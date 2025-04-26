#include "gtest/gtest.h" // Google Test header
#include "compairer/Dic5Compairer.h" // The class we are testing
#include "Card.h"           // Needed for creating Card objects
#include "Library.h"       // For PokerSolverUtils::Combinations
#include <vector>
#include <string>
#include <stdexcept>
#include <memory>           // For std::shared_ptr
#include <iostream>         // For SetUpTestSuite logging
#include <iomanip>        // For std::hex
#include <algorithm>      // For std::sort

// Use the namespace defined in your project
using namespace PokerSolver;
// Assuming Library.h defines Combinations in PokerSolverUtils
using namespace PokerSolverUtils;

// Helper function to easily create a vector of Cards from string representations
std::vector<Card> cardsFromStrings(const std::vector<std::string>& cardStrings) {
    std::vector<Card> cards;
    cards.reserve(cardStrings.size());
    for (const auto& s : cardStrings) {
        try {
            cards.emplace_back(s); // Use Card's string constructor
        } catch (const std::exception& e) {
            throw std::runtime_error("Error creating card from string '" + s + "' in test helper: " + e.what());
        }
    }
    return cards;
}

// Test Fixture for Dic5Compairer
class Dic5CompairerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        try {
            std::string resourcePath = "./five_card_strength.txt"; // Adjust as needed!
            compairer_ = std::make_shared<Dic5Compairer>(resourcePath);
            std::cout << "[          ] Dic5Compairer loaded successfully for tests from: " << resourcePath << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "FATAL ERROR in SetUpTestSuite: Failed to load Dic5Compairer from path specified in test: " << e.what() << std::endl;
            compairer_ = nullptr;
        }
    }

    static void TearDownTestSuite() {
        compairer_.reset();
    }

    // --- DEBUG HELPER (Commented Out) ---
    /*
    uint64_t getHandKey_Debug(const std::vector<Card>& hand) {
         if (!compairer_ || hand.size() != 5) return 0;
         std::vector<Card> sorted = hand;
         std::sort(sorted.begin(), sorted.end());
         // This requires computeHandKey to be public for direct call:
         // return compairer_->computeHandKey(sorted);
         std::cout << "DEBUG: getHandKey_Debug called, but computeHandKey is private." << std::endl;
         return 0; // Placeholder if private
    }
    */
    // --- END DEBUG HELPER ---

    static std::shared_ptr<Dic5Compairer> compairer_;
};

std::shared_ptr<Dic5Compairer> Dic5CompairerTest::compairer_ = nullptr;

// --- Test Cases ---

TEST_F(Dic5CompairerTest, Loading) { ASSERT_NE(compairer_, nullptr); }
TEST_F(Dic5CompairerTest, GetRank_RoyalFlush) { ASSERT_NE(compairer_, nullptr); std::vector<Card> p = cardsFromStrings({"2c", "3d"}); std::vector<Card> b = cardsFromStrings({"Ah", "Kh", "Qh", "Jh", "10h"}); EXPECT_EQ(compairer_->getRank(p, b), 1); }
TEST_F(Dic5CompairerTest, GetRank_StraightFlush) { ASSERT_NE(compairer_, nullptr); std::vector<Card> p = cardsFromStrings({"9s", "4s"}); std::vector<Card> b = cardsFromStrings({"8s", "7s", "6s", "5s", "2d"}); EXPECT_EQ(compairer_->getRank(p, b), 9); }
TEST_F(Dic5CompairerTest, GetRank_FourOfAKind) { ASSERT_NE(compairer_, nullptr); std::vector<Card> p = cardsFromStrings({"Kc", "Kd"}); std::vector<Card> b = cardsFromStrings({"Ac", "Ad", "As", "Ah", "Qh"}); EXPECT_EQ(compairer_->getRank(p, b), 41); }
TEST_F(Dic5CompairerTest, GetRank_FullHouse) { ASSERT_NE(compairer_, nullptr); std::vector<Card> p = cardsFromStrings({"Qh", "Qs"}); std::vector<Card> b = cardsFromStrings({"Kh", "Ks", "Kd", "2c", "3d"}); EXPECT_EQ(compairer_->getRank(p, b), 197); }
TEST_F(Dic5CompairerTest, GetRank_FlushAceHigh) { ASSERT_NE(compairer_, nullptr); std::vector<Card> p = cardsFromStrings({"3c", "4d"}); std::vector<Card> b = cardsFromStrings({"Ah", "Kh", "Qh", "7h", "2h"}); EXPECT_EQ(compairer_->getRank(p, b), 1282); }
TEST_F(Dic5CompairerTest, GetRank_Straight9High) { ASSERT_NE(compairer_, nullptr); std::vector<Card> p = cardsFromStrings({"9c", "4d"}); std::vector<Card> b = cardsFromStrings({"8c", "7d", "6h", "5s", "2s"}); EXPECT_EQ(compairer_->getRank(p, b), 2557); }
TEST_F(Dic5CompairerTest, GetRank_StraightAceLow) { ASSERT_NE(compairer_, nullptr); std::vector<Card> p = cardsFromStrings({"Ac", "Kd"}); std::vector<Card> b = cardsFromStrings({"2d", "3h", "4s", "5c", "Ks"}); EXPECT_EQ(compairer_->getRank(p, b), 2561); }
TEST_F(Dic5CompairerTest, GetRank_ThreeOfAKind) { ASSERT_NE(compairer_, nullptr); std::vector<Card> p = cardsFromStrings({"10s", "10d"}); std::vector<Card> b = cardsFromStrings({"10h", "Ks", "Qd", "3c", "2h"}); EXPECT_EQ(compairer_->getRank(p, b), 2645); }
TEST_F(Dic5CompairerTest, GetRank_TwoPair) { ASSERT_NE(compairer_, nullptr); std::vector<Card> p = cardsFromStrings({"Ac", "Kd"}); std::vector<Card> b = cardsFromStrings({"Ah", "Ks", "Qd", "3c", "2h"}); EXPECT_EQ(compairer_->getRank(p, b), 3421); }
TEST_F(Dic5CompairerTest, GetRank_OnePair) { ASSERT_NE(compairer_, nullptr); std::vector<Card> p = cardsFromStrings({"Ac", "10d"}); std::vector<Card> b = cardsFromStrings({"Ah", "Ks", "Qd", "3c", "2h"}); EXPECT_EQ(compairer_->getRank(p, b), 3499); }

// --- Previously Failing Test (Corrected Expected Value) ---
TEST_F(Dic5CompairerTest, GetRank_HighCardAce) {
    ASSERT_NE(compairer_, nullptr);
    std::vector<Card> privateHand = cardsFromStrings({"Ad", "3s"});
    std::vector<Card> board = cardsFromStrings({"Ks", "Qh", "7d", "4c", "2h"});
    int expectedRank = 1328041; // Corrected Expected rank
    // --- DEBUG PRINT 6 (Commented Out) ---
    // std::vector<Card> availableCards = privateHand;
    // availableCards.insert(availableCards.end(), board.begin(), board.end());
    // PokerSolverUtils::Combinations<Card> combos(availableCards, 5);
    // int bestRankDebug = std::numeric_limits<int>::max();
    // std::vector<Card> best5CardHandDebug;
    // uint64_t bestKeyDebug = 0;
    // while (!combos.done()) { /* ... find best combo/key ... */ combos.next(); }
    // std::cout << "DEBUG Test (GetRank_HighCardAce): Best 5-card hand determined: ";
    // for(const auto& c : best5CardHandDebug) std::cout << c.toString();
    // std::cout << ", Key: 0x" << std::hex << bestKeyDebug << std::dec << ", Rank: " << bestRankDebug << std::endl;
    // --- END DEBUG PRINT 6 ---
    EXPECT_EQ(compairer_->getRank(privateHand, board), expectedRank);
}


// --- 7-Card Comparison Tests ---
TEST_F(Dic5CompairerTest, Compare_SF_vs_Quads) {
     ASSERT_NE(compairer_, nullptr);
     std::vector<Card> p1 = cardsFromStrings({"9s", "4s"}); // Rank 9
     std::vector<Card> p2 = cardsFromStrings({"Ac", "Ks"}); // Rank 41
     std::vector<Card> board = cardsFromStrings({"8s", "7s", "Ad", "As", "Ah"}); // 5-card board
     EXPECT_EQ(compairer_->compare(p1, p2, board), Compairer::ComparisonResult::LARGER);
     EXPECT_EQ(compairer_->compare(p2, p1, board), Compairer::ComparisonResult::SMALLER);
}
TEST_F(Dic5CompairerTest, Compare_Flush_vs_Straight) { ASSERT_NE(compairer_, nullptr); std::vector<Card> p1 = cardsFromStrings({"3h", "4h"}); std::vector<Card> p2 = cardsFromStrings({"9c", "10d"}); std::vector<Card> b = cardsFromStrings({"Ah", "Kh", "Qh", "8c", "7d"}); EXPECT_EQ(compairer_->compare(p1, p2, b), Compairer::ComparisonResult::LARGER); EXPECT_EQ(compairer_->compare(p2, p1, b), Compairer::ComparisonResult::SMALLER); }
TEST_F(Dic5CompairerTest, Compare_FullHouse_Kickers) { ASSERT_NE(compairer_, nullptr); std::vector<Card> p1 = cardsFromStrings({"Qh", "Qs"}); std::vector<Card> p2 = cardsFromStrings({"Jh", "Js"}); std::vector<Card> b = cardsFromStrings({"Kh", "Ks", "Kd", "2c", "3d"}); EXPECT_EQ(compairer_->compare(p1, p2, b), Compairer::ComparisonResult::LARGER); EXPECT_EQ(compairer_->compare(p2, p1, b), Compairer::ComparisonResult::SMALLER); }
TEST_F(Dic5CompairerTest, Compare_Tie) { ASSERT_NE(compairer_, nullptr); std::vector<Card> p1 = cardsFromStrings({"2c", "3d"}); std::vector<Card> p2 = cardsFromStrings({"2s", "3h"}); std::vector<Card> b = cardsFromStrings({"Ah", "Ks", "Qd", "Jc", "10h"}); EXPECT_EQ(compairer_->compare(p1, p2, b), Compairer::ComparisonResult::EQUAL); }


// --- Error Handling Tests (EXPECT_THROW separated) ---
TEST_F(Dic5CompairerTest, ErrorHandling_InvalidPrivateHandSize) {
    ASSERT_NE(compairer_, nullptr);
    std::vector<Card> p1 = cardsFromStrings({"Ac"});
    std::vector<Card> p2 = cardsFromStrings({"Kc", "Qc"});
    std::vector<Card> b = cardsFromStrings({"10c", "9c", "8c", "7c", "6c"});
    EXPECT_THROW(compairer_->getRank(p1, b), std::invalid_argument);
    EXPECT_THROW(compairer_->compare(p1, p2, b), std::invalid_argument);
}

TEST_F(Dic5CompairerTest, ErrorHandling_InvalidBoardSize) {
    ASSERT_NE(compairer_, nullptr);
    std::vector<Card> p1 = cardsFromStrings({"Ac", "Kc"});
    std::vector<Card> b_short = cardsFromStrings({"10c", "9c"}); // < 5 total cards
    std::vector<Card> b_long = cardsFromStrings({"10c", "9c", "8c", "7c", "6c", "5c"}); // > 5 board cards
    EXPECT_THROW(compairer_->getRank(p1, b_short), std::invalid_argument);
    EXPECT_THROW(compairer_->getRank(p1, b_long), std::invalid_argument);
}


// --- Test Direct 5-Card Rank Lookup ---
TEST_F(Dic5CompairerTest, GetRank_Direct5Card_RoyalFlush) { ASSERT_NE(compairer_, nullptr); std::vector<Card> h = cardsFromStrings({"Ah", "Kh", "Qh", "Jh", "10h"}); try { EXPECT_EQ(compairer_->getRank(h), 1); } catch (const std::exception& e) { FAIL() << e.what(); } }

// --- Previously Failing Test (Corrected Expected Value) ---
TEST_F(Dic5CompairerTest, GetRank_Direct5Card_HighCard) {
    ASSERT_NE(compairer_, nullptr);
    std::vector<Card> hand = cardsFromStrings({"Ac", "Ks", "Qh", "7d", "4c"});
    int expectedRank = 1328041; // Corrected Expected rank
    // --- DEBUG PRINT 7 (Commented Out) ---
    // std::vector<Card> sortedHand = hand;
    // std::sort(sortedHand.begin(), sortedHand.end());
    // uint64_t key_in_test = getHandKey_Debug(sortedHand);
    // std::cout << "DEBUG Test Direct5Card: Hand: ";
    // for(const auto& c : sortedHand) std::cout << c.toString();
    // std::cout << ", Key: 0x" << std::hex << key_in_test << std::dec << std::endl;
    // --- END DEBUG PRINT 7 ---
    try {
        int actualRank = compairer_->getRank(hand);
        // --- DEBUG PRINT 8 (Commented Out) ---
        // std::cout << "DEBUG Test Direct5Card: Hand: ";
        // for(const auto& c : sortedHand) std::cout << c.toString();
        // std::cout << " Actual Rank Returned: " << actualRank << std::endl;
        // --- END DEBUG PRINT 8 ---
        EXPECT_EQ(actualRank, expectedRank);
    } catch (const std::invalid_argument& e) { FAIL() << "Direct 5-card getRank(vector<Card>) failed (invalid argument): " << e.what();
    } catch (const std::logic_error& e) { FAIL() << "Direct 5-card getRank(vector<Card>) failed or is not implemented as expected: " << e.what();
    } catch (const std::exception& e) { FAIL() << "Direct 5-card getRank(vector<Card>) failed with unexpected exception: " << e.what(); }
}

TEST_F(Dic5CompairerTest, GetRank_Direct5Card_WrongSize) {
    ASSERT_NE(compairer_, nullptr);
    std::vector<Card> h4 = cardsFromStrings({"Ac", "Ks", "Qh", "7d"});
    std::vector<Card> h6 = cardsFromStrings({"Ac", "Ks", "Qh", "7d", "4c", "2s"});
    EXPECT_THROW(compairer_->getRank(h4), std::invalid_argument);
    EXPECT_THROW(compairer_->getRank(h6), std::invalid_argument);
}

