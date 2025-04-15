#include "compairer/Dic5Compairer.h"
#include "tools/lookup8.h" // Provides PokerSolverUtils::computeLookupKey(...)
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace PokerSolver {

Dic5Compairer::Dic5Compairer(const std::string& resourceFilePath) {
    loadResourceFile(resourceFilePath);
}

void Dic5Compairer::loadResourceFile(const std::string& resourceFilePath) {
    std::ifstream infile(resourceFilePath);
    if (!infile) {
        throw std::runtime_error("Dic5Compairer::loadResourceFile: Could not open resource file: " + resourceFilePath);
    }
    
    std::string line;
    while (std::getline(infile, line)) {
        if (line.empty())
            continue;

        // Each line has the format: card1-card2-card3-card4-card5, ordinalRank
        std::istringstream iss(line);
        std::string handStr;
        if (!std::getline(iss, handStr, ',')) {
            continue; // Skip malformed line.
        }
        std::string rankStr;
        if (!std::getline(iss, rankStr)) {
            continue; // Skip if no rank value.
        }
        int ordinalRank = std::stoi(rankStr);

        // Remove potential whitespace from the hand string.
        handStr.erase(std::remove_if(handStr.begin(), handStr.end(),
                                     [](unsigned char c){ return std::isspace(c); }),
                      handStr.end());

        // Parse the hand string into individual card strings separated by hyphens.
        std::vector<Card> hand;
        size_t pos = 0;
        while (pos < handStr.size()) {
            size_t nextDash = handStr.find('-', pos);
            std::string cardStr;
            if (nextDash == std::string::npos) {
                cardStr = handStr.substr(pos);
                pos = handStr.size();
            } else {
                cardStr = handStr.substr(pos, nextDash - pos);
                pos = nextDash + 1;
            }
            // Construct a Card using its string constructor.
            hand.push_back(Card(cardStr));
        }
        if (hand.size() != 5) {
            throw std::logic_error("Dic5Compairer::loadResourceFile: Expected 5 cards per hand.");
        }
        // Ensure canonical order.
        std::sort(hand.begin(), hand.end());
        uint64_t key = computeHandKey(hand);
        lookupTable_[key] = ordinalRank;
    }
    infile.close();
}

uint64_t Dic5Compairer::computeHandKey(const std::vector<Card>& sortedHand) const {
    // Build a vector of bytes from the sorted hand.
    std::vector<uint8_t> bytes;
    bytes.reserve(5);
    for (const auto& card : sortedHand) {
        bytes.push_back(card.toByte());
    }
    // Use the lookup8 utility to compute a 64-bit key.
    return PokerSolverUtils::computeLookupKey(bytes);
}

int Dic5Compairer::getRank(const std::vector<Card>& hand) const {
    if (hand.size() != 5) {
        throw std::invalid_argument("Dic5Compairer::getRank: Hand must contain exactly 5 cards.");
    }
    // Copy and sort to obtain canonical order.
    std::vector<Card> sortedHand = hand;
    std::sort(sortedHand.begin(), sortedHand.end());
    uint64_t key = computeHandKey(sortedHand);
    auto it = lookupTable_.find(key);
    if (it == lookupTable_.end()) {
        throw std::runtime_error("Dic5Compairer::getRank: Hand not found in lookup table.");
    }
    return it->second;
}

int Dic5Compairer::compare(const std::vector<Card>& hand1, const std::vector<Card>& hand2) const {
    int rank1 = getRank(hand1);
    int rank2 = getRank(hand2);
    // Lower ordinal rank means a stronger hand.
    if (rank1 < rank2)
        return -1;
    else if (rank1 > rank2)
        return 1;
    else
        return 0;
}

} // namespace PokerSolver
