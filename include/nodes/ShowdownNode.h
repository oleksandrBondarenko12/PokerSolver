#ifndef POKERSOLVER_SHOWDOWNNODE_H
#define POKERSOLVER_SHOWDOWNNODE_H

#include "GameTreeNode.h"
#include <vector>
#include <string>
#include <stdexcept>

namespace PokerSolver {

// A specialized node type for showdown outcomes.
class ShowdownNode : public GameTreeNode {
public:
    // Enumeration to indicate the type of showdown outcome.
    enum class ShowdownResult {
        NotTie, // a clear winning outcome
        Tie     // tied outcome: the pot is split among players
    };

    // Constructor.
    // tiePayoffs: vector of payoffs for each player in case of a tie.
    // playerPayoffs: matrix of payoffs for non-tie outcome; each row corresponds to the winning player,
    //                and each column gives that player's payoff against each opponent or scenario.
    // round: the game round (should be River or Turn/Flop as applicable).
    // pot: the current pot size.
    // parent: pointer to the parent node (if any).
    ShowdownNode(const std::vector<double>& tiePayoffs,
                 const std::vector<std::vector<double>>& playerPayoffs,
                 GameRound round,
                 double pot,
                 std::shared_ptr<GameTreeNode> parent = nullptr);

    // Override the pure virtual methods from GameTreeNode.
    NodeType type() const override;
    std::string nodeTypeToString() const override;

    // Returns the complete payoff vector for a given outcome.
    // If result == Tie, returns the tiePayoffs vector;
    // if result == NotTie, returns the payoff vector for the winning player (given by 'winner').
    std::vector<double> getPayoffs(ShowdownResult result, int winner) const;

    // Returns a single payoff value.
    // If result == Tie, returns the payoff for the specified player;
    // if result == NotTie, returns the payoff for the specified player from the winner's payoff row.
    double getPayoff(ShowdownResult result, int winner, int player) const;

private:
    // Data for tied outcomes: one payoff per player.
    std::vector<double> tiePayoffs_;

    // Data for non–tie outcomes: each row corresponds to a winning player's payoff distribution.
    std::vector<std::vector<double>> playerPayoffs_;
};

} // namespace PokerSolver

#endif // POKERSOLVER_SHOWDOWNNODE_H
