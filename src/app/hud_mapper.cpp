#include "app/hud_mapper.h"

#include "app/ai_advisor.h"
#include "app/game_controller.h"
#include "app/interaction_session.h"

namespace game2048 {

namespace {

constexpr HUDControlMode ToHUDControlMode(ControlMode mode) {
    switch (mode) {
        case ControlMode::Human:
            return HUDControlMode::Human;
        case ControlMode::AIAutoplay:
            return HUDControlMode::AIAutoplay;
        case ControlMode::AISingleStep:
            return HUDControlMode::AISingleStep;
    }
    return HUDControlMode::Human;
}

constexpr HUDOverlayMode ToHUDOverlayMode(OverlayMode mode) {
    switch (mode) {
        case OverlayMode::None:
            return HUDOverlayMode::None;
        case OverlayMode::Help:
            return HUDOverlayMode::Help;
        case OverlayMode::Victory:
            return HUDOverlayMode::Victory;
        case OverlayMode::GameOver:
            return HUDOverlayMode::GameOver;
    }
    return HUDOverlayMode::None;
}

constexpr HUDInputGate ToHUDInputGate(InputGate gate) {
    switch (gate) {
        case InputGate::Accepting:
            return HUDInputGate::Accepting;
        case InputGate::BlockedByOverlay:
            return HUDInputGate::BlockedByOverlay;
        case InputGate::BlockedByAnimation:
            return HUDInputGate::BlockedByAnimation;
    }
    return HUDInputGate::Accepting;
}

constexpr HUDAgentKind ToHUDAgentKind(ai::AgentKind kind) {
    switch (kind) {
        case ai::AgentKind::Human:
            return HUDAgentKind::Human;
        case ai::AgentKind::Greedy:
            return HUDAgentKind::Greedy;
        case ai::AgentKind::Expectimax:
            return HUDAgentKind::Expectimax;
    }
    return HUDAgentKind::Expectimax;
}

RecommendationHUD ToRecommendationHUD(const ai::MoveDecision& decision) {
    RecommendationHUD hud {};
    hud.direction = decision.direction;
    hud.valid = decision.valid;
    return hud;
}

SearchHUD ToSearchHUD(const SearchStats& stats) {
    SearchHUD hud {};
    hud.nodes = stats.nodes;
    hud.cacheHits = stats.cacheHits;
    hud.maxDepthReached = stats.maxDepthReached;
    hud.elapsedMs = stats.elapsedMs;
    hud.evaluation = stats.evaluation;
    return hud;
}

EvaluatorHUD ToEvaluatorHUD(const ai::FeatureBreakdown& breakdown) {
    EvaluatorHUD hud {};
    hud.emptyTiles = breakdown.emptyTiles;
    hud.monotonicity = breakdown.monotonicity;
    hud.smoothness = breakdown.smoothness;
    hud.cornerMax = breakdown.cornerMax;
    hud.mergePotential = breakdown.mergePotential;
    hud.snakePattern = breakdown.snakePattern;
    hud.trapPenalty = breakdown.trapPenalty;
    return hud;
}

}  // namespace

HUDState BuildHUDState(const GameController& gameController,
                       const InteractionSession& session,
                       const AIAdvisor& advisor,
                       AnimationSpeed animationSpeed) {
    HUDState hud {};
    hud.game.score = gameController.Score();
    hud.game.bestScore = gameController.BestScore();
    hud.game.maxTile = gameController.BoardState().MaxTile();
    hud.game.gameOver = gameController.IsGameOver();
    hud.game.achieved2048 = gameController.HasReached2048Ever();
    hud.game.seed = gameController.Seed();

    hud.session.canDismissOverlay = session.CanDismissOverlay();
    hud.session.showContinueHint = session.ShowContinueHint();
    hud.session.controlMode = ToHUDControlMode(session.Control());
    hud.session.overlayMode = ToHUDOverlayMode(session.Overlay());
    hud.session.inputGate = ToHUDInputGate(session.Gate());
    hud.session.animationSpeed = animationSpeed;

    hud.ai.agentKind = ToHUDAgentKind(advisor.GetAgent());
    hud.ai.recommendation = ToRecommendationHUD(advisor.Recommendation());
    hud.ai.lastSearch = ToSearchHUD(advisor.LastSearch());
    hud.ai.evaluatorBreakdown = ToEvaluatorHUD(advisor.Breakdown(gameController.BoardState()));
    return hud;
}

}  // namespace game2048