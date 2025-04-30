#include "nodes/ChanceNode.h" // Adjust path if necessary

#include <utility>   // For std::move
#include <stdexcept> // For std::invalid_argument
#include <iostream>  // For std::cerr warning

namespace poker_solver {
namespace nodes {

// Use alias
namespace core = poker_solver::core;

// --- Constructor ---

ChanceNode::ChanceNode(core::GameRound round,
                       double pot,
                       std::weak_ptr<GameTreeNode> parent,
                       std::vector<core::Card> dealt_cards, // Matches header
                       std::shared_ptr<GameTreeNode> child_node, // Matches header
                       bool is_donk_opportunity)
    : core::GameTreeNode(round, pot, std::move(parent)),
      dealt_cards_(std::move(dealt_cards)), // Store the dealt cards
      child_node_(std::move(child_node)),   // Store the child node
      is_donk_opportunity_(is_donk_opportunity) {

    // Optional validation - allow empty dealt_cards for flexibility?
    // if (dealt_cards_.empty()) {
    //     throw std::invalid_argument("ChanceNode cannot have empty dealt_cards.");
    // }

    // Set the parent pointer on the initial child node, if provided
    if (child_node_) {
        // Use shared_from_this() which requires ChanceNode to inherit from std::enable_shared_from_this
        // which GameTreeNode already does.
        child_node_->SetParent(shared_from_this());
    }
}

// --- Modifiers ---

void ChanceNode::SetChild(std::shared_ptr<GameTreeNode> child) {
     if (!child) {
        throw std::invalid_argument("Cannot set a null child node for ChanceNode.");
    }
    child_node_ = std::move(child);
    // Ensure parent is set correctly
    child_node_->SetParent(shared_from_this());
}


} // namespace nodes
} // namespace poker_solver
