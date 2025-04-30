#include "gtest/gtest.h"
#include "tools/utils.h"        // Adjust path if needed
#include "ranges/PrivateCards.h" // Adjust path if needed
#include "Card.h"          // Adjust path if needed
#include <vector>
#include <string>
#include <numeric> // For std::iota
#include <map>     // For checking results
#include <optional> // For std::optional
#include <sstream>  // For std::ostringstream
#include <stdexcept>// For std::runtime_error
#include <cmath>    // For std::abs
#include <iostream> // For debug
#include <iomanip>  // For debug output formatting

// Use namespaces
using namespace poker_solver::core;
using namespace poker_solver::utils; // For ExchangeColorIsomorphism

// Test fixture for Utils tests
class UtilsTest : public ::testing::Test {
 protected:
  // Sample range including isomorphic hands when swapping clubs (0) and spades (3)
  // Also includes pairs for h(2)/d(1) swap.
  std::vector<PrivateCards> test_range_;
  std::vector<double> test_values_;

  // Store original indices for verification
  std::map<PrivateCards, size_t> original_indices_;

  void SetUp() override {
      try {
          test_range_ = {
              PrivateCards(Card::StringToInt("Ac").value(), Card::StringToInt("Kc").value()), // 0: AcKc (c=0) -> iso AsKs (idx 1)
              PrivateCards(Card::StringToInt("As").value(), Card::StringToInt("Ks").value()), // 1: AsKs (s=3) -> iso AcKc (idx 0)
              PrivateCards(Card::StringToInt("Ac").value(), Card::StringToInt("Qc").value()), // 2: AcQc (c=0) -> iso AsQs (idx 3)
              PrivateCards(Card::StringToInt("As").value(), Card::StringToInt("Qs").value()), // 3: AsQs (s=3) -> iso AcQc (idx 2)
              PrivateCards(Card::StringToInt("Ks").value(), Card::StringToInt("Kd").value()), // 4: KsKd (s=3, d=1) -> iso KcKd (idx 5)
              PrivateCards(Card::StringToInt("Kc").value(), Card::StringToInt("Kd").value()), // 5: KcKd (c=0, d=1) -> iso KsKd (idx 4)
              PrivateCards(Card::StringToInt("Ah").value(), Card::StringToInt("Kh").value()), // 6: AhKh (h=2) -> iso AdKd (idx 13)
              PrivateCards(Card::StringToInt("7d").value(), Card::StringToInt("6c").value()), // 7: 7d6c (d=1, c=0) -> iso 7d6s (idx 8)
              PrivateCards(Card::StringToInt("7d").value(), Card::StringToInt("6s").value()), // 8: 7d6s (d=1, s=3) -> iso 7d6c (idx 7)
              PrivateCards(Card::StringToInt("Kc").value(), Card::StringToInt("Kh").value()), // 9: KcKh (c=0, h=2) -> iso KsKh (idx 11)
              PrivateCards(Card::StringToInt("Kd").value(), Card::StringToInt("Kh").value()), // 10: KdKh (d=1, h=2) -> no c/s swap
              PrivateCards(Card::StringToInt("Kh").value(), Card::StringToInt("Ks").value()), // 11: KhKs (h=2, s=3) -> iso KhKc (idx 9)
              PrivateCards(Card::StringToInt("Ac").value(), Card::StringToInt("Kc").value()), // 12: AcKc (Duplicate of 0, c=0) -> iso AsKs (idx 1)
              PrivateCards(Card::StringToInt("Ad").value(), Card::StringToInt("Kd").value()), // 13: AdKd (d=1) -> iso AhKh (idx 6)
              PrivateCards(Card::StringToInt("Ah").value(), Card::StringToInt("Kh").value()), // 14: AhKh (Duplicate of 6, h=2) -> iso AdKd (idx 13)
              PrivateCards(Card::StringToInt("As").value(), Card::StringToInt("Ks").value())  // 15: AsKs (Duplicate of 1, s=3) -> iso AcKc (idx 0)
          };

          // Assign simple values corresponding to original index
          test_values_.resize(test_range_.size());
          std::iota(test_values_.begin(), test_values_.end(), 0.0); // 0.0, 1.0, 2.0, ...

          // Store original indices (handle duplicates - map stores first encountered)
          for(size_t i = 0; i < test_range_.size(); ++i) {
              original_indices_.try_emplace(test_range_[i], i);
          }

      } catch (const std::exception& e) {
          FAIL() << "Setup failed: " << e.what();
      }
  }
};

// Test swapping clubs (0) and spades (3)
TEST_F(UtilsTest, ExchangeClubSpade) {
    int club_idx = Card::SuitCharToIndex('c'); // 0
    int spade_idx = Card::SuitCharToIndex('s'); // 3
    std::vector<double> original_values = test_values_;
    ExchangeColorIsomorphism(test_values_, test_range_, club_idx, spade_idx);

    // --- Debug Output (Optional) ---
    // std::cout << "[DEBUG UtilsTest] Final test_values_ for Club/Spade:" << std::endl;
    // std::cout << std::fixed << std::setprecision(1); // Format output
    // for(size_t k=0; k < test_values_.size(); ++k) {
    //     std::cout << "  idx " << std::setw(2) << k << ": " << std::setw(4) << test_values_[k]
    //               << " (Original: " << std::setw(4) << original_values[k] << ")" << std::endl;
    // }
    // --- END DEBUG ---


    // Expected swaps based on trace: 0<->1, 2<->3, 4<->5, 7<->8, 9<->11
    // Indices 12 and 15 map to already swapped indices (1 and 0) and should remain unchanged.

    // Verify swaps happened correctly
    EXPECT_DOUBLE_EQ(test_values_[0], original_values[1]) << "Index 0";
    EXPECT_DOUBLE_EQ(test_values_[1], original_values[0]) << "Index 1";
    EXPECT_DOUBLE_EQ(test_values_[2], original_values[3]) << "Index 2";
    EXPECT_DOUBLE_EQ(test_values_[3], original_values[2]) << "Index 3";
    EXPECT_DOUBLE_EQ(test_values_[4], original_values[5]) << "Index 4";
    EXPECT_DOUBLE_EQ(test_values_[5], original_values[4]) << "Index 5";
    EXPECT_DOUBLE_EQ(test_values_[7], original_values[8]) << "Index 7";
    EXPECT_DOUBLE_EQ(test_values_[8], original_values[7]) << "Index 8";
    EXPECT_DOUBLE_EQ(test_values_[9], original_values[11])<< "Index 9";
    EXPECT_DOUBLE_EQ(test_values_[11], original_values[9])<< "Index 11";

    // Verify unchanged values (including duplicates whose partners were already swapped)
    EXPECT_DOUBLE_EQ(test_values_[6], original_values[6]) << "Index 6";
    EXPECT_DOUBLE_EQ(test_values_[10], original_values[10])<< "Index 10";
    EXPECT_DOUBLE_EQ(test_values_[12], original_values[12])<< "Index 12 (Duplicate)"; // Corrected
    EXPECT_DOUBLE_EQ(test_values_[13], original_values[13])<< "Index 13";
    EXPECT_DOUBLE_EQ(test_values_[14], original_values[14])<< "Index 14";
    EXPECT_DOUBLE_EQ(test_values_[15], original_values[15])<< "Index 15 (Duplicate)"; // Corrected
}

// Test swapping suits that are the same (should do nothing)
TEST_F(UtilsTest, ExchangeSameSuit) {
    int heart_idx = Card::SuitCharToIndex('h'); // 2
    std::vector<double> original_values = test_values_;
    ExchangeColorIsomorphism(test_values_, test_range_, heart_idx, heart_idx);
    EXPECT_EQ(test_values_, original_values); // Vector should be unchanged
}

// Test swapping hearts (2) and diamonds (1)
TEST_F(UtilsTest, ExchangeHeartDiamond) {
    int heart_idx = Card::SuitCharToIndex('h'); // 2
    int diamond_idx = Card::SuitCharToIndex('d'); // 1
    std::vector<double> original_values = test_values_;

    ExchangeColorIsomorphism(test_values_, test_range_, heart_idx, diamond_idx);

    // Expected swaps based on corrected trace: 4<->11, 5<->9, 6<->13
    // Indices 10, 14 map to already swapped indices (4, 13) and should remain unchanged.

    // Check swapped values
    EXPECT_DOUBLE_EQ(test_values_[4], original_values[11]) << "Index 4 (KsKd) should get val from 11 (KhKs)";
    EXPECT_DOUBLE_EQ(test_values_[11], original_values[4]) << "Index 11 (KhKs) should get val from 4 (KsKd)";
    EXPECT_DOUBLE_EQ(test_values_[5], original_values[9]) << "Index 5 (KcKd) should get val from 9 (KcKh)";
    EXPECT_DOUBLE_EQ(test_values_[9], original_values[5]) << "Index 9 (KcKh) should get val from 5 (KcKd)";
    EXPECT_DOUBLE_EQ(test_values_[6], original_values[13]) << "Index 6 (AhKh) should get val from 13 (AdKd)";
    EXPECT_DOUBLE_EQ(test_values_[13], original_values[6]) << "Index 13 (AdKd) should get val from 6 (AhKh)";

    // Check explicitly unchanged values (indices 0, 1, 2, 3, 7, 8, 10, 12, 14, 15)
    EXPECT_DOUBLE_EQ(test_values_[0], original_values[0]) << "Index 0 unchanged";
    EXPECT_DOUBLE_EQ(test_values_[1], original_values[1]) << "Index 1 unchanged";
    EXPECT_DOUBLE_EQ(test_values_[2], original_values[2]) << "Index 2 unchanged";
    EXPECT_DOUBLE_EQ(test_values_[3], original_values[3]) << "Index 3 unchanged";
    EXPECT_DOUBLE_EQ(test_values_[7], original_values[7]) << "Index 7 unchanged";
    EXPECT_DOUBLE_EQ(test_values_[8], original_values[8]) << "Index 8 unchanged";
    EXPECT_DOUBLE_EQ(test_values_[10], original_values[10]) << "Index 10 unchanged (iso partner not in range or already swapped)";
    EXPECT_DOUBLE_EQ(test_values_[12], original_values[12]) << "Index 12 unchanged";
    EXPECT_DOUBLE_EQ(test_values_[14], original_values[14]) << "Index 14 unchanged (iso partner already swapped)";
    EXPECT_DOUBLE_EQ(test_values_[15], original_values[15]) << "Index 15 unchanged";
}

// Test edge cases
TEST_F(UtilsTest, EdgeCases) {
    std::vector<double> empty_values;
    std::vector<PrivateCards> empty_range;
    int c = 0, s = 3;
    EXPECT_NO_THROW(ExchangeColorIsomorphism(empty_values, empty_range, c, s));
    std::vector<double> one_value = {1.0};
    EXPECT_THROW(ExchangeColorIsomorphism(one_value, empty_range, c, s), std::invalid_argument);
    EXPECT_THROW(ExchangeColorIsomorphism(empty_values, test_range_, c, s), std::invalid_argument);
    EXPECT_THROW(ExchangeColorIsomorphism(test_values_, test_range_, -1, s), std::out_of_range);
    EXPECT_THROW(ExchangeColorIsomorphism(test_values_, test_range_, c, 4), std::out_of_range);
}
