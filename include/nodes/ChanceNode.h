#ifndef POKERSOLVER_CHANCENODE_H
#define POKERSOLVER_CHANCENODE_H

#include "GameTreeNode.h"
#include "Card.h"
#include <vector>
#include <memory>

namespace PokerSolver {

class ChanceNode : public GameTreeNode {
public:
    // Constructor:
    // - round: the game round at which the chance event occurs.
    // - pot: the current pot value.
    // - parent: the parent node in the game tree.
    // - cards: the list of cards dealt (e.g. for flop, turn, or river).
    // - donk: an optional flag indicating a "donk" scenario (default is false).
    ChanceNode(GameRound round, double pot,
               std::shared_ptr<GameTreeNode> parent,
               const std::vector<Card>& cards,
               bool donk = false);

    // Destructor (defaulted).
    virtual ~ChanceNode() = default;

    // Accessor for cards involved in the chance event.
    const std::vector<Card>& cards() const;

    // Accessor and mutator for the child node.
    std::shared_ptr<GameTreeNode> child() const;
    void setChild(std::shared_ptr<GameTreeNode> child);

    // Returns whether this chance node is a donk.
    bool isDonk() const noexcept;

    // --- Override Abstract Interface from GameTreeNode ---
    NodeType type() const override;
    std::string nodeTypeToString() const override;

private:
    std::vector<Card> cards_;
    std::shared_ptr<GameTreeNode> child_;
    bool donk_;
};

} // namespace PokerSolver

#endif // POKERSOLVER_CHANCENODE_H
