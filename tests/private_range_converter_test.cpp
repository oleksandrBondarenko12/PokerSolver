#include "gtest/gtest.h"
#include "tools/PrivateRangeConverter.h" // Adjust path if needed
#include "ranges/PrivateCards.h"             // Adjust path if needed
#include "Card.h"                      // Adjust path if needed
#include <vector>
#include <string>
#include <string_view>
#include <stdexcept>
#include <optional>
#include <unordered_set> // For checking results efficiently
#include <functional>    // For std::hash<PrivateCards>
#include <cmath>         // For std::abs
#include <sstream>       // For HandVectorToString helper

// Use namespaces
using namespace poker_solver::core;
using namespace poker_solver::ranges;

// Helper function to check if a specific hand exists in the results
// and optionally check its weight.
bool ContainsHand(const std::vector<PrivateCards>& results,
                  const PrivateCards& target_hand,
                  std::optional<double> expected_weight = std::nullopt) {
    for (const auto& hand : results) {
        if (hand == target_hand) { // Uses PrivateCards::operator==
            if (expected_weight.has_value()) {
                // Use EXPECT_DOUBLE_EQ style comparison for floating point
                return std::abs(hand.Weight() - expected_weight.value()) < 1e-9;
            }
            return true; // Found hand, weight check not requested
        }
    }
    return false; // Hand not found
}

