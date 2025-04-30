#include "Card.h" // Adjust path if necessary

#include <stdexcept>
#include <sstream>   // For error message formatting
#include <algorithm> // For std::find
#include <cctype>    // For std::tolower, std::toupper

namespace poker_solver {
namespace core {

// --- Constants for Ranks and Suits ---
const std::array<char, kNumRanks> kRankChars = {
    '2', '3', '4', '5', '6', '7', '8', '9', 'T', 'J', 'Q', 'K', 'A'
};
const std::array<char, kNumSuits> kSuitChars = {'c', 'd', 'h', 's'};

const std::array<char, kNumSuits>& Card::GetAllSuitChars() {
    return kSuitChars;
}
const std::array<char, kNumRanks>& Card::GetAllRankChars() {
    return kRankChars;
}

// --- Helper Functions ---

bool Card::IsValidCardInt(int card_int) {
    return card_int >= 0 && card_int < kNumCardsInDeck;
}

char Card::SuitIndexToChar(int suit_index) {
    if (suit_index >= 0 && suit_index < kNumSuits) {
        return kSuitChars[suit_index];
    }
    return '?'; // Indicate error
}

char Card::RankIndexToChar(int rank_index) {
    if (rank_index >= 0 && rank_index < kNumRanks) {
        return kRankChars[rank_index];
    }
    return '?'; // Indicate error
}

int Card::SuitCharToIndex(char suit_char) {
    char lower_suit = std::tolower(suit_char);
    for (int i = 0; i < kNumSuits; ++i) {
        if (lower_suit == kSuitChars[i]) {
            return i;
        }
    }
    return -1; // Indicate error
}

int Card::RankCharToIndex(char rank_char) {
    char upper_rank = std::toupper(rank_char);
     for (int i = 0; i < kNumRanks; ++i) {
        if (upper_rank == kRankChars[i]) {
            return i;
        }
    }
    return -1; // Indicate error
}


// --- Constructors ---

Card::Card(int card_int) {
    if (!IsValidCardInt(card_int)) {
        std::ostringstream oss;
        oss << "Invalid card integer: " << card_int << ". Must be 0-"
            << (kNumCardsInDeck - 1) << ".";
        throw std::out_of_range(oss.str());
    }
    card_int_ = card_int;
}

Card::Card(std::string_view card_str) {
    card_int_ = StringToInt(card_str);
    if (!card_int_.has_value()) {
         std::ostringstream oss;
         oss << "Invalid card string format: \"" << card_str << "\". Expected format like 'As', 'Td', '2c'.";
        throw std::invalid_argument(oss.str());
    }
}

// --- Accessors ---

std::string Card::ToString() const {
    if (!card_int_.has_value()) {
        return "Empty";
    }
    return IntToString(card_int_.value());
}

// --- Comparison Operators ---

bool Card::operator==(const Card& other) const {
    // Two cards are equal if their internal optional<int> representations are equal.
    // This correctly handles two empty cards being equal, and an empty card
    // not being equal to a valid card.
    return card_int_ == other.card_int_;
}

bool Card::operator!=(const Card& other) const {
    return !(*this == other);
}


// --- Static Conversion Utilities ---

std::optional<int> Card::StringToInt(std::string_view card_str) {
    if (card_str.length() != 2) {
        return std::nullopt;
    }
    int rank_index = RankCharToIndex(card_str[0]);
    int suit_index = SuitCharToIndex(card_str[1]);
    if (rank_index == -1 || suit_index == -1) {
        return std::nullopt;
    }
    return rank_index * kNumSuits + suit_index;
}

std::string Card::IntToString(int card_int) {
    if (!IsValidCardInt(card_int)) {
        return "Invalid";
    }
    int rank_index = card_int / kNumSuits;
    int suit_index = card_int % kNumSuits;
    std::string s;
    s += RankIndexToChar(rank_index);
    s += SuitIndexToChar(suit_index);
    return s;
}

// --- Static Bitmask Utilities ---

uint64_t Card::CardIntsToUint64(const std::vector<int>& card_ints) {
    uint64_t board_mask = 0;
    for (int card_int : card_ints) {
        board_mask |= CardIntToUint64(card_int);
    }
    return board_mask;
}

uint64_t Card::CardsToUint64(const std::vector<Card>& cards) {
    uint64_t board_mask = 0;
    for (const auto& card : cards) {
        board_mask |= CardToUint64(card);
    }
    return board_mask;
}

uint64_t Card::CardIntToUint64(int card_int) {
    if (!IsValidCardInt(card_int)) {
        std::ostringstream oss;
        oss << "Invalid card integer for bitmask: " << card_int;
        throw std::out_of_range(oss.str());
    }
    return static_cast<uint64_t>(1) << card_int;
}

uint64_t Card::CardToUint64(const Card& card) {
    if (card.IsEmpty()) {
        return 0;
    }
    return static_cast<uint64_t>(1) << card.card_int().value();
}

std::vector<int> Card::Uint64ToCardInts(uint64_t board_mask) {
    std::vector<int> card_ints;
    card_ints.reserve(7);
    for (int i = 0; i < kNumCardsInDeck; ++i) {
        if ((board_mask >> i) & 1) {
            card_ints.push_back(i);
        }
    }
    return card_ints;
}

std::vector<Card> Card::Uint64ToCards(uint64_t board_mask) {
    std::vector<Card> cards;
    cards.reserve(7);
    std::vector<int> card_ints = Uint64ToCardInts(board_mask);
    for (int card_int : card_ints) {
        cards.emplace_back(card_int);
    }
    return cards;
}

bool Card::DoBoardsOverlap(uint64_t board_mask1, uint64_t board_mask2) {
    return (board_mask1 & board_mask2) != 0;
}


} // namespace core
} // namespace poker_solver
