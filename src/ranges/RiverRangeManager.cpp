#include "ranges/RiverRangeManager.h"
#include <stdexcept>      // For exceptions
#include <algorithm>      // For std::sort
#include <utility>        // For std::move

namespace PokerSolver {

// Constructor implementation
RiverRangeManager::RiverRangeManager(std::shared_ptr<Compairer> handEvaluator)
    : handEvaluator_(std::move(handEvaluator)) // Initialize handEvaluator_ using std::move
{
    // The mutex maplock_ is default-constructed.
}

// Setter for the hand evaluator
void RiverRangeManager::setHandEvaluator(std::shared_ptr<Compairer> handEvaluator) {
    handEvaluator_ = std::move(handEvaluator);
}

// Helper function to create a bitmask from board cards
uint64_t RiverRangeManager::getBoardMask(const std::vector<Card>& board) const {
    uint64_t mask = 0ULL;
    for (const auto& card : board) {
        mask |= (1ULL << card.toByte()); // Set the bit corresponding to the card's byte value
    }
    return mask;
}

// Core function to get or compute river combos
const std::vector<RiverCombs>& RiverRangeManager::getRiverCombos(int player,
                                                                 const std::vector<PrivateCards>& range,
                                                                 const std::vector<Card>& board) {
    // Ensure a hand evaluator has been set
    if (!handEvaluator_) {
        throw std::runtime_error("RiverRangeManager::getRiverCombos: Hand evaluator is not set.");
    }

    // Select the appropriate cache map based on the player index
    std::unordered_map<uint64_t, std::vector<RiverCombs>>* riverRanges;
    if (player == 0) {
        riverRanges = &p1RiverRanges_;
    } else if (player == 1) {
        riverRanges = &p2RiverRanges_;
    } else {
        throw std::out_of_range("RiverRangeManager::getRiverCombos: Invalid player index: " + std::to_string(player));
    }

    // Compute the board mask to use as the cache key
    uint64_t boardMask = getBoardMask(board);
    uint64_t key = boardMask; // Use the mask directly as the key

    // --- Cache Lookup ---
    { // Scope for the lock guard
        std::lock_guard<std::mutex> lock(maplock_); // Lock the mutex for thread-safe map access
        auto it = riverRanges->find(key);
        if (it != riverRanges->end()) {
            // Cache hit: return the existing vector
            return it->second;
        }
    } // Mutex is automatically unlocked here by lock_guard destructor

    // --- Cache Miss: Compute the RiverCombs ---

    // 1. Count valid combos (those not conflicting with the board)
    int validComboCount = 0;
    for (const auto& hand : range) {
        if ((hand.toBoardMask() & boardMask) == 0ULL) { // Check for overlap using bitwise AND
            validComboCount++;
        }
    }

    // 2. Create and populate the result vector
    std::vector<RiverCombs> computedCombos;
    computedCombos.reserve(validComboCount); // Reserve space for efficiency

    // Convert board to vector<int> once if needed by RiverCombs constructor (assuming it needs ints)
    // If RiverCombs can take vector<Card>, adapt this.
    std::vector<int> boardInts;
    boardInts.reserve(board.size());
    for(const auto& card : board) {
        boardInts.push_back(static_cast<int>(card.toByte()));
    }


    for (size_t i = 0; i < range.size(); ++i) {
        const auto& hand = range[i];
        uint64_t handMask = hand.toBoardMask();

        // Skip if hand conflicts with the board
        if ((handMask & boardMask) != 0ULL) {
            continue;
        }

        // Combine private hand and board for evaluation
        std::vector<Card> fullHand = {hand.card1(), hand.card2()};
        fullHand.insert(fullHand.end(), board.begin(), board.end());

        // Get the rank using the Compairer
        // Assuming getRank takes the private hand and the board separately
        int rank = handEvaluator_->getRank({hand.card1(), hand.card2()}, board);

        // Create the RiverCombs object
        // Pass the original index 'i' as the reach probability index.
        computedCombos.emplace_back(boardInts, hand, rank, static_cast<int>(i));
    }

    // 3. Sort the computed combos by rank (strongest first)
    // ASSUMPTION: Higher rank value means a stronger hand. Adjust if necessary.
    std::sort(computedCombos.begin(), computedCombos.end(),
              [](const RiverCombs& a, const RiverCombs& b) {
                  return a.rank() > b.rank(); // Sort descending by rank
              });

    // --- Cache Update ---
    { // Scope for the lock guard
        std::lock_guard<std::mutex> lock(maplock_); // Lock again for thread-safe insertion
        // Use std::move to efficiently insert the computed vector into the map
        auto result_pair = riverRanges->emplace(key, std::move(computedCombos));
        // Return a reference to the newly inserted vector
        return result_pair.first->second; // result_pair.first is an iterator to the inserted element
    } // Mutex is automatically unlocked here
}

} // namespace PokerSolver
