#pragma once

#include <cstdint>
#include <optional>

#include "ai/ai_engine.h"
#include "core/board.h"
#include "shared/stats.h"

namespace game2048 {

enum class ControlMode {
    Human,
    AIAutoplay,
    AISingleStep,
};

enum class OverlayMode {
    None,
    Help,
    Victory,
    GameOver,
};

enum class InputGate {
    Accepting,
    BlockedByOverlay,
    BlockedByAnimation,
};

enum class AnimationSpeed {
    Normal,
    Slow,
    Turbo
};

enum class AIStatus {
    Idle,
    Searching,
    Ready,
    Failed,
};

enum class RuntimeEventType {
    Reset,
    Undo,
    Move,
    ToggleAutoplay,
    StepAI,
    CycleAgent,
    CycleAnimation,
    OpenHelp,
    CloseOverlay,
    Quit,
};

struct RuntimeEvent {
    RuntimeEventType type = RuntimeEventType::Move;
    Direction direction = Direction::Left;
};

struct RuntimeConfig {
    std::uint64_t seed = kDefaultSeed;
    ai::AgentKind agent = ai::AgentKind::Expectimax;
    ai::SearchConfig search {};
    std::optional<Board> initialBoard {};
};

struct RuntimeMoveAnimation {
    std::uint64_t revision = 0;
    Board before {};
    Board after {};
    MoveTrace trace {};
    std::optional<SpawnEvent> spawn {};
    bool triggeredGameOver = false;
};

struct RuntimeSnapshot {
    Board board {};
    std::uint32_t score = 0;
    int maxTile = 0;
    bool gameOver = false;
    bool reached2048 = false;
    std::uint64_t seed = kDefaultSeed;
    std::uint64_t boardRevision = 0;
    bool quitRequested = false;

    ControlMode controlMode = ControlMode::Human;
    OverlayMode overlayMode = OverlayMode::None;
    InputGate inputGate = InputGate::Accepting;
    bool canDismissOverlay = false;
    bool showContinueHint = false;
    AnimationSpeed animationSpeed = AnimationSpeed::Normal;

    ai::AgentKind agent = ai::AgentKind::Expectimax;
    AIStatus aiStatus = AIStatus::Idle;
    ai::MoveDecision recommendation {};
    std::uint64_t recommendationRevision = 0;
    SearchStats lastSearch {};
    ai::FeatureBreakdown evaluatorBreakdown {};
    std::optional<RuntimeMoveAnimation> lastMove {};
};

}  // namespace game2048
