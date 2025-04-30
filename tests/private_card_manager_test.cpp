#include "gtest/gtest.h"
#include "ranges/PrivateCardsManager.h" // Adjust path
#include "ranges/PrivateCards.h"           // Adjust path
#include "Card.h"                    // Adjust path
#include <vector>
#include <memory>
#include <stdexcept>
#include <optional>
#include <numeric> // For std::accumulate
#include <cmath>   // For std::abs
#include <iostream> // For debug/warning output
#include <iomanip>  // For std::fixed, std::setprecision
#include <sstream>  // For HandVectorToString helper

// Use namespaces
using namespace poker_solver::core;
using namespace poker_solver::ranges;

// Test fixture for PrivateCardsManager tests
class PrivateCardsManagerTest : public ::testing::Test {
 protected:
  // Sample ranges (very small for manual probability calculation)
  // P0: AA, KK, AKs
  // P1: QQ, AKo
  std::vector<PrivateCards> range_p0_;
  std::vector<PrivateCards> range_p1_;

  // Card integers for convenience
  int ac_, ad_, ah_, as_, kc_, kd_, kh_, ks_, qc_, qd_, qh_, qs_;
  int h5_; // For board
  int qc_int_; // Specific int needed for test

  void SetUp() override {
    try {
      // Initialize card integers
      ac_ = Card::StringToInt("Ac").value(); ad_ = Card::StringToInt("Ad").value();
      ah_ = Card::StringToInt("Ah").value(); as_ = Card::StringToInt("As").value();
      kc_ = Card::StringToInt("Kc").value(); kd_ = Card::StringToInt("Kd").value();
      kh_ = Card::StringToInt("Kh").value(); ks_ = Card::StringToInt("Ks").value();
      qc_ = Card::StringToInt("Qc").value(); qd_ = Card::StringToInt("Qd").value();
      qh_ = Card::StringToInt("Qh").value(); qs_ = Card::StringToInt("Qs").value();
      h5_ = Card::StringToInt("5h").value();
      qc_int_ = Card::StringToInt("Qc").value(); // Store Qc int

      // Player 0 Range (6 AA combos, 6 KK combos, 4 AKs combos = 16 total)
      range_p0_.emplace_back(ac_, ad_); range_p0_.emplace_back(ac_, ah_); // Indices 0, 1
      range_p0_.emplace_back(ac_, as_); range_p0_.emplace_back(ad_, ah_); // Indices 2, 3
      range_p0_.emplace_back(ad_, as_); range_p0_.emplace_back(ah_, as_); // Indices 4, 5 (AA done)
      range_p0_.emplace_back(kc_, kd_); range_p0_.emplace_back(kc_, kh_); // Indices 6, 7
      range_p0_.emplace_back(kc_, ks_); range_p0_.emplace_back(kd_, kh_); // Indices 8, 9
      range_p0_.emplace_back(kd_, ks_); range_p0_.emplace_back(kh_, ks_); // Indices 10, 11 (KK done)
      range_p0_.emplace_back(ac_, kc_); range_p0_.emplace_back(ad_, kd_); // Indices 12, 13
      range_p0_.emplace_back(ah_, kh_); range_p0_.emplace_back(as_, ks_); // Indices 14, 15 (AKs done)

      // Player 1 Range (6 QQ combos, 12 AKo combos = 18 total)
      range_p1_.emplace_back(qc_, qd_); range_p1_.emplace_back(qc_, qh_); // Indices 0, 1
      range_p1_.emplace_back(qc_, qs_); range_p1_.emplace_back(qd_, qh_); // Indices 2, 3
      range_p1_.emplace_back(qd_, qs_); range_p1_.emplace_back(qh_, qs_); // Indices 4, 5 (QQ done)
      range_p1_.emplace_back(ac_, kd_); range_p1_.emplace_back(ac_, kh_); range_p1_.emplace_back(ac_, ks_); // Indices 6, 7, 8
      range_p1_.emplace_back(ad_, kc_); range_p1_.emplace_back(ad_, kh_); range_p1_.emplace_back(ad_, ks_); // Indices 9, 10, 11
      range_p1_.emplace_back(ah_, kc_); range_p1_.emplace_back(ah_, kd_); range_p1_.emplace_back(ah_, ks_); // Indices 12, 13, 14
      range_p1_.emplace_back(as_, kc_); range_p1_.emplace_back(as_, kd_); range_p1_.emplace_back(as_, kh_); // Indices 15, 16, 17 (AKo done)

    } catch (const std::exception& e) {
      FAIL() << "Setup failed: " << e.what();
    }
  }

