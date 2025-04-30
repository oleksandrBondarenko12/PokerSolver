#include "nodes/ActionNode.h" // Adjust path if necessary
#include "trainable/Trainable.h"  // Include base class for shared_ptr type

// Include concrete Trainable implementations needed for lazy creation
#include "trainable/DiscountedCfrTrainable.h" // Adjust path
// #include "solver/discounted_cfr_trainable_hf.h" // Include if using half-float
// #include "solver/discounted_cfr_trainable_sf.h" // Include if using single-float

#include <stdexcept>
#include <sstream>
#include <utility> // For std::move

namespace poker_solver {
namespace nodes {

// Use aliases
namespace core = poker_solver::core;
namespace solver = poker_solver::solver;


// --- Constructor ---

ActionNode::ActionNode(size_t player_index,
                       core::GameRound round,
                       double pot,
                       std::weak_ptr<GameTreeNode> parent,
                       size_t num_possible_deals)
    : core::GameTreeNode(round, pot, std::move(parent)),
      player_index_(player_index),
      trainables_(num_possible_deals, nullptr) // Initialize vector with nullptrs
       {
    if (num_possible_deals == 0) {
        throw std::invalid_argument("Number of possible deals cannot be zero.");
    }
}

// --- Modifiers ---

void ActionNode::AddChild(core::GameAction action, std::shared_ptr<GameTreeNode> child) {
    if (!child) {
        throw std::invalid_argument("Cannot add a null child node.");
    }
    actions_.push_back(std::move(action));
    children_.push_back(std::move(child));
    // Set the parent pointer on the child node
    children_.back()->SetParent(shared_from_this());
}

void ActionNode::SetActionsAndChildren(
    std::vector<core::GameAction> actions,
    std::vector<std::shared_ptr<GameTreeNode>> children) {
    if (actions.size() != children.size()) {
        throw std::invalid_argument(
            "Number of actions must match number of children.");
    }
    actions_ = std::move(actions);
    children_ = std::move(children);
    // Set parent pointers for all children
    for (const auto& child : children_) {
        if (child) {
            child->SetParent(shared_from_this());
        } else {
             throw std::invalid_argument("Cannot set a null child node.");
        }
    }
}


// --- Trainable Management ---

void ActionNode::SetPlayerRange(const std::vector<core::PrivateCards>* player_range) {
    if (!player_range) {
         throw std::invalid_argument("Player range pointer cannot be null.");
    }
    player_range_ = player_range;
}

std::shared_ptr<solver::Trainable> ActionNode::GetTrainableIfExists(size_t deal_index) const {
     if (deal_index >= trainables_.size()) {
        std::ostringstream oss;
        oss << "Invalid deal_index (" << deal_index << ") for ActionNode. Max index is "
            << (trainables_.size() > 0 ? trainables_.size() - 1 : 0) << ".";
        throw std::out_of_range(oss.str());
    }
    return trainables_[deal_index];
}


std::shared_ptr<solver::Trainable> ActionNode::GetTrainable(
    size_t deal_index,
    TrainablePrecision precision) {

    if (!player_range_) {
         throw std::runtime_error(
            "Player range must be set via SetPlayerRange before calling GetTrainable.");
    }
    if (deal_index >= trainables_.size()) {
        std::ostringstream oss;
        oss << "Invalid deal_index (" << deal_index << ") for ActionNode. Max index is "
            << (trainables_.size() > 0 ? trainables_.size() - 1 : 0) << ".";
        throw std::out_of_range(oss.str());
    }

    // Lazy creation: If the pointer at this index is null, create the object.
    if (!trainables_[deal_index]) {
        // Select concrete Trainable type based on precision (or other config)
        // For now, we only have DiscountedCfrTrainable implemented.
        switch (precision) {
            case TrainablePrecision::kFloat:
                // Pass the associated player range and a reference to this node
                // Note: Passing *this directly requires careful lifetime management if
                // the Trainable stores it. Passing necessary info might be safer.
                // The original passed 'this', let's stick to that for now.
                trainables_[deal_index] =
                    std::make_shared<solver::DiscountedCfrTrainable>(
                        player_range_, *this);
                break;
            case TrainablePrecision::kHalf:
                 throw std::runtime_error("Half-float Trainable not implemented yet.");
                 // trainables_[deal_index] =
                 //    std::make_shared<solver::DiscountedCfrTrainableHF>(
                 //        player_range_, *this);
                break;
            case TrainablePrecision::kSingle:
                 throw std::runtime_error("Single-float Trainable not implemented yet.");
                 // trainables_[deal_index] =
                 //    std::make_shared<solver::DiscountedCfrTrainableSF>(
                 //        player_range_, *this);
                break;
            default:
                 throw std::runtime_error("Unknown TrainablePrecision specified.");
        }
    }

    return trainables_[deal_index];
}


} // namespace nodes
} // namespace poker_solver
