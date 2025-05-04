///////////////////////////////////////////////////////////////////////////////
// tests/pcfr_solver_test.cpp                 – PCfrSolver smoke-test
///////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include "solver/PCfrSolver.h"
#include "solver/best_response.h"

#include "nodes/ActionNode.h"
#include "nodes/TerminalNode.h"
#include "nodes/ShowdownNode.h"
#include "nodes/GameActions.h"

#include "Deck.h"
#include "Card.h"
#include "GameTree.h"

#include "compairer/Dic5Compairer.h"
#include "ranges/PrivateCardsManager.h"
#include "ranges/RiverRangeManager.h"

#include "tools/GameTreeBuildingSettings.h"
#include "tools/StreetSetting.h"
#include "tools/Rule.h"

using namespace poker_solver;            // NOLINT
using namespace core;                    // NOLINT
using namespace nodes;                   // NOLINT
using namespace solver;                  // NOLINT
using namespace poker_solver::ranges;    // NOLINT
using tree::GameTree;

// ─────────────────────────────────────────────────────────────────────────────
// Helper: build a tiny 2-action tree (check / bet) just to exercise CFR.
// ─────────────────────────────────────────────────────────────────────────────
static std::shared_ptr<GameTreeNode> make_toy_root(double pot)
{
    auto root = std::make_shared<ActionNode>(0, GameRound::kPreflop, pot,
                                             std::weak_ptr<GameTreeNode>{}, 1);

    // branch 1: check–check → showdown
    auto p1chk = std::make_shared<ActionNode>(1, GameRound::kPreflop, pot,
                                              root, 1);
    root->AddChild(GameAction(PokerAction::kCheck), p1chk);

    p1chk->AddChild(GameAction(PokerAction::kCheck),
        std::make_shared<ShowdownNode>(GameRound::kPreflop, pot, p1chk, 2,
                                       std::vector<double>{1.0, 1.0}));

    // branch 2: bet → fold | call
    const double bet = 1.0;
    auto p1act = std::make_shared<ActionNode>(1, GameRound::kPreflop,
                                              pot + bet, root, 1);
    root->AddChild(GameAction(PokerAction::kBet, bet), p1act);

    p1act->AddChild(GameAction(PokerAction::kFold),
        std::make_shared<TerminalNode>(std::vector<double>{-1.0, 1.0},
                                       GameRound::kPreflop, pot + bet, p1act));

    p1act->AddChild(GameAction(PokerAction::kCall),
        std::make_shared<ShowdownNode>(GameRound::kPreflop, pot + 2 * bet,
                                       p1act, 2,
                                       std::vector<double>{2.0, 2.0}));

    return root;
}

// ─────────────────────────────────────────────────────────────────────────────
// Very small wrapper that exposes a fixed root.
// ─────────────────────────────────────────────────────────────────────────────
class ToyTree : public GameTree
{
public:
    ToyTree(const std::shared_ptr<GameTreeNode>& r,
            const config::Rule& rule,
            const core::Deck&   d)
        : GameTree(rule, d), root_(r) {}

    std::shared_ptr<core::GameTreeNode> GetRoot() const override { return root_; }

private:
    std::shared_ptr<GameTreeNode> root_;
};

// Build a minimal Rule object (all numbers arbitrary - we never use them).
static config::Rule make_dummy_rule(const Deck& deck)
{
    config::GameTreeBuildingSettings build;
    build.street_settings.push_back(config::StreetSetting{});
    return config::Rule(deck,
                        /*OOP commit*/0.0,
                        /*IP  commit*/0.0,
                        GameRound::kPreflop,
                        /*raise cap*/1,
                        /*SB*/0.5,
                        /*BB*/1.0,
                        /*stack*/100.0,
                        build);
}

// ─────────────────────────────────────────────────────────────────────────────
TEST(PCfrSolver, KuhnPokerSmoke)
{
    // ----- deck and toy ranges ---------------------------------------------
    Deck deck;
    int ac = Card::StringToInt("Ac").value();
    int kd = Card::StringToInt("Kd").value();

    std::vector<PrivateCards> rP0{ PrivateCards(ac, kd) };
    std::vector<PrivateCards> rP1{ PrivateCards(kd, ac) };
    std::vector<std::vector<PrivateCards>> ranges{ rP0, rP1 };

    // ----- helpers ----------------------------------------------------------
    auto cmp  = std::make_shared<eval::Dic5Compairer>("five_card_strength.txt");
    RiverRangeManager     rrm(cmp);
    PrivateCardsManager   pcm(ranges, /*board*/0);

    // ----- make tree --------------------------------------------------------
    auto root  = make_toy_root(1.0);
    auto rule  = make_dummy_rule(deck);
    auto tree  = std::make_shared<ToyTree>(root, rule, deck);

    // ----- solver config ----------------------------------------------------
    PCfrSolver::Config cfg;
    cfg.num_iterations       = 2000;
    cfg.print_interval       = 1000;
    cfg.trainable_precision  =
        static_cast<ActionNode::TrainablePrecision>(1e-6f);
    cfg.num_threads          = 1;
    cfg.debug_log            = false;

    PCfrSolver cfr(tree, ranges, deck, cmp, cfg);
    cfr.Train();

    // ----- exploitability check --------------------------------------------
    BestResponse br({/*debug*/false});
    double expl = br.CalculateExploitability(root, ranges,
                                             pcm, rrm, deck,
                                             /*board*/0, root->GetPot());

    // very loose bar – just prove loop converges a bit
    EXPECT_LT(expl, 0.25);
}