  // Helper to calculate opponent weight sum for a given hand and board
  double CalculateOpponentWeightSum(
      const PrivateCards& player_hand,
      const std::vector<PrivateCards>& oppo_range,
      uint64_t board_mask) {
      uint64_t player_mask = player_hand.GetBoardMask();
      double opponent_weight_sum = 0.0;
      // std::cout << "      Calculating oppo sum for " << player_hand.ToString() << std::endl; // DEBUG
      for (const auto& oppo_hand : oppo_range) {
          uint64_t oppo_mask = oppo_hand.GetBoardMask();
          bool blocked_by_board = Card::DoBoardsOverlap(oppo_mask, board_mask);
          bool blocked_by_player = Card::DoBoardsOverlap(oppo_mask, player_mask);
          if (!blocked_by_board && !blocked_by_player) {
              opponent_weight_sum += oppo_hand.Weight();
              // std::cout << "        Adding weight for " << oppo_hand.ToString() << std::endl; // DEBUG
          } else {
              // std::cout << "        Skipping " << oppo_hand.ToString() << " (Board: " << blocked_by_board << ", Player: " << blocked_by_player << ")" << std::endl; // DEBUG
          }
      }
      // std::cout << "      Result = " << opponent_weight_sum << std::endl; // DEBUG
      return opponent_weight_sum;
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

// --- Tests ---

TEST(PrivateCardsManagerConstructorValidation, InvalidInputs) {
    // Empty ranges
    EXPECT_THROW(PrivateCardsManager({}, 0), std::invalid_argument);
    // Mismatched player count (currently restricted to 2)
    std::vector<PrivateCards> r1 = {PrivateCards(0,1)};
    std::vector<PrivateCards> r2 = {PrivateCards(2,3)};
    std::vector<PrivateCards> r3 = {PrivateCards(4,5)};
    EXPECT_THROW(PrivateCardsManager({r1}, 0), std::invalid_argument); // 1 player
    EXPECT_THROW(PrivateCardsManager({r1, r2, r3}, 0), std::invalid_argument); // 3 players
}

TEST_F(PrivateCardsManagerTest, Getters) {
    std::vector<std::vector<PrivateCards>> ranges = {range_p0_, range_p1_};
    PrivateCardsManager pcm(ranges, 0ULL); // Empty board

    EXPECT_EQ(pcm.GetNumPlayers(), 2);
    EXPECT_EQ(pcm.GetPlayerRange(0).size(), 16);
    EXPECT_EQ(pcm.GetPlayerRange(1).size(), 18);
    EXPECT_THROW(pcm.GetPlayerRange(2), std::out_of_range);
}

TEST_F(PrivateCardsManagerTest, GetOpponentHandIndex) {
     std::vector<std::vector<PrivateCards>> ranges = {range_p0_, range_p1_};
     PrivateCardsManager pcm(ranges, 0ULL); // Empty board

     // Find P1's index for AcKd (which is index 6 in range_p1_)
     // starting from P0's AcKc (index 12 in range_p0_) - should fail
     EXPECT_FALSE(pcm.GetOpponentHandIndex(0, 1, 12).has_value());

     // Find P1's index for AcKd (index 6) starting from P1's AcKd (index 6)
     EXPECT_EQ(pcm.GetOpponentHandIndex(1, 1, 6).value(), 6);

     // Find P0's index for AcAd (index 0) starting from P1's AcKd (index 6) - should fail
     EXPECT_FALSE(pcm.GetOpponentHandIndex(1, 0, 6).has_value());

     // Find P1's index for QQ (e.g., QcQd, index 0) starting from P0's AA (index 0) - should fail
     EXPECT_FALSE(pcm.GetOpponentHandIndex(0, 1, 0).has_value());

     // Test invalid inputs
     EXPECT_FALSE(pcm.GetOpponentHandIndex(0, 1, 99).has_value()); // Invalid hand index
     EXPECT_FALSE(pcm.GetOpponentHandIndex(2, 1, 0).has_value()); // Invalid player index
     EXPECT_FALSE(pcm.GetOpponentHandIndex(0, 2, 0).has_value()); // Invalid player index
}

TEST_F(PrivateCardsManagerTest, GetInitialReachProbsEmptyBoard) {
    std::vector<std::vector<PrivateCards>> ranges = {range_p0_, range_p1_};
    PrivateCardsManager pcm(ranges, 0ULL); // Empty board

    const auto& probs_p0 = pcm.GetInitialReachProbs(0);
    const auto& probs_p1 = pcm.GetInitialReachProbs(1);

    EXPECT_EQ(probs_p0.size(), range_p0_.size());
    EXPECT_EQ(probs_p1.size(), range_p1_.size());

    // Manual calculation for a specific hand (e.g., P0's AcAd)
    // P0 has AcAd. P1 range size = 18.
    // P1 hands blocked by AcAd: AKo (AdKc, AdKh, AdKs), AKo (AcKd, AcKh, AcKs) = 6 hands
    // Possible P1 hands = 18 - 6 = 12.
    // P0 weight = 1.0. P1 weight = 1.0.
    // Relative prob = 1.0 * (12 * 1.0) = 12.0
    double p0_sum = 0;
    for(size_t i=0; i<range_p0_.size(); ++i) {
        const auto& p0_hand = range_p0_[i];
        uint64_t p0_mask = p0_hand.GetBoardMask();
        double oppo_sum = 0;
        for(const auto& p1_hand : range_p1_) {
            if (!Card::DoBoardsOverlap(p0_mask, p1_hand.GetBoardMask())) {
                oppo_sum += p1_hand.Weight();
            }
        }
        p0_sum += p0_hand.Weight() * oppo_sum;
    }
    ASSERT_GT(p0_sum, 0);
    double expected_prob_acad = (1.0 * 12.0) / p0_sum;
    EXPECT_NEAR(probs_p0[0], expected_prob_acad, 1e-9); // Check AcAd (index 0)

    // Check sums
    double total_prob_p0 = std::accumulate(probs_p0.begin(), probs_p0.end(), 0.0);
    double total_prob_p1 = std::accumulate(probs_p1.begin(), probs_p1.end(), 0.0);
    EXPECT_NEAR(total_prob_p0, 1.0, 1e-9);
    EXPECT_NEAR(total_prob_p1, 1.0, 1e-9);
}

TEST_F(PrivateCardsManagerTest, GetInitialReachProbsWithBoard) {
    // Board: Ac Qd 5h
    std::vector<int> board_ints = {ac_, qd_, h5_};
    uint64_t board_mask = Card::CardIntsToUint64(board_ints);
    std::vector<std::vector<PrivateCards>> ranges = {range_p0_, range_p1_};
    PrivateCardsManager pcm(ranges, board_mask);

    const auto& probs_p0 = pcm.GetInitialReachProbs(0);
    const auto& probs_p1 = pcm.GetInitialReachProbs(1);

    EXPECT_EQ(probs_p0.size(), range_p0_.size());
    EXPECT_EQ(probs_p1.size(), range_p1_.size());

    // 1. Check hands blocked DIRECTLY by board have zero probability
    EXPECT_DOUBLE_EQ(probs_p0[0], 0.0) << "AcAd blocked by Ac"; // AcAd index 0
    EXPECT_DOUBLE_EQ(probs_p0[1], 0.0) << "AcAh blocked by Ac"; // AcAh index 1
    EXPECT_DOUBLE_EQ(probs_p0[2], 0.0) << "AcAs blocked by Ac"; // AcAs index 2
    EXPECT_DOUBLE_EQ(probs_p0[12], 0.0) << "AcKc blocked by Ac"; // AcKc index 12
    EXPECT_DOUBLE_EQ(probs_p1[0], 0.0) << "QcQd blocked by Qd"; // QcQd index 0
    EXPECT_DOUBLE_EQ(probs_p1[3], 0.0) << "QdQh blocked by Qd"; // QdQh index 3
    EXPECT_DOUBLE_EQ(probs_p1[4], 0.0) << "QdQs blocked by Qd"; // QdQs index 4
    EXPECT_DOUBLE_EQ(probs_p1[6], 0.0) << "AcKd blocked by Ac"; // AcKd index 6
    // AdKc (Index 9) is NOT blocked by Ac Qd 5h

    // 2. Check hands NOT blocked by board have NON-ZERO probability
    EXPECT_GT(probs_p0[11], 0.0) << "KhKs should be possible"; // KhKs (Index 11)
    EXPECT_GT(probs_p0[15], 0.0) << "AsKs should be possible"; // AsKs (Index 15)
    EXPECT_GT(probs_p1[1], 0.0) << "QcQh should be possible"; // QcQh (Index 1)
    EXPECT_GT(probs_p1[9], 0.0) << "AdKc should be possible"; // AdKc (Index 9)
    EXPECT_GT(probs_p1[10], 0.0) << "AdKh should be possible"; // AdKh (Index 10)


    // 3. Check relative probabilities for two non-blocked hands for P0
    //    Hand A: KhKs (Index 11) vs Hand B: AhAs (Index 5)
    //    Board: Ac Qd 5h
    PrivateCards handA = range_p0_[11]; // KhKs
    PrivateCards handB = range_p0_[5];  // AhAs
    double oppo_sum_A = CalculateOpponentWeightSum(handA, range_p1_, board_mask);
    double oppo_sum_B = CalculateOpponentWeightSum(handB, range_p1_, board_mask);
    double rel_prob_A = handA.Weight() * oppo_sum_A;
    double rel_prob_B = handB.Weight() * oppo_sum_B;

    // --- Debug Output (Optional) ---
    // std::cout << std::fixed << std::setprecision(10);
    // std::cout << "[DEBUG ReachProbs] Hand A (KhKs): oppo_sum=" << oppo_sum_A << ", rel_prob=" << rel_prob_A << std::endl;
    // std::cout << "[DEBUG ReachProbs] Hand B (AhAs): oppo_sum=" << oppo_sum_B << ", rel_prob=" << rel_prob_B << std::endl;
    // --- End Debug Output ---

    ASSERT_GT(oppo_sum_A, 1e-12) << "Opponent sum for KhKs is zero";
    ASSERT_GT(oppo_sum_B, 1e-12) << "Opponent sum for AhAs is zero";
    ASSERT_GT(rel_prob_A, 1e-12);
    ASSERT_GT(rel_prob_B, 1e-12);

    double final_prob_A = probs_p0[11];
    double final_prob_B = probs_p0[5];

    // --- Debug Output (Optional) ---
    // std::cout << "[DEBUG ReachProbs] Final Prob A (KhKs): " << final_prob_A << std::endl;
    // std::cout << "[DEBUG ReachProbs] Final Prob B (AhAs): " << final_prob_B << std::endl;
    // if (final_prob_B > 1e-12 && rel_prob_B > 1e-12) {
    //    std::cout << "[DEBUG ReachProbs] Ratio Final (A/B): " << (final_prob_A / final_prob_B) << std::endl;
    //    std::cout << "[DEBUG ReachProbs] Ratio Rel   (A/B): " << (rel_prob_A / rel_prob_B) << std::endl;
    // }
    // --- End Debug Output ---

    // Check if the ratio of final probabilities matches the ratio of relative probabilities
    if (std::abs(final_prob_B) > 1e-12 && std::abs(rel_prob_B) > 1e-12) {
         EXPECT_NEAR(final_prob_A / final_prob_B, rel_prob_A / rel_prob_B, 1e-9)
             << "Relative probability ratio mismatch for KhKs vs AhAs";
    } else if (std::abs(final_prob_A) > 1e-12 || std::abs(rel_prob_A) > 1e-12) {
        FAIL() << "Probability B is zero while A is non-zero, cannot compare ratios.";
    } // else both are zero, ratio is undefined but effectively equal (0/0), pass.


    // 4. Check sums == 1.0
    double total_prob_p0 = std::accumulate(probs_p0.begin(), probs_p0.end(), 0.0);
    double total_prob_p1 = std::accumulate(probs_p1.begin(), probs_p1.end(), 0.0);
    EXPECT_NEAR(total_prob_p0, 1.0, 1e-9) << "P0 probabilities do not sum to 1.0";
    EXPECT_NEAR(total_prob_p1, 1.0, 1e-9) << "P1 probabilities do not sum to 1.0";
}
