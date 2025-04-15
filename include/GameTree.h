#ifndef POKERSOLVER_GAMETREE_H
#define POKERSOLVER_GAMETREE_H

#include "nodes/GameTreeNode.h"
#include <memory>
#include <functional>
#include <queue>

namespace PokerSolver {

/**
 * @brief The GameTree class encapsulates the root and overall traversal of the game tree.
 *
 * This class is responsible for storing the pointer to the root node and providing common
 * methods for traversing (or later modifying) the tree. It decouples the tree‐management logic
 * from the individual node implementations.
 */
class GameTree {
public:
    /// Default constructor creates an empty tree (with a null root).
    GameTree() = default;

    /// Constructor with an initial root.
    explicit GameTree(std::shared_ptr<GameTreeNode> root)
        : root_(std::move(root)) {}

    /// Returns the root node of the game tree.
    std::shared_ptr<GameTreeNode> root() const {
        return root_;
    }

    /// Sets the root node of the game tree.
    void setRoot(std::shared_ptr<GameTreeNode> root) {
        root_ = std::move(root);
    }

    /**
     * @brief Performs a Depth-First Search (DFS) traversal of the game tree.
     *
     * Calls the provided callback function on each node encountered.
     *
     * @param callback A function taking a shared pointer to GameTreeNode.
     */
    void traverseDFS(const std::function<void(std::shared_ptr<GameTreeNode>)>& callback) const {
        std::function<void(std::shared_ptr<GameTreeNode>)> dfsHelper =
            [&](std::shared_ptr<GameTreeNode> node) {
                if (!node) return;
                callback(node);
                // If the node supports children (for instance in ActionNode),
                // a dynamic cast might be used to iterate over them.
                // For this example, we assume each node has a virtual method to
                // provide its children, or you might downcast based on NodeType.
                // Here we use a simple placeholder lambda.
                // (Implementing a generic "getChildren" in GameTreeNode may be an option.)
            };
        dfsHelper(root_);
    }

    /**
     * @brief Performs a Breadth-First Search (BFS) traversal of the game tree.
     *
     * Calls the provided callback function on each node encountered.
     *
     * @param callback A function taking a shared pointer to GameTreeNode.
     */
    void traverseBFS(const std::function<void(std::shared_ptr<GameTreeNode>)>& callback) const {
        if (!root_) return;
        std::queue<std::shared_ptr<GameTreeNode>> nodeQueue;
        nodeQueue.push(root_);
        while (!nodeQueue.empty()) {
            auto node = nodeQueue.front();
            nodeQueue.pop();
            callback(node);
            // If the node has children, add them to the queue.
            // (This requires that each concrete node type provides a method for accessing children.)
            // For instance:
            //     auto children = node->children();
            //     for (auto& child : children) {
            //         nodeQueue.push(child);
            //     }
        }
    }

private:
    std::shared_ptr<GameTreeNode> root_;
};

} // namespace PokerSolver

#endif // POKERSOLVER_GAMETREE_H
