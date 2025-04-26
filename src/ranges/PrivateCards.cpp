#include "private_cards.h" // Adjust path if necessary

#include <stdexcept>
#include <sstream>
#include <algorithm> // For std::swap
#include <functional> // For std::hash

namespace poker_solver {
namespace core {

// --- Helper: Combine Hashes ---
// Function to combine two hash values (often used in hash specializations).
// Based on boost::hash_combine.
template <class T>
inline void hash_combine(std::size_t& seed, const T& v) {
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

// --- Constructors ---

PrivateCards::PrivateCards()
    : card1_int_(-1), // Indicate invalid state
      card2_int_(-1),
      weight_(0.0),
      board_mask_(0) {}

PrivateCards::PrivateCards(int card1_int, int card2_int, double weight) {
    Initialize(card1_int, card2_int, weight);
}

// Private helper for initialization logic shared by constructors.
void PrivateCards::Initialize(int c1, int c2, double w) {
    // Validate individual cards
    if (!Card::IsValidCardInt(c1)) {
        std::ostringstream oss;
        oss << "Invalid integer for card 1: " << c1;
        throw std::out_of_range(oss.str());
    }
     if (!Card::IsValidCardInt(c2)) {
        std::ostringstream oss;
        oss << "Invalid integer for card 2: " << c2;
        throw std::out_of_range(oss.str());
    }
    if (c1 == c2) {
         std::ostringstream oss;
         oss << "Private cards cannot be identical: " << c1;
        throw std::invalid_argument(oss.str());
    }

    // Ensure canonical order (card1_int_ <= card2_int_)
    card1_int_ = std::min(c1, c2);
    card2_int_ = std::max(c1, c2);
    weight_ = w;

    // Pre-calculate board mask
    board_mask_ = Card::CardIntToUint64(card1_int_) |
                  Card::CardIntToUint64(card2_int_);
}


// --- Accessors ---

std::string PrivateCards::ToString() const {
    // Check for the default-constructed invalid state
    if (card1_int_ < 0 || card2_int_ < 0) {
        return "InvalidPrivateCards";
    }
    // Convert ints back to strings for representation
    // Note: This might not capture suitedness directly unless inferred.
    // The original implementation returned card strings concatenated,
    // e.g., "AsKs". Let's replicate that.
    return Card::IntToString(card1_int_) + Card::IntToString(card2_int_);
}

// --- Operators ---

bool PrivateCards::operator==(const PrivateCards& other) const {
    // Equality depends only on the cards, not the weight.
    return card1_int_ == other.card1_int_ && card2_int_ == other.card2_int_;
}

bool PrivateCards::operator!=(const PrivateCards& other) const {
    return !(*this == other);
}

bool PrivateCards::operator<(const PrivateCards& other) const {
    // Order primarily by the first card, then by the second.
    if (card1_int_ != other.card1_int_) {
        return card1_int_ < other.card1_int_;
    }
    return card2_int_ < other.card2_int_;
}


} // namespace core
} // namespace poker_solver


// --- Hash Function Specialization Implementation ---

namespace std {
std::size_t hash<poker_solver::core::PrivateCards>::operator()(
    const poker_solver::core::PrivateCards& pc) const noexcept {
    // Combine the hashes of the two card integers.
    // Start with hash of the first card.
    std::size_t seed = std::hash<int>{}(pc.Card1Int());
    // Combine with hash of the second card.
    poker_solver::core::hash_combine(seed, pc.Card2Int());
    return seed;
    // Alternative simple combination (less robust against collisions):
    // return std::hash<int>{}(pc.Card1Int()) ^
    //        (std::hash<int>{}(pc.Card2Int()) << 1);
}
} // namespace std
