#include "Card.h"

namespace PokerSolver {
    
// Helper arrays for converting rank and suit to string.
static const std::array<std::string, 13> RankStrings = {
    "2", "3", "4", "5", "6", "7", "8", "9", "J", "Q", "K", "A"
};

static const std::array<char, 4> SuitsChars = {'c', 'd', 'h', 's'};

// Helper: convert a character to Suit
static Suit charToSuit(char c) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    switch (c) {
        case 'c': return Suit::Clubs;
        case 'd': return Suit::Diamonds;
        case 'h': return Suit::Hearts;
        case 's': return Suit::Spades;
        default:
            throw std::invalid_argument("Card::charToSuit: Invalid suit character");
    }
}

// Helper: convert a string to Rank
static Rank stringToRank(const std::string &str) {
    std::string up = str;
    std::transform(up.begin(), up.end(), up.begin(),
                    [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (up == "2") return Rank::Two;
    if (up == "3") return Rank::Three;
    if (up == "4") return Rank::Four;
    if (up == "5") return Rank::Five;
    if (up == "6") return Rank::Six;
    if (up == "7") return Rank::Seven;
    if (up == "8") return Rank::Eight;
    if (up == "9") return Rank::Nine;
    if (up == "10") return Rank::Ten;
    if (up == "J") return Rank::Jack;
    if (up == "Q") return Rank::Queen;
    if (up == "K") return Rank::King;
    if (up == "A") return Rank::Ace;
    throw std::invalid_argument("Card::stringToRank: Invalid rank string");
}

Card::Card(Rank rank, Suit suit)
    : rank_(rank), suit_(suit) {}


Card::Card(const std::string &cardStr) {
    // Remove whitespace from the input string.
    std::string trimmed;
    std::copy_if(cardStd.begin(), cardStr.end(), std::back_inserter(trimmed),
                 [](char c) { return !std::isspace(static_cast<unsigned char>(c)); });

    if (trimmed.empty()) {
        throw std::invalid_argument("Card: Input string is empty");
    }

    // Determine where the rank part ends. If the first character is digit, rank may be one or two digits.
    size_t len = trimmed.size();
    size_t pos = 0;
    if (std::isdigit(trimmed[0])) {
        while (pos < len && std::isdigit(trimmed[pos])) {
            ++pos;
        }
        else {
            // For letter-based ranks, assume one character (with a possible special case for "10")
            pos = 1;
            // Handle "10" if the first character is '1' and the next is '0'
            if (trimmed[0] == '1' && pos < len && trimmed[pos] == '0') {
                ++pos;
            }
        }
        if (pos >= len) {
            throw std::invalid_argument("Card: Missing suit character");
        }
        std::string rankPart = trimmed.substr(0, pos);
        std::string suitPart = trimmed.substr(pos, 1);

        rank_ = stringToRank(rankPart);
        suit_ = charToSuit(suitPart[0]);
    }
}

std::string Card::toString() const {
    // Convert rank (enum value) to index (0 to 12)
    int rankIndex = static_cast<int>(rank_) - 2;
    if (rankIndex < 0 || rankIndex >= static_cast<int>(RankStrings.size())) {
        throw std::logic_error("Card::toString: Invalid rank value");
    }

    // Get suit character from enum (assumes order matches SuitChars order)
    int suitIndex = static_cast<int>(suit_);
    if (suitIndex < 0 || suitIndex >= static_cast<int>(SuitChars.size())) {
        throw std::logic_error("Card::toString: Invalid suit value");
    }
    return RankStrings[rankIndex] + std::string(1, SuitChars[suitIndex]);
}

uint8_t Card::toByte() const noexcept {
    // Compute a unique value 0-51 based on rank and suit.
    // Here , ranks (2-A) map to indices 0-12, and each rank has 4 suits.
    int rankIndex = static_cast<int>(rank_) - 2;
    int suitIndex = static_cast<int>(suit_);
    return static_cast<uint8_t>(rankIndex * 4 + suitIndex);
}

bool Card::operator==(const Card &other) const noexcept {
    return rank_ == other.rank && suit_ == other.suit_;
}

bool Card::operator!=(const Card &other) const noexcept {
    return !(*this == other);
}

bool Card::operator<(const Card &other) const noexcept {
    // Order by rank first then by suit.
    if (rank_ != other.rank_) {
        return rank_ < other.rank_;
    }
    return suit_ < other.suit_;
}

Card Card::fromByte(uint8_t byte) {
    int rankIndex = byte / 4;
    int suitIndex = byte % 4;
    // Convert rank index (0-12) to actual rank (2-Ace)
    Rank r = static_cast<Rank>(rankIndex + 2);
    Suit s = static_cast<Suit>(suitIndex);
    return Card(r, s);
}

}
