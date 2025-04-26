#include "compairer/Dic5Compairer.h"
#include "tools/lookup8.h" // Provides PokerSolverUtils::hash1
#include "Library.h"       // Provides PokerSolverUtils::Combinations, ::string_split

#include <fstream>        // For std::ifstream
#include <sstream>        // For std::istringstream
#include <stdexcept>      // For exceptions
#include <algorithm>      // For std::sort, std::remove_if, std::min
#include <vector>
#include <limits>         // For std::numeric_limits
#include <iostream>       // For std::cerr, std::cout
#include <cctype>         // For std::isspace
#include <iomanip>        // For std::hex for printing keys

namespace PokerSolver {

// --- Constructor ---
Dic5Compairer::Dic5Compairer(const std::string& resourceFilePath) {
    loadResourceFile(resourceFilePath);
}

// --- Load Resource File ---
void Dic5Compairer::loadResourceFile(const std::string& resourceFilePath) {
    std::ifstream infile(resourceFilePath);
    if (!infile) {
        throw std::runtime_error("Dic5Compairer::loadResourceFile: Could not open resource file: " + resourceFilePath);
    }
    lookupTable_.clear();
    std::string line;
    int lineNumber = 0;
    // --- DEBUG --- Add a flag to check if we found the specific hand during load
    // bool foundTargetHandLoad = false; // Keep commented unless debugging again
    // --- /DEBUG ---
    while (std::getline(infile, line)) {
        lineNumber++;
        line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](unsigned char ch) { return !std::isspace(ch); }));
        line.erase(std::find_if(line.rbegin(), line.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), line.end());
        if (line.empty() || line[0] == '#') continue;
        size_t commaPos = line.find(',');
        if (commaPos == std::string::npos) { /* warning */ continue; }
        std::string handStr = line.substr(0, commaPos);
        std::string rankStr = line.substr(commaPos + 1);
        handStr.erase(std::remove_if(handStr.begin(), handStr.end(), ::isspace), handStr.end());
        rankStr.erase(std::remove_if(rankStr.begin(), rankStr.end(), ::isspace), rankStr.end());
        int ordinalRank;
        try { ordinalRank = std::stoi(rankStr); } catch (const std::exception& e) { /* warning */ continue; }
        std::vector<std::string> cardStrings = PokerSolverUtils::string_split(handStr, '-');
        if (cardStrings.size() != 5) { /* warning */ continue; }
        std::vector<Card> hand; hand.reserve(5); bool parseError = false;
        for (const auto& cardStr : cardStrings) { try { hand.emplace_back(cardStr); } catch (const std::exception& e) { /* warning */ parseError = true; break; } }
        if (parseError) continue;
        std::sort(hand.begin(), hand.end());
        uint64_t key = computeHandKey(hand); // Call computeHandKey *after* sorting

        // --- DEBUG PRINT 1: Check during loading (Commented Out) ---
        // if (ordinalRank == 4117 || ordinalRank == 1328041) {
        //      std::cout << "DEBUG Load: Found Rank " << ordinalRank << " at Line " << lineNumber << ", HandStr: " << handStr
        //                << ", Sorted Cards: ";
        //      for(const auto& c : hand) std::cout << c.toString();
        //      std::cout << ", Key Generated: 0x" << std::hex << key << std::dec
        //                << ", Rank Read: " << ordinalRank << std::endl;
        //      if (ordinalRank == 4117) foundTargetHandLoad = true;
        // }
        // --- END DEBUG PRINT 1 ---

        lookupTable_.emplace(key, ordinalRank);
    } // End while loop

    // --- DEBUG --- Check if we ever found the target rank during load (Commented Out) ---
    // if (!foundTargetHandLoad) {
    //     std::cout << "DEBUG Load: Did NOT find rank 4117 while loading the file." << std::endl;
    // }
     // --- /DEBUG ---

    if (lookupTable_.empty()) { throw std::runtime_error("Dic5Compairer::loadResourceFile: Lookup table is empty after processing file: " + resourceFilePath); }
    std::cout << "Dic5Compairer: Loaded " << lookupTable_.size() << " 5-card hand ranks from " << resourceFilePath << std::endl;
}


// --- Compute Hand Key ---
uint64_t Dic5Compairer::computeHandKey(const std::vector<Card>& sortedHand) const {
    if (sortedHand.size() != 5) { throw std::logic_error("Dic5Compairer::computeHandKey: Internal error - requires exactly 5 sorted cards."); }
    std::vector<uint8_t> bytes; bytes.reserve(5);

    // --- DEBUG PRINT 2: Check key generation input/output (Commented Out) ---
    // std::string debugHandStr;
    // std::cout << "DEBUG computeHandKey Input: ";
    // for(const auto& card : sortedHand) {
    //     bytes.push_back(card.toByte());
    //     debugHandStr += card.toString();
    //     std::cout << card.toString() << "(" << (int)card.toByte() << ") ";
    // }
    // std::cout << " Bytes: ";
    // for(const auto& b : bytes) std::cout << (int)b << " ";
    // --- END DEBUG PRINT 2 ---

    // Non-debug version:
     for(const auto& card : sortedHand) { bytes.push_back(card.toByte()); }

    uint64_t key = PokerSolverUtils::hash1(bytes.data(), bytes.size(), 0);

    // --- DEBUG PRINT 3: Check key generation output (Commented Out) ---
    // std::cout << "DEBUG computeHandKey Hand: " << debugHandStr << " Output Key: 0x" << std::hex << key << std::dec << std::endl;
    // --- END DEBUG PRINT 3 ---
    return key;
}

// --- Get Rank (7-card implementation) ---
int Dic5Compairer::getRank(const std::vector<Card>& privateHand,
                           const std::vector<Card>& board) const {
    if (privateHand.size() != 2) { throw std::invalid_argument("Dic5Compairer::getRank: Private hand must contain exactly 2 cards."); }
    if (board.size() > 5) { throw std::invalid_argument("Dic5Compairer::getRank: Board cannot contain more than 5 cards. Received: " + std::to_string(board.size())); }
    if (privateHand.size() + board.size() < 5) { throw std::invalid_argument("Dic5Compairer::getRank: Need at least 5 cards total to form a hand."); }

    std::vector<Card> availableCards = privateHand;
    availableCards.insert(availableCards.end(), board.begin(), board.end());

    PokerSolverUtils::Combinations<Card> combos(availableCards, 5);
    int bestRank = std::numeric_limits<int>::max();
    bool combinationFound = false;
    // uint64_t bestHandKey = 0; // No longer needed without debug print

    while (!combos.done()) {
        std::vector<Card> fiveCardHand = combos.current();
        combinationFound = true;
        std::sort(fiveCardHand.begin(), fiveCardHand.end());
        uint64_t key = computeHandKey(fiveCardHand);
        auto it = lookupTable_.find(key);
        if (it == lookupTable_.end()) {
             std::string handStr; for(size_t i = 0; i < fiveCardHand.size(); ++i) { handStr += fiveCardHand[i].toString() + (i < fiveCardHand.size() - 1 ? "-" : ""); }
             std::cerr << "ERROR: 5-card hand key not found in lookup table for sub-hand: " << handStr << " (Key: 0x" << std::hex << key << std::dec << ")" << std::endl;
             throw std::runtime_error("Dic5Compairer::getRank: 5-card hand key not found in lookup table.");
        }
        if (it->second < bestRank) {
             bestRank = it->second;
             // bestHandKey = key; // No longer needed
        }
        combos.next();
    }
    if (!combinationFound) { throw std::logic_error("Dic5Compairer::getRank: No 5-card combinations could be generated."); }

    // --- DEBUG PRINT 4: Check result of 7-card evaluation (Commented Out) ---
    // if (bestRank == 1328041 || bestRank == 4117) {
    //     std::cout << "DEBUG 7-Card Eval: Input Private: " << privateHand[0].toString() << privateHand[1].toString() << " Board: ";
    //     for(const auto& c : board) std::cout << c.toString();
    //     std::cout << " Best 5-Card Key Found: 0x" << std::hex << bestHandKey << std::dec
    //               << " Best Rank Found: " << bestRank << std::endl;
    // }
    // --- END DEBUG PRINT 4 ---

    return bestRank;
}


// --- Compare (implementation) ---
Compairer::ComparisonResult Dic5Compairer::compare(const std::vector<Card>& privateHand1,
                                                   const std::vector<Card>& privateHand2,
                                                   const std::vector<Card>& board) const {
     if (privateHand1.size() != 2 || privateHand2.size() != 2) { throw std::invalid_argument("Dic5Compairer::compare: Private hands must contain exactly 2 cards."); }

    int rank1 = getRank(privateHand1, board);
    int rank2 = getRank(privateHand2, board);

    if (rank1 < rank2) { return ComparisonResult::LARGER; }
    else if (rank1 > rank2) { return ComparisonResult::SMALLER; }
    else { return ComparisonResult::EQUAL; }
}

// --- Get Rank (Direct 5-card version) ---
int Dic5Compairer::getRank(const std::vector<Card>& hand) const {
    if (hand.size() == 5) {
        std::vector<Card> sortedHand = hand;
        std::sort(sortedHand.begin(), sortedHand.end());
        uint64_t key = computeHandKey(sortedHand);
        auto it = lookupTable_.find(key);
        if (it == lookupTable_.end()) {
            std::string handStr; for(size_t i = 0; i < sortedHand.size(); ++i) { handStr += sortedHand[i].toString() + (i < sortedHand.size() - 1 ? "-" : ""); }
            std::cerr << "ERROR: 5-card hand key not found in lookup table for hand: " << handStr << " (Key: 0x" << std::hex << key << std::dec << ")" << std::endl;
            throw std::runtime_error("Dic5Compairer::getRank(5 cards): Hand key not found in lookup table.");
        }
        // --- DEBUG PRINT 5: Check result of direct 5-card lookup (Commented Out) ---
        // if (it->second == 1328041 || it->second == 4117) {
        //     std::string handStr; for(size_t i = 0; i < sortedHand.size(); ++i) { handStr += sortedHand[i].toString() + (i < sortedHand.size() - 1 ? "-" : ""); }
        //     std::cout << "DEBUG 5-Card Direct Lookup: Hand: " << handStr
        //               << " Key: 0x" << std::hex << key << std::dec
        //               << " Rank Found: " << it->second << std::endl;
        // }
        // --- END DEBUG PRINT 5 ---
        return it->second;
    } else {
         throw std::invalid_argument("Dic5Compairer::getRank(vector<Card>): This overload requires exactly 5 cards. Use getRank(privateHand, board) for evaluation based on the Compairer interface.");
    }
}

} // namespace PokerSolver
