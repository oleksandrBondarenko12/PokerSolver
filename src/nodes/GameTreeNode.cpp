#include "nodes/GameTreeNode.h" // Adjust path if necessary


#include <utility> // For std::move

namespace poker_solver {
namespace core {

// --- Constructor ---

// Protected constructor definition
GameTreeNode::GameTreeNode(GameRound round, double pot,
                           std::weak_ptr<GameTreeNode> parent)
    : round_(round), pot_(pot), parent_(std::move(parent)) {}


// --- SetParent Method ---
void GameTreeNode::SetParent(std::shared_ptr<GameTreeNode> parent) {
    // Store the parent as a weak_ptr
    parent_ = parent;
}


// --- Static Helpers ---

GameRound GameTreeNode::IntToGameRound(int round_int) {
  switch (round_int) {
    case 0: return GameRound::kPreflop;
    case 1: return GameRound::kFlop;
    case 2: return GameRound::kTurn;
    case 3: return GameRound::kRiver;
    default: { // Use braces for scope within switch case
        std::ostringstream oss;
        oss << "Invalid integer for GameRound: " << round_int;
        throw std::out_of_range(oss.str());
      }
  }
}

int GameTreeNode::GameRoundToInt(GameRound game_round) {
  // Underlying enum type is int, direct cast is safe here.
  return static_cast<int>(game_round);
}

std::string GameTreeNode::GameRoundToString(GameRound game_round) {
  switch (game_round) {
    case GameRound::kPreflop: return "Preflop";
    case GameRound::kFlop:    return "Flop";
    case GameRound::kTurn:    return "Turn";
    case GameRound::kRiver:   return "River";
    default:                  return "UnknownRound"; // Should not happen
  }
}

} // namespace core
} // namespace poker_solver
