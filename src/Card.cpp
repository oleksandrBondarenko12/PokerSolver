#include "Card.h"
#include <stdexcept> // Include for exceptions
#include <array>     // Include for std::array
#include <sstream>   // Include for std::ostringstream
#include <algorithm> // Include for std::transform, std::copy_if
#include <cctype>    // Include for std::tolower, std::isdigit, std::isspace
#include <string>    // Include for std::string operations

namespace PokerSolver {

// Helper arrays for converting rank and suit to string.
static const std::array<std::string, 13> RankStrings = {
    "2", "3", "4", "5", "6", "7", "8", "9", "T", "J", "Q", "K", "A" // Use 'T' for Ten
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
            throw std::invalid_argument("Card::charToSuit: Invalid suit character: " + std::string(1, c));
    }
}

// Helper: convert a string to Rank
static Rank stringToRank(const std::string &str) {
    std::string up = str;
    // Convert to uppercase for case-insensitive comparison
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
    if (up == "T" || up == "10") return Rank::Ten; // Allow 'T' or '10'
    if (up == "J") return Rank::Jack;
    if (up == "Q") return Rank::Queen;
    if (up == "K") return Rank::King;
    if (up == "A") return Rank::Ace;

    throw std::invalid_argument("Card::stringToRank: Invalid rank string: " + str);
}

// Constructor from rank and suit
Card::Card(Rank rank, Suit suit)
    : rank_(rank), suit_(suit) {}

// Constructor from string representation (e.g., "Ah", "10c", "Td")
Card::Card(const std::string &cardStr) {
    // Remove whitespace from the input string.
    std::string trimmed;
    std::copy_if(cardStr.begin(), cardStr.end(), std::back_inserter(trimmed),
                 [](char c) { return !std::isspace(static_cast<unsigned char>(c)); });

    if (trimmed.empty()) {
        throw std::invalid_argument("Card::Card(string): Input string is empty");
    }

    size_t len = trimmed.size();
    if (len < 2) {
         throw std::invalid_argument("Card::Card(string): Input string too short: " + trimmed);
    }

    // The last character is always the suit.
    char suitChar = trimmed.back();
    // The part before the last character is the rank.
    std::string rankPart = trimmed.substr(0, len - 1);

    // Validate and assign
    rank_ = stringToRank(rankPart); // Use helper to parse rank
    suit_ = charToSuit(suitChar);   // Use helper to parse suit

    // --- NOTE: The previous logic for parsing based on std::isdigit was removed ---
    // --- as the simpler substr logic above handles "10" and single chars correctly ---
    // --- and avoids the structural error that caused the compilation failure. ---
    // --- If you specifically need the digit-checking logic, ensure braces are correct: ---
    /*
    size_t pos = 0;
    if (std::isdigit(trimmed[0])) {
        while (pos < len && std::isdigit(trimmed[pos])) {
            ++pos;
        }
    } // <<< BRACE WAS MISSING HERE
    else {
        pos = 1;
        // Handle "10" specifically if needed, though substr approach is cleaner
        if (trimmed[0] == '1' && pos < len && trimmed[pos] == '0') {
             ++pos;
        }
    }
    if (pos >= len) { // Check if suit is missing after rank parsing
        throw std::invalid_argument("Card::Card(string): Missing suit character after rank");
    }
    std::string rankPart = trimmed.substr(0, pos);
    std::string suitPart = trimmed.substr(pos); // Suit is the rest of the string
     if (suitPart.length() != 1) {
         throw std::invalid_argument("Card::Card(string): Invalid suit format: " + suitPart);
     }
    rank_ = stringToRank(rankPart);
    suit_ = charToSuit(suitPart[0]);
    */

} // <<< BRACE WAS MISSING HERE for the constructor


// Convert the card to a string (e.g. "Ah", "Tc")
std::string Card::toString() const {
    int rankIndex = static_cast<int>(rank_) - static_cast<int>(Rank::Two);
    if (rankIndex < 0 || rankIndex >= static_cast<int>(RankStrings.size())) {
        throw std::logic_error("Card::toString: Invalid internal rank value");
    }
    int suitIndex = static_cast<int>(suit_);
    if (suitIndex < 0 || suitIndex >= static_cast<int>(SuitsChars.size())) {
        throw std::logic_error("Card::toString: Invalid internal suit value");
    }
    return RankStrings[rankIndex] + std::string(1, SuitsChars[suitIndex]);
}

// Return a unique byte representation (0-51)
uint8_t Card::toByte() const noexcept {
    int rankIndex = static_cast<int>(rank_) - static_cast<int>(Rank::Two);
    int suitIndex = static_cast<int>(suit_);
    return static_cast<uint8_t>(rankIndex * 4 + suitIndex);
}

// Create a Card from a byte representation.
Card Card::fromByte(uint8_t byte) {
    if (byte > 51) {
         throw std::out_of_range("Card::fromByte: Input byte " + std::to_string(byte) + " is out of range (0-51).");
    }
    int rankIndex = byte / 4;
    int suitIndex = byte % 4;
    Rank r = static_cast<Rank>(rankIndex + static_cast<int>(Rank::Two));
    Suit s = static_cast<Suit>(suitIndex);
    return Card(r, s);
}


// Comparison operators
bool Card::operator==(const Card &other) const noexcept {
    return rank_ == other.rank_ && suit_ == other.suit_;
}

bool Card::operator!=(const Card &other) const noexcept {
    return !(*this == other);
}

bool Card::operator<(const Card &other) const noexcept {
    if (rank_ != other.rank_) {
        return rank_ < other.rank_;
    }
    return suit_ < other.suit_;
}

} // namespace PokerSolver