// Helper to get a specific hand from results
std::optional<PrivateCards> GetHand(const std::vector<PrivateCards>& results,
                                    int c1_int, int c2_int) {
     PrivateCards target(c1_int, c2_int); // Create target (weight doesn't matter for comparison)
     for(const auto& hand : results) {
         if (hand == target) {
             return hand;
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


// --- Test Cases ---

// Test parsing pairs (e.g., QQ)
TEST(PrivateRangeConverterTest, ParsePairs) {
    auto results = PrivateRangeConverter::StringToPrivateCards("QQ");
    // Should generate C(4,2) = 6 combos for QQ
    EXPECT_EQ(results.size(), 6);
    // Check one specific combo exists with default weight 1.0
    PrivateCards qcqs(Card::StringToInt("Qc").value(), Card::StringToInt("Qs").value());
    EXPECT_TRUE(ContainsHand(results, qcqs, 1.0));
    // Check another
    PrivateCards qhqd(Card::StringToInt("Qh").value(), Card::StringToInt("Qd").value());
    EXPECT_TRUE(ContainsHand(results, qhqd, 1.0));
}

// Test parsing suited hands (e.g., AKs)
TEST(PrivateRangeConverterTest, ParseSuited) {
    auto results = PrivateRangeConverter::StringToPrivateCards("AKs");
    // Should generate 4 combos (AcKc, AdKd, AhKh, AsKs)
    EXPECT_EQ(results.size(), 4);
    PrivateCards ackc(Card::StringToInt("Ac").value(), Card::StringToInt("Kc").value());
    EXPECT_TRUE(ContainsHand(results, ackc, 1.0));
    PrivateCards asks(Card::StringToInt("As").value(), Card::StringToInt("Ks").value());
    EXPECT_TRUE(ContainsHand(results, asks, 1.0));
}

// Test parsing offsuit hands (e.g., AKo)
TEST(PrivateRangeConverterTest, ParseOffsuit) {
    auto results = PrivateRangeConverter::StringToPrivateCards("AKo");
    // Should generate 12 combos (4 * 3)
    EXPECT_EQ(results.size(), 12);
    PrivateCards ackd(Card::StringToInt("Ac").value(), Card::StringToInt("Kd").value());
    EXPECT_TRUE(ContainsHand(results, ackd, 1.0));
    PrivateCards adkc(Card::StringToInt("Ad").value(), Card::StringToInt("Kc").value());
    EXPECT_TRUE(ContainsHand(results, adkc, 1.0));
    // Check that a suited combo is NOT present
    PrivateCards ackc(Card::StringToInt("Ac").value(), Card::StringToInt("Kc").value());
    EXPECT_FALSE(ContainsHand(results, ackc));
}

// Test parsing specific hands (e.g., AcKc)
TEST(PrivateRangeConverterTest, ParseSpecific) {
    auto results = PrivateRangeConverter::StringToPrivateCards("AcKc");
    EXPECT_EQ(results.size(), 1);
    PrivateCards ackc(Card::StringToInt("Ac").value(), Card::StringToInt("Kc").value());
    EXPECT_TRUE(ContainsHand(results, ackc, 1.0));
}

// Test parsing weights
TEST(PrivateRangeConverterTest, ParseWeights) {
    auto results = PrivateRangeConverter::StringToPrivateCards("QQ:0.5, AKs:0.25, T9o");
    // Expected size: 6 (QQ) + 4 (AKs) + 12 (T9o) = 22 combos
    EXPECT_EQ(results.size(), 22);

    // Check weights
    PrivateCards qcqs(Card::StringToInt("Qc").value(), Card::StringToInt("Qs").value());
    EXPECT_TRUE(ContainsHand(results, qcqs, 0.5));

    PrivateCards adkd(Card::StringToInt("Ad").value(), Card::StringToInt("Kd").value());
    EXPECT_TRUE(ContainsHand(results, adkd, 0.25));

    PrivateCards tc9d(Card::StringToInt("Tc").value(), Card::StringToInt("9d").value());
    EXPECT_TRUE(ContainsHand(results, tc9d, 1.0)); // Default weight

    // Test near-zero weight exclusion
    results = PrivateRangeConverter::StringToPrivateCards("QQ:0.001, AKs");
    EXPECT_EQ(results.size(), 4); // Only AKs should remain
    PrivateCards ackc(Card::StringToInt("Ac").value(), Card::StringToInt("Kc").value());
    EXPECT_TRUE(ContainsHand(results, ackc, 1.0)); // AKs has default weight
}

// Test filtering with initial board
TEST(PrivateRangeConverterTest, BoardFiltering) {
    std::vector<int> board = { Card::StringToInt("Ac").value(), Card::StringToInt("Qd").value() };
    // Range: "AKs, QQ"
    // AKs: AcKc (Blocked), AdKd, AhKh, AsKs -> 3 remain
    // QQ: QcQs, QcQh, QsQh, QdQc(B), QdQs(B), QdQh(B) -> 3 remain
    // Total expected: 3 + 3 = 6
    auto results = PrivateRangeConverter::StringToPrivateCards("AKs,QQ", board);
    EXPECT_EQ(results.size(), 6);

    // Check blocked hands are absent
    PrivateCards ackc(Card::StringToInt("Ac").value(), Card::StringToInt("Kc").value());
    EXPECT_FALSE(ContainsHand(results, ackc));
    PrivateCards qdqc(Card::StringToInt("Qd").value(), Card::StringToInt("Qc").value());
    EXPECT_FALSE(ContainsHand(results, qdqc));

    // Check a remaining hand is present
    PrivateCards adkd(Card::StringToInt("Ad").value(), Card::StringToInt("Kd").value());
    EXPECT_TRUE(ContainsHand(results, adkd, 1.0));
    PrivateCards qcqs(Card::StringToInt("Qc").value(), Card::StringToInt("Qs").value());
    EXPECT_TRUE(ContainsHand(results, qcqs, 1.0));
}

// Test invalid syntax
TEST(PrivateRangeConverterTest, InvalidSyntax) {
    EXPECT_THROW(PrivateRangeConverter::StringToPrivateCards("AKx"), std::invalid_argument);
    EXPECT_THROW(PrivateRangeConverter::StringToPrivateCards("AAs"), std::invalid_argument); // Cannot be suited pair
    EXPECT_THROW(PrivateRangeConverter::StringToPrivateCards("AAo"), std::invalid_argument); // Cannot be offsuit pair
    EXPECT_THROW(PrivateRangeConverter::StringToPrivateCards("AK"), std::invalid_argument);  // Needs s, o, or specific suits
    EXPECT_THROW(PrivateRangeConverter::StringToPrivateCards("AcK"), std::invalid_argument); // Invalid length / format
    EXPECT_THROW(PrivateRangeConverter::StringToPrivateCards("QQ:abc"), std::invalid_argument); // Invalid weight
    EXPECT_THROW(PrivateRangeConverter::StringToPrivateCards("QQ:"), std::invalid_argument); // Empty weight
    // EXPECT_THROW(PrivateRangeConverter::StringToPrivateCards("QQ : 0.5"), std::invalid_argument); // This passes due to trimming
    // EXPECT_THROW(PrivateRangeConverter::StringToPrivateCards(",,"), std::invalid_argument); // Handled by StringSplit/empty component logic
    EXPECT_THROW(PrivateRangeConverter::StringToPrivateCards("AcAc"), std::invalid_argument); // Specific combo identical cards
}

// Test duplicate definitions
TEST(PrivateRangeConverterTest, DuplicateDefinitions) {
    // Duplicate within the string itself should throw
    EXPECT_THROW(PrivateRangeConverter::StringToPrivateCards("AKs,AcKc"), std::invalid_argument);
    EXPECT_THROW(PrivateRangeConverter::StringToPrivateCards("QQ,QcQd"), std::invalid_argument);
    EXPECT_THROW(PrivateRangeConverter::StringToPrivateCards("AKo,AcKd"), std::invalid_argument);
    // Test case sensitivity (should be treated as same hand)
    EXPECT_THROW(PrivateRangeConverter::StringToPrivateCards("ako,AcKd"), std::invalid_argument);
    // Different weights should still throw if hand is identical
    EXPECT_THROW(PrivateRangeConverter::StringToPrivateCards("QQ:0.5,QcQd:0.2"), std::invalid_argument);
}

