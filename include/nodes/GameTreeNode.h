#ifndef POKERSOLVER_GAMETREENODE_H
#define POKERSOLVER_GAMETREENODE_H

#include <memory>
#include <string>

namespace PokerSolver {

// -----------------------------------------------------------------------------
// Enumerations for actions, node types, and game rounds

enum class PokerAction {
    BEGIN,
    ROUNDBEGIN,
    BET,
    RAISE,
    CHECK,
    FOLD,
    CALL
};

enum class NodeType {
    Action,
    Chance,
    Terminal,
    Showdown
};

enum class GameRound {
    Preflop,
    Flop,
    Turn,
    River
};

// -----------------------------------------------------------------------------
// Abstract base class for a game-tree node
//
// This class encapsulates the common attributes of all nodes (the game round,
// the pot value, the parent pointer, and the node depth) and provides some
// utility methods for debugging and tracing the node history. Derived classes
// must override the pure virtual functions to report their own node type and
// provide node–specific behavior.
//
class GameTreeNode {
public:
    // Constructors
    GameTreeNode();
    GameTreeNode(GameRound round, double pot, std::shared_ptr<GameTreeNode> parent = nullptr);

    // Virtual destructor
    virtual ~GameTreeNode();

    // --- Pure Virtual Interface ---
    // Returns the type of this node. Each derived class must implement this.
    virtual NodeType type() const = 0;

    // Returns a human–readable string for this node’s type.
    virtual std::string nodeTypeToString() const = 0;

    // --- Accessors ---
    GameRound round() const;
    double pot() const;
    std::shared_ptr<GameTreeNode> parent() const;
    int depth() const;

    // --- Mutators ---
    void setParent(std::shared_ptr<GameTreeNode> parent);
    void setPot(double pot);
    void setRound(GameRound round);

    // --- Utility Methods ---
    // Returns a one–line string representation of the node.
    std::string toString() const;

    // Returns a string containing the history from the root to this node.
    std::string printHistory() const;

    // --- Static Helper Functions ---
    // Converts a GameRound enum value to a string.
    static std::string gameRoundToString(GameRound round);

protected:
    GameRound round_{ GameRound::Preflop };
    double pot_{ 0.0 };

private:
    // Parent pointer stored as a weak pointer to avoid cyclic references.
    std::weak_ptr<GameTreeNode> parent_;
    int depth_{ 0 };

    // Disable copy construction and assignment.
    GameTreeNode(const GameTreeNode&) = delete;
    GameTreeNode& operator=(const GameTreeNode&) = delete;
};

} // namespace PokerSolver

#endif // POKERSOLVER_GAMETREENODE_H
