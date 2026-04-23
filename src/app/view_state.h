#pragma once

#include <cstdint>

#include "app/session_types.h"
#include "ui/animation.h"
#include "core/board.h"

namespace game2048 {

enum class HUDControlMode {
    Human,
    AIAutoplay,
    AISingleStep,
};

enum class HUDOverlayMode {
    None,
    Help,
    Victory,
    GameOver,
};

enum class HUDInputGate {
    Accepting,
    BlockedByOverlay,
    BlockedByAnimation,
};

enum class HUDAgentKind {
    Human,
    Greedy,
    Expectimax,
};

struct GameHUD {
    std::uint32_t score = 0;
    std::uint32_t bestScore = 0;
    int maxTile = 0;
    bool gameOver = false;
    bool achieved2048 = false;
    std::uint64_t seed = 0;
};

struct SessionHUD {
    bool canDismissOverlay = false;
    bool showContinueHint = false;
    HUDControlMode controlMode = HUDControlMode::Human;
    HUDOverlayMode overlayMode = HUDOverlayMode::None;
    HUDInputGate inputGate = HUDInputGate::Accepting;
    AnimationSpeed animationSpeed = AnimationSpeed::Normal;
};

struct RecommendationHUD {
    Direction direction = Direction::Left;
    bool valid = false;
};

struct SearchHUD {
    std::uint64_t nodes = 0;
    std::uint64_t cacheHits = 0;
    int maxDepthReached = 0;
    double elapsedMs = 0.0;
    double evaluation = 0.0;
};

struct EvaluatorHUD {
    double emptyTiles = 0.0;
    double monotonicity = 0.0;
    double smoothness = 0.0;
    double cornerMax = 0.0;
    double mergePotential = 0.0;
    double snakePattern = 0.0;
    double trapPenalty = 0.0;
};

struct AIHUD {
    HUDAgentKind agentKind = HUDAgentKind::Expectimax;
    RecommendationHUD recommendation {};
    SearchHUD lastSearch {};
    EvaluatorHUD evaluatorBreakdown {};
};

struct HUDState {
    GameHUD game {};
    SessionHUD session {};
    AIHUD ai {};
};

}  // namespace game2048
