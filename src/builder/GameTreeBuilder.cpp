#include "GameTreeBuilder.h"

// Include the concrete node types we need.
#include "nodes/ActionNode.h"
#include "nodes/ChanceNode.h"
#include "nodes/TerminalNode.h"
#include <stdexcept>
#include <vector>
#include <algorithm>

namespace PokerSolver {

GameTreeBuilder::GameTreeBuilder(const Rule& rule)
    : rule_(rule)
{
}

std::shared_ptr<GameTreeNode> GameTreeBuilder::buildRoot() {
    // For the initial state, we create an ActionNode.
    // In a realistic implementation, the available actions would be derived
    // from the rule and the game state. Here, we create two dummy actions.
    std::vector<GameActions> actions;
    actions.push_back(GameActions(PokerAction::CHECK, 0.0));
    actions.push_back(GameActions(PokerAction::BET, 10.0));

    // The round is taken from the rule, and we use the current pot as given by Rule::getPot().
    auto root = std::make_shared<ActionNode>(rule_.currentRound(), rule_.getPot(), nullptr, 0, actions);
    return root;
}

void GameTreeBuilder::expandNode(std::shared_ptr<GameTreeNode> node) {
    if (!node) {
        throw std::invalid_argument("GameTreeBuilder::expandNode: Provided node is null.");
    }

    switch (node->type()) {
        case NodeType::Action: {
            // Expand an ActionNode into its child nodes.
            auto actionNode = std::dynamic_pointer_cast<ActionNode>(node);
            if (!actionNode) {
                throw std::runtime_error("GameTreeBuilder::expandNode: Failed to cast to ActionNode.");
            }
            
            // Determine the next round.
            GameRound currentRound = node->round();
            GameRound nextRound;
            bool isTerminal = false;
            switch (currentRound) {
                case GameRound::Preflop:
                    nextRound = GameRound::Flop;
                    break;
                case GameRound::Flop:
                    nextRound = GameRound::Turn;
                    break;
                case GameRound::Turn:
                    nextRound = GameRound::River;
                    break;
                case GameRound::River:
                    // At river, we assume no further expansion; create terminal node.
                    isTerminal = true;
                    nextRound = currentRound; // not used
                    break;
                default:
                    throw std::logic_error("GameTreeBuilder::expandNode: Unknown game round.");
            }
            
            if (isTerminal) {
                // Create a TerminalNode with dummy payoffs.
                std::vector<double> payoffs = {1.0, -1.0}; // Dummy payoff values.
                auto terminalChild = std::make_shared<TerminalNode>(currentRound, node->pot(), payoffs, 0, node);
                actionNode->addChild(terminalChild);
            } else {
                // For each available action (the number of branches equals the number of actions),
                // create a child ChanceNode with a dummy board.
                std::vector<Card> board;
                // For a flop, we expect three board cards.
                if (nextRound == GameRound::Flop) {
                    board.push_back(Card(Rank::Two, Suit::Clubs));
                    board.push_back(Card(Rank::Three, Suit::Clubs));
                    board.push_back(Card(Rank::Four, Suit::Clubs));
                } else if (nextRound == GameRound::Turn) {
                    // For turn, add one card.
                    board.push_back(Card(Rank::Five, Suit::Clubs));
                } else if (nextRound == GameRound::River) {
                    // For river, add another card.
                    board.push_back(Card(Rank::Six, Suit::Clubs));
                }
                
                // Create one child chance node per available action.
                int numBranches = static_cast<int>(actionNode->actions().size());
                for (int i = 0; i < numBranches; ++i) {
                    double newPot = node->pot() + 10; // Dummy pot update.
                    auto chanceChild = std::make_shared<ChanceNode>(nextRound, newPot, node, board, false);
                    actionNode->addChild(chanceChild);
                }
            }
            break;
        }
        case NodeType::Chance: {
            // Expand a ChanceNode into an ActionNode.
            auto chanceNode = std::dynamic_pointer_cast<ChanceNode>(node);
            if (!chanceNode) {
                throw std::runtime_error("GameTreeBuilder::expandNode: Failed to cast to ChanceNode.");
            }
            
            // For this example, suppose after a chance event the game goes to the next decision stage.
            GameRound currentRound = node->round();
            GameRound nextRound;
            switch (currentRound) {
                case GameRound::Flop:
                    nextRound = GameRound::Turn;
                    break;
                case GameRound::Turn:
                    nextRound = GameRound::River;
                    break;
                default:
                    // If not expected, do not expand.
                    return;
            }
            
            // Create dummy actions for the new decision node.
            std::vector<GameActions> actions;
            actions.push_back(GameActions(PokerAction::CHECK, 0.0));
            actions.push_back(GameActions(PokerAction::BET, 20.0));
            
            auto actionChild = std::make_shared<ActionNode>(nextRound, node->pot() + 5, node, 0, actions);
            chanceNode->setChild(actionChild);
            break;
        }
        case NodeType::Terminal:
        case NodeType::Showdown:
            // Do not expand terminal or showdown nodes.
            break;
        default:
            throw std::logic_error("GameTreeBuilder::expandNode: Unsupported node type encountered.");
    }
}

} // namespace PokerSolver
