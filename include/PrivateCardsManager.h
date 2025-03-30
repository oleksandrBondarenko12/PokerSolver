#ifndef POKERSOLVER_PRIVATECARDSMANAGER_H
#define POKERSOLVER_PRIVATECARDSMANAGER_H

#include "PrivateCards.h"
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <numeric>

namespace PokerSolver {

class PrivateCardsManager {
public:
    // Default constructor (empty manager)
    PrivateCardsManager();

    // Constructor that takes player ranges, number of players, and the initial board mask.
    PrivateCardsManager(const std::vector<std::vector<PrivateCards>>& ranges,
                        int playerCount,
                        uint64_t initialBoard);

    // Returns a const reference to the private hand range of a given player.
    const std::vector<PrivateCards>& getPreflopCards(int player) const;

    // Map an index from one player's range to the corresponding index in another player's range.
    // (For now, we assume an identity mapping; adjust as needed for your application.)
    int mapIndexBetweenPlayers(int fromPlayer, int toPlayer, int index) const;

    // Compute the initial reach probabilities for a player's range given the initial board.
    // (In this simple implementation, all hands are weighted uniformly.)
    std::vector<float> computeInitialReachProbabilities(int player, uint64_t initialBoard) const;

    // Normalize or update the relative probabilities (weights) of all hands in each player's range.
    void updateRelativeProbabilities();

private:
    // For each player, store a vector of possible private hands.
    std::vector<std::vector<PrivateCards>> privateRanges_;

    // Number of players.
    int playerCount_;

    // Bitmask (or other representation) of the initial board.
    uint64_t initialBoard_;
};

} // namespace PokerSolver

#endif // POKERSOLVER_PRIVATECARDSMANAGER_H
