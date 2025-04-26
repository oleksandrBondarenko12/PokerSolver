#ifndef POKERSOLVER_RIVERRANGEMANAGER_H
#define POKERSOLVER_RIVERRANGEMANAGER_H

#include "RiverCombs.h"         // Definition of RiverCombs
#include "PrivateCards.h"       // Definition of PrivateCards
#include "Card.h"               // Definition of Card
#include "compairer/Compairer.h" // Interface for hand evaluation

#include <vector>
#include <unordered_map>
#include <memory>               // For std::shared_ptr
#include <mutex>                // For std::mutex
#include <cstdint>              // For uint64_t

namespace PokerSolver {

/**
 * @brief Manages the computation and caching of ranked hand combinations at the river.
 *
 * This class takes a player's range (vector of PrivateCards) and a specific river board,
 * evaluates each valid hand combination using a Compairer, ranks them, and caches
 * the results. It ensures thread safety for concurrent access.
 */
class RiverRangeManager {
public:
    /**
     * @brief Default constructor. Initializes an empty manager without a hand evaluator.
     * A hand evaluator must be set later or provided via the other constructor.
     */
    RiverRangeManager() = default;

    /**
     * @brief Constructs a RiverRangeManager with a specific hand evaluator.
     * @param handEvaluator A shared pointer to an object implementing the Compairer interface.
     */
    explicit RiverRangeManager(std::shared_ptr<Compairer> handEvaluator);

    /**
     * @brief Retrieves the ranked river combinations for a given player, range, and board.
     *
     * If the combinations for this specific board have been computed before, a cached result
     * is returned. Otherwise, it computes the ranks, sorts the combinations, caches the result,
     * and then returns it.
     *
     * @param player The index of the player (e.g., 0 for OOP, 1 for IP).
     * @param range The vector of PrivateCards representing the player's hand range reaching the river.
     * @param board The vector of Card objects representing the 5 community cards on the river.
     * @return const std::vector<RiverCombs>& A const reference to the vector of ranked RiverCombs.
     * The vector is sorted by rank (strongest first, assuming higher rank is better).
     * @throws std::runtime_error if the hand evaluator is not set.
     * @throws std::out_of_range if the player index is invalid.
     */
    const std::vector<RiverCombs>& getRiverCombos(int player,
                                                  const std::vector<PrivateCards>& range,
                                                  const std::vector<Card>& board);

    /**
     * @brief Sets the hand evaluator to be used for ranking.
     * @param handEvaluator A shared pointer to an object implementing the Compairer interface.
     */
    void setHandEvaluator(std::shared_ptr<Compairer> handEvaluator);

private:
    /**
     * @brief Computes a 64-bit bitmask representation of the board cards.
     * @param board A vector of Card objects representing the board.
     * @return uint64_t A bitmask where each card corresponds to a set bit.
     */
    uint64_t getBoardMask(const std::vector<Card>& board) const;

    // Cache for player 0's river ranges, keyed by board mask.
    std::unordered_map<uint64_t, std::vector<RiverCombs>> p1RiverRanges_;
    // Cache for player 1's river ranges, keyed by board mask.
    std::unordered_map<uint64_t, std::vector<RiverCombs>> p2RiverRanges_;

    // The hand evaluator used to rank combinations.
    std::shared_ptr<Compairer> handEvaluator_;
    // Mutex to protect concurrent access to the range maps.
    std::mutex maplock_;
};

} // namespace PokerSolver

#endif // POKERSOLVER_RIVERRANGEMANAGER_H
