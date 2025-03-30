#ifndef POKERSOLVER_ACTIONNODE_H
#define POKERSOLVER_ACTIONNODE_H

#include "GameTreeNode.h"
#include "GameActions.h"    // For the GameActions type
#include "Trainable.h"      // For the pointer to Trainable instances
#include <vector>
#include <memory>

namespace PokerSolver {

// ActionNode represents a decision node in the game tree where a player chooses among a set of actions.
class ActionNode : public GameTreeNode {
public:
    // Constructor: creates an ActionNode for a given game round, pot, parent node,
    // the acting player's index, and the list of available actions.
    ActionNode(GameRound round,
               double pot,
               std::shared_ptr<GameTreeNode> parent,
               int player,
               const std::vector<GameActions>& actions);

    // Virtual destructor
    virtual ~ActionNode();

    // --- Overrides from GameTreeNode ---
    NodeType type() const override;
    std::string nodeTypeToString() const override;

    // --- Accessors ---
    // Returns the player index who is to act in this node.
    int player() const;

    // Returns the list of available actions.
    const std::vector<GameActions>& actions() const;

    // Returns the child nodes (each child represents the result of taking an action).
    const std::vector<std::shared_ptr<GameTreeNode>>& children() const;

    // Returns the trainable instance for a given action index.
    // (Each action branch can be trained separately.)
    std::shared_ptr<Trainable> getTrainable(int actionIndex) const;

    // --- Mutators and helpers ---
    // Adds a child node to the list of children.
    void addChild(const std::shared_ptr<GameTreeNode>& child);

    // Associates a Trainable instance with an action index.
    void setTrainable(int actionIndex, std::shared_ptr<Trainable> trainable);

private:
    int player_; // The index of the player who is to act at this node.
    std::vector<GameActions> actions_; // List of available game actions.
    std::vector<std::shared_ptr<GameTreeNode>> children_; // Child nodes resulting from each action.
    // A vector of Trainable pointers—one for each available action.
    std::vector<std::shared_ptr<Trainable>> trainables_;
};

} // namespace PokerSolver

#endif // POKERSOLVER_ACTIONNODE_H
