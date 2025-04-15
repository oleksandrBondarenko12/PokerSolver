#include "RiverCombs.h"

namespace PokerSolver {

RiverCombs::RiverCombs()
    : rank_(0), reachProbIndex_(0)
{
    // Default constructor: creates an empty combination.
}

RiverCombs::RiverCombs(const std::vector<int>& board,
                       const PrivateCards &privateCards,
                       int rank,
                       int reachProbIndex)
    : rank_(rank),
      privateCards_(privateCards),
      reachProbIndex_(reachProbIndex),
      board_(board)
{
}

int RiverCombs::rank() const {
    return rank_;
}

const PrivateCards& RiverCombs::privateCards() const {
    return privateCards_;
}

int RiverCombs::reachProbIndex() const {
    return reachProbIndex_;
}

const std::vector<int>& RiverCombs::board() const {
    return board_;
}

} // namespace PokerSolver
