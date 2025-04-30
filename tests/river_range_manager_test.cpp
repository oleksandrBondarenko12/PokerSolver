#include "gtest/gtest.h"
#include "ranges/RiverRangeManager.h" // Adjust path
#include "compairer/Dic5Compairer.h"      // Adjust path
#include "ranges/PrivateCards.h"       // Adjust path
#include "Card.h"              // Adjust path
#include <vector>
#include <memory>
#include <stdexcept>
#include <optional>
#include <string>        // Include string for error message concatenation
#include <sstream>       // Include sstream for HandVectorToString helper
#include <iostream>      // For potential debug output

// Use namespaces
using namespace poker_solver::core;
using namespace poker_solver::eval; // For Dic5Compairer
using namespace poker_solver::ranges;

// Test fixture for RiverRangeManager tests
class RiverRangeManagerTest : public ::testing::Test {
 protected:
  // Path to the dictionary file (relative to build/executable directory)
  const std::string kDictionaryPath = "five_card_strength.txt";
  std::shared_ptr<Dic5Compairer> compairer_; // Use concrete type for setup
  std::unique_ptr<RiverRangeManager> manager_;

  // Sample ranges and boards
  std::vector<PrivateCards> range_p0_;
  std::vector<PrivateCards> range_p1_;
  std::vector<int> board_ints_;
  uint64_t board_mask_;

  void SetUp() override {
    try {
      // Ensure the dictionary file exists relative to where tests run (build dir)
      // The CMake custom command should handle copying it there.
      compairer_ = std::make_shared<Dic5Compairer>(kDictionaryPath);
      manager_ = std::make_unique<RiverRangeManager>(compairer_);

      // Create some sample ranges (adjust as needed)
      // Player 0: AcKd, AhAs, 7h6h, 2c2d
      range_p0_.emplace_back(Card::StringToInt("Ac").value(), Card::StringToInt("Kd").value()); // AKo (Index 0) -> Straight
      range_p0_.emplace_back(Card::StringToInt("Ah").value(), Card::StringToInt("As").value()); // AA (Index 1) -> Pair AA
      range_p0_.emplace_back(Card::StringToInt("7h").value(), Card::StringToInt("6h").value()); // 76s (Index 2) -> Q-High
      range_p0_.emplace_back(Card::StringToInt("2c").value(), Card::StringToInt("2d").value()); // 22 (Index 3) -> Trips 2s (board has 2s)

      // Player 1: Similar structure
      range_p1_.emplace_back(Card::StringToInt("Kc").value(), Card::StringToInt("Qd").value()); // KQo
      range_p1_.emplace_back(Card::StringToInt("Kh").value(), Card::StringToInt("Ks").value()); // KK
      range_p1_.emplace_back(Card::StringToInt("8s").value(), Card::StringToInt("7s").value()); // 87s
      range_p1_.emplace_back(Card::StringToInt("3c").value(), Card::StringToInt("3d").value()); // 33

      // Sample river board
      board_ints_ = {
          Card::StringToInt("5h").value(), // 14
          Card::StringToInt("Js").value(), // 39
          Card::StringToInt("Qc").value(), // 40
          Card::StringToInt("2s").value(), // 3   <--- Board has a Deuce
          Card::StringToInt("Td").value()  // 33
      };
      board_mask_ = Card::CardIntsToUint64(board_ints_);

    } catch (const std::exception& e) {
      FAIL() << "Setup failed: " << e.what();
    }
  }

  // Helper to convert card strings to integer vectors
  std::vector<int> StringsToInts(const std::vector<std::string>& card_strs) {
      std::vector<int> ints;
      ints.reserve(card_strs.size());
      for (const auto& s : card_strs) {
          std::optional<int> opt_int = Card::StringToInt(s); // Use optional
          if (!opt_int) {
              // Use basic_string concatenation or ostringstream for error message
              std::string error_msg = "Invalid card string in test helper: ";
              error_msg += s;
              throw std::runtime_error(error_msg);
          }
          ints.push_back(opt_int.value());
      }
      return ints;
  }

  // Helper to find a specific hand in the results
  std::optional<RiverCombs> FindHandInResults(
      const std::vector<RiverCombs>& results,
      const PrivateCards& target_hand) {
      for(const auto& combo : results) {
          if (combo.private_cards == target_hand) {
              return combo;
          }
      }
      return std::nullopt;
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

TEST(RiverRangeManagerConstructorTest, NullCompairer) {
    std::shared_ptr<Dic5Compairer> null_compairer = nullptr;
    EXPECT_THROW(RiverRangeManager mgr(null_compairer), std::invalid_argument);
}

TEST_F(RiverRangeManagerTest, BasicCalculationAndFiltering) {
    ASSERT_NE(manager_, nullptr);

    // P0: AcKd, AhAs, 7h6h, 2c2d
    // Board: 5h Js Qc 2s Td
    // Expected remaining for P0: All 4 hands (2c2d does NOT conflict with 2s)
    const auto& results_p0 = manager_->GetRiverCombos(0, range_p0_, board_mask_);
    EXPECT_EQ(results_p0.size(), 4); // Expect all 4 hands

    // Check if 2c2d IS present (it shouldn't be filtered)
    PrivateCards hand_22(Card::StringToInt("2c").value(), Card::StringToInt("2d").value());
    auto combo_22 = FindHandInResults(results_p0, hand_22);
    EXPECT_TRUE(combo_22.has_value()); // 22 should be present
    // Check rank and index for 22
    if(combo_22.has_value()) {
        int expected_rank_22 = compairer_->GetBestRankForCards(StringsToInts({"2c", "2d", "5h", "Js", "Qc", "2s", "Td"})); // Trips
        EXPECT_EQ(combo_22->rank, expected_rank_22);
        EXPECT_EQ(combo_22->original_range_index, 3); // Was the 3rd element in range_p0_
    }


    // Check if AKo is present and check its rank and original index
    PrivateCards hand_ako(Card::StringToInt("Ac").value(), Card::StringToInt("Kd").value());
    auto combo_ako = FindHandInResults(results_p0, hand_ako);
    ASSERT_TRUE(combo_ako.has_value());
    EXPECT_EQ(combo_ako->private_cards, hand_ako);
    int expected_rank_ako = compairer_->GetBestRankForCards(StringsToInts({"Ac", "Kd", "5h", "Js", "Qc", "2s", "Td"})); // Straight
    EXPECT_EQ(combo_ako->rank, expected_rank_ako);
    EXPECT_EQ(combo_ako->original_range_index, 0);

     // Check if AA is present and check its rank and original index
    PrivateCards hand_aa(Card::StringToInt("Ah").value(), Card::StringToInt("As").value());
    auto combo_aa = FindHandInResults(results_p0, hand_aa);
    ASSERT_TRUE(combo_aa.has_value());
    EXPECT_EQ(combo_aa->private_cards, hand_aa);
    int expected_rank_aa = compairer_->GetBestRankForCards(StringsToInts({"Ah", "As", "5h", "Js", "Qc", "2s", "Td"})); // Pair AA
    EXPECT_EQ(combo_aa->rank, expected_rank_aa);
    EXPECT_EQ(combo_aa->original_range_index, 1);

     // Check if 76s is present and check its rank and original index
    PrivateCards hand_76s(Card::StringToInt("7h").value(), Card::StringToInt("6h").value());
    auto combo_76s = FindHandInResults(results_p0, hand_76s);
    ASSERT_TRUE(combo_76s.has_value());
    EXPECT_EQ(combo_76s->private_cards, hand_76s);
    int expected_rank_76s = compairer_->GetBestRankForCards(StringsToInts({"7h", "6h", "5h", "Js", "Qc", "2s", "Td"})); // Q-High
    EXPECT_EQ(combo_76s->rank, expected_rank_76s);
    EXPECT_EQ(combo_76s->original_range_index, 2);
}

TEST_F(RiverRangeManagerTest, SortingOrder) {
     ASSERT_NE(manager_, nullptr);
     // P0: AcKd (Straight), AhAs (Pair AA), 7h6h (Q-High), 2c2d (Trips 2s)
     // Board: 5h Js Qc 2s Td
     const auto& results_p0 = manager_->GetRiverCombos(0, range_p0_, board_mask_);
     ASSERT_EQ(results_p0.size(), 4); // Expect all 4 hands

     // Get ranks
     int rank_ako = compairer_->GetHandRank(Card::CardsToUint64({Card("Ac"), Card("Kd")}), board_mask_); // Straight
     int rank_aa = compairer_->GetHandRank(Card::CardsToUint64({Card("Ah"), Card("As")}), board_mask_);  // Pair AA
     int rank_76s = compairer_->GetHandRank(Card::CardsToUint64({Card("7h"), Card("6h")}), board_mask_); // Q-High
     int rank_22 = compairer_->GetHandRank(Card::CardsToUint64({Card("2c"), Card("2d")}), board_mask_); // Trips 2s

     // Verify ranks are valid
     ASSERT_NE(rank_ako, Dic5Compairer::kInvalidRank) << "AKo rank invalid";
     ASSERT_NE(rank_aa, Dic5Compairer::kInvalidRank) << "AA rank invalid";
     ASSERT_NE(rank_76s, Dic5Compairer::kInvalidRank) << "76s rank invalid";
     ASSERT_NE(rank_22, Dic5Compairer::kInvalidRank) << "22 rank invalid";

     // Expected Strength Order: Straight < Trips < Pair < High Card
     // Expected Rank Number Order: rank_ako < rank_22 < rank_aa < rank_76s

     // Sorted order (worst first, using lhs.rank > rhs.rank): 76s, AA, 22, AKo
     EXPECT_EQ(results_p0[0].rank, rank_76s) << "First element should be worst rank (76s)";
     EXPECT_EQ(results_p0[1].rank, rank_aa) << "Second element should be next worst rank (AA)";
     EXPECT_EQ(results_p0[2].rank, rank_22) << "Third element should be next worst rank (22)";
     EXPECT_EQ(results_p0[3].rank, rank_ako) << "Last element should be best rank (AKo)";

     // Verify the ranks are ordered descending numerically in the result vector
     EXPECT_GT(results_p0[0].rank, results_p0[1].rank);
     EXPECT_GT(results_p0[1].rank, results_p0[2].rank);
     EXPECT_GT(results_p0[2].rank, results_p0[3].rank);
}

TEST_F(RiverRangeManagerTest, Caching) {
    ASSERT_NE(manager_, nullptr);
    const auto& results1 = manager_->GetRiverCombos(0, range_p0_, board_mask_);
    ASSERT_FALSE(results1.empty());
    const auto& results2 = manager_->GetRiverCombos(0, range_p0_, board_mask_);
    ASSERT_FALSE(results2.empty());
    EXPECT_EQ(&results1, &results2) << "Second call did not return the cached object.";

    uint64_t board2_mask = Card::CardIntsToUint64({
        Card::StringToInt("Kh").value(), Card::StringToInt("Ks").value(),
        Card::StringToInt("5c").value(), Card::StringToInt("8d").value(),
        Card::StringToInt("9h").value() });
    const auto& results_p1_b2 = manager_->GetRiverCombos(1, range_p1_, board2_mask);
    ASSERT_FALSE(results_p1_b2.empty());
    const auto& results_p1_b2_again = manager_->GetRiverCombos(1, range_p1_, board2_mask);
    EXPECT_EQ(&results_p1_b2, &results_p1_b2_again);
    const auto& results1_again = manager_->GetRiverCombos(0, range_p0_, board_mask_);
    EXPECT_EQ(&results1, &results1_again);
}

TEST_F(RiverRangeManagerTest, InvalidInputs) {
    ASSERT_NE(manager_, nullptr);
    EXPECT_THROW(manager_->GetRiverCombos(2, range_p0_, board_mask_), std::out_of_range);
    uint64_t board_4cards = Card::CardIntsToUint64(
        {board_ints_[0], board_ints_[1], board_ints_[2], board_ints_[3]});
    uint64_t board_6cards = board_mask_ | Card::CardIntToUint64(Card::StringToInt("3s").value());
    EXPECT_THROW(manager_->GetRiverCombos(0, range_p0_, board_4cards), std::invalid_argument);
    EXPECT_THROW(manager_->GetRiverCombos(0, range_p0_, board_6cards), std::invalid_argument);
    EXPECT_THROW(manager_->GetRiverCombos(0, range_p0_, 0ULL), std::invalid_argument);
}

TEST_F(RiverRangeManagerTest, BoardVectorOverload) {
     ASSERT_NE(manager_, nullptr);
     const auto& results_vec = manager_->GetRiverCombos(0, range_p0_, board_ints_);
     const auto& results_mask = manager_->GetRiverCombos(0, range_p0_, board_mask_);
     EXPECT_EQ(&results_vec, &results_mask);
     EXPECT_EQ(results_vec.size(), 4); // Corrected Expected Size
     std::vector<int> board_4ints = {board_ints_[0], board_ints_[1], board_ints_[2], board_ints_[3]};
     EXPECT_THROW(manager_->GetRiverCombos(0, range_p0_, board_4ints), std::invalid_argument);
}
