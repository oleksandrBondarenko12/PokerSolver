#ifndef POKERSOLVER_RIVERCOMBS_H
#define POKERSOLVER_RIVERCOMBS_H

#include <vector>
#include "PrivateCards.h"

namespace PokerSolver {

/// \brief Represents a combination at the river stage.
///
/// This structure bundles together:
///   - The board cards used (represented as a vector of int for simplicity; 
///     in a more advanced version you might use a bit–mask or a vector of Card objects).
///   - The private cards (encapsulated in a PrivateCards instance)
///   - The computed rank (an evaluation metric) for this combination.
///   - An index (reachProbIndex) that can be used to look up reach probabilities.
class RiverCombs {
public:
    /// Default constructor.
    RiverCombs();

    /// Construct a RiverCombs instance.
    ///
    /// \param board A vector representing the board cards (e.g., card bytes or indices).
    /// \param privateCards The private cards combination.
    /// \param rank The evaluated rank (or strength) of the resulting hand.
    /// \param reachProbIndex An index into the reach–probability data.
    RiverCombs(const std::vector<int>& board, const PrivateCards &privateCards, int rank, int reachProbIndex);

    // --- Accessors ---

    /// Returns the evaluated rank.
    [[nodiscard]] int rank() const;

    /// Returns the PrivateCards object.
    [[nodiscard]] const PrivateCards& privateCards() const;

    /// Returns the reach probability index.
    [[nodiscard]] int reachProbIndex() const;

    /// Returns the board representation.
    [[nodiscard]] const std::vector<int>& board() const;

private:
    int rank_;
    PrivateCards privateCards_;
    int reachProbIndex_;
    std::vector<int> board_;
};

} // namespace PokerSolver

#endif // POKERSOLVER_RIVERCOMBS_H
