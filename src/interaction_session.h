#pragma once

#include <cstdint>
#include <optional>

#include "board.h"
#include "input.h"

namespace game2048 {

enum class ControlMode {
    Human,
    AIAutoplay,
    AISingleStep
};

enum class OverlayMode {
    None,
    Help,
    Victory,
    GameOver
};

enum class InputGate {
    Accepting,
    BlockedByOverlay,
    BlockedByAnimation
};

struct InteractionInput {
    InputCommand command = InputCommand::None;
    std::optional<Direction> pressedMove {};
    std::optional<Direction> heldMove {};
    double nowSeconds = 0.0;
    bool animationBlocksInput = false;
    bool gameOver = false;
    bool reached2048Ever = false;
};

struct InteractionActions {
    bool exitRequested = false;
    bool resetRequested = false;
    bool undoRequested = false;
    bool cycleAgentRequested = false;
    bool cycleAnimationRequested = false;
    bool hintInvalidated = false;
    bool aiMoveRequested = false;
    std::optional<Direction> moveToExecute {};
};

class InteractionSession {
public:
    static constexpr double kRepeatDelaySeconds = 0.20;
    static constexpr double kRepeatPeriodSeconds = 0.075;

    explicit InteractionSession(std::uint64_t seed = 0);

    InteractionActions Tick(const InteractionInput& input);
    void OnReset(std::uint64_t seed);

    ControlMode Control() const;
    OverlayMode Overlay() const;
    InputGate Gate() const;
    bool CanDismissOverlay() const;
    bool ShowContinueHint() const;
    bool HasShownVictoryOverlay() const;
    std::uint64_t Seed() const;
    std::optional<Direction> PendingMove() const;

private:
    void SyncGameState(const InteractionInput& input);
    void RecomputeInputGate(bool animationBlocksInput);
    void ClearBufferedInput();
    void BufferMove(Direction direction);
    std::optional<Direction> ReleaseBufferedMove();
    bool IsModalOverlay() const;

    std::uint64_t seed_ = 0;
    ControlMode controlMode_ = ControlMode::Human;
    OverlayMode overlayMode_ = OverlayMode::None;
    InputGate inputGate_ = InputGate::Accepting;
    bool victoryOverlayShown_ = false;
    std::optional<Direction> pendingMove_ {};
    std::optional<Direction> lastHeldDir_ {};
    double repeatDeadline_ = 0.0;
};

}  // namespace game2048
