#include <filesystem>

#include "../src/app/ai_advisor.h"
#include "../src/app/game_controller.h"
#include "../src/app/hud_mapper.h"
#include "../src/app/interaction_session.h"
#include "test_framework.h"

namespace {

using game2048::AIAdvisor;
using game2048::AnimationSpeed;
using game2048::BuildHUDState;
using game2048::GameController;
using game2048::HUDControlMode;
using game2048::HUDAgentKind;
using game2048::HUDInputGate;
using game2048::HUDOverlayMode;
using game2048::InputCommand;
using game2048::InteractionInput;
using game2048::InteractionSession;
using game2048::ai::AgentKind;

struct ScopedFileCleanup {
    std::filesystem::path path;

    ~ScopedFileCleanup() {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
};

std::filesystem::path TestFilePath(const char* name) {
    return std::filesystem::temp_directory_path() / name;
}

InteractionInput MakeInput() {
    return {};
}

}  // namespace

TEST_CASE(HUDMapper_MapsSessionOverlayAndAnimationState) {
    const auto path = TestFilePath("game2048_test_hud_mapper_session.txt");
    ScopedFileCleanup cleanup {path};

    GameController controller(123, path);
    InteractionSession session(123);
    AIAdvisor advisor(AgentKind::Greedy);

    InteractionInput help = MakeInput();
    help.command = InputCommand::ToggleHelp;
    session.Tick(help);

    const auto hud = BuildHUDState(controller, session, advisor, AnimationSpeed::Turbo);

    EXPECT_EQ(hud.game.score, controller.Score());
    EXPECT_EQ(hud.game.bestScore, controller.BestScore());
    EXPECT_EQ(hud.game.seed, controller.Seed());
    EXPECT_EQ(hud.session.controlMode, HUDControlMode::Human);
    EXPECT_EQ(hud.session.overlayMode, HUDOverlayMode::Help);
    EXPECT_EQ(hud.session.inputGate, HUDInputGate::BlockedByOverlay);
    EXPECT_EQ(hud.session.animationSpeed, AnimationSpeed::Turbo);
    EXPECT_TRUE(hud.session.canDismissOverlay);
}

TEST_CASE(HUDMapper_MapsAdvisorRecommendationSearchAndBreakdown) {
    const auto path = TestFilePath("game2048_test_hud_mapper_ai.txt");
    ScopedFileCleanup cleanup {path};

    GameController controller(123, path);
    InteractionSession session(123);
    AIAdvisor advisor(AgentKind::Greedy);
    const auto decision = advisor.RequestMove(controller.BoardState());
    const auto breakdown = advisor.Breakdown(controller.BoardState());

    const auto hud = BuildHUDState(controller, session, advisor, AnimationSpeed::Normal);

    EXPECT_EQ(hud.ai.agentKind, HUDAgentKind::Greedy);
    EXPECT_EQ(hud.ai.recommendation.valid, decision.valid);
    EXPECT_EQ(hud.ai.recommendation.direction, decision.direction);
    EXPECT_EQ(hud.ai.lastSearch.nodes, decision.stats.nodes);
    EXPECT_EQ(hud.ai.lastSearch.cacheHits, decision.stats.cacheHits);
    EXPECT_EQ(hud.ai.lastSearch.maxDepthReached, decision.stats.maxDepthReached);
    EXPECT_NEAR(hud.ai.lastSearch.elapsedMs, decision.stats.elapsedMs, 1e-9);
    EXPECT_NEAR(hud.ai.lastSearch.evaluation, decision.stats.evaluation, 1e-9);
    EXPECT_NEAR(hud.ai.evaluatorBreakdown.emptyTiles, breakdown.emptyTiles, 1e-9);
    EXPECT_NEAR(hud.ai.evaluatorBreakdown.monotonicity, breakdown.monotonicity, 1e-9);
    EXPECT_NEAR(hud.ai.evaluatorBreakdown.smoothness, breakdown.smoothness, 1e-9);
    EXPECT_NEAR(hud.ai.evaluatorBreakdown.cornerMax, breakdown.cornerMax, 1e-9);
    EXPECT_NEAR(hud.ai.evaluatorBreakdown.mergePotential, breakdown.mergePotential, 1e-9);
    EXPECT_NEAR(hud.ai.evaluatorBreakdown.snakePattern, breakdown.snakePattern, 1e-9);
    EXPECT_NEAR(hud.ai.evaluatorBreakdown.trapPenalty, breakdown.trapPenalty, 1e-9);
}