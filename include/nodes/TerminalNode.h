#ifndef POKERSOLVER_TERMINALNODE_H
#define POKERSOLVER_TERMINALNODE_H

#include "GameTreeNode.h"
#include <vector>
#include <string>
#include <memory>

namespace PokerSolver {

// TerminalNode represents an end-of-game node with final payoffs.
class TerminalNode : public GameTreeNode {
public:
    // Constructor.
    // @param round: the game round (Preflop, Flop, Turn, River) at which the terminal state is reached.
    // @param pot: the pot value at the terminal state.
    // @param payoffs: a vector of final payoffs for each player.
    // @param winner: the index of the winning player (if applicable; otherwise, -1).
    // @param parent: pointer to the parent node (default is nullptr for root).
    TerminalNode(GameRound round, double pot, const std::vector<double>& payoffs, int winner = -1, 
                 std::shared_ptr<GameTreeNode> parent = nullptr);

    // Virtual destructor.
    virtual ~TerminalNode() override = default;

    // --- Override of Pure Virtual Methods from GameTreeNode ---

    // Returns the type of this node: Terminal.
    virtual NodeType type() const override;

    // Returns a string representation of the node type.
    virtual std::string nodeTypeToString() const override;

    // --- Accessors ---
    
    // Returns the final payoffs.
    const std::vector<double>& payoffs() const;

    // Returns the winning player index (-1 if no single winner).
    int winner() const;

    // Returns a one-line string representation (overrides base toString if needed).
    virtual std::string toString() const override;

private:
    std::vector<double> payoffs_;
    int winner_; // Index of the winning player (or -1 if not applicable)
};

} // namespace PokerSolver

#endif // POKERSOLVER_TERMINALNODE_H
