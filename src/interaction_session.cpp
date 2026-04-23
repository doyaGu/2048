#include "interaction_session.h"

namespace game2048 {

InteractionSession::InteractionSession(std::uint64_t seed)
    : seed_(seed) {}

InteractionActions InteractionSession::Tick(const InteractionInput& input) {
    InteractionActions actions;
    SyncGameState(input);
    RecomputeInputGate(input.animationBlocksInput);

    switch (input.command) {
        case InputCommand::Reset:
            actions.resetRequested = true;
            actions.hintInvalidated = true;
            OnReset(seed_);
            return actions;

        case InputCommand::Undo:
            if (overlayMode_ != OverlayMode::Help && overlayMode_ != OverlayMode::GameOver) {
                actions.undoRequested = true;
                actions.hintInvalidated = true;
                overlayMode_ = OverlayMode::None;
                controlMode_ = ControlMode::Human;
                ClearBufferedInput();
                RecomputeInputGate(input.animationBlocksInput);
            }
            return actions;

        case InputCommand::ToggleHelp:
            if (overlayMode_ == OverlayMode::GameOver) {
                return actions;
            }
            overlayMode_ = overlayMode_ == OverlayMode::Help ? OverlayMode::None : OverlayMode::Help;
            controlMode_ = ControlMode::Human;
            ClearBufferedInput();
            RecomputeInputGate(input.animationBlocksInput);
            return actions;

        case InputCommand::ToggleAutoAI:
            if (overlayMode_ == OverlayMode::Help || overlayMode_ == OverlayMode::GameOver) {
                return actions;
            }
            overlayMode_ = OverlayMode::None;
            ClearBufferedInput();
            controlMode_ = controlMode_ == ControlMode::AIAutoplay ? ControlMode::Human : ControlMode::AIAutoplay;
            actions.hintInvalidated = true;
            RecomputeInputGate(input.animationBlocksInput);
            break;

        case InputCommand::StepAI:
            if (overlayMode_ == OverlayMode::Help || overlayMode_ == OverlayMode::GameOver) {
                return actions;
            }
            overlayMode_ = OverlayMode::None;
            ClearBufferedInput();
            controlMode_ = ControlMode::AISingleStep;
            actions.hintInvalidated = true;
            RecomputeInputGate(input.animationBlocksInput);
            break;

        case InputCommand::CycleAgent:
            if (overlayMode_ != OverlayMode::Help && overlayMode_ != OverlayMode::GameOver) {
                actions.cycleAgentRequested = true;
                actions.hintInvalidated = true;
            }
            return actions;

        case InputCommand::CycleAnimationSpeed:
            if (overlayMode_ != OverlayMode::Help && overlayMode_ != OverlayMode::GameOver) {
                actions.cycleAnimationRequested = true;
            }
            return actions;

        case InputCommand::Exit:
            if (overlayMode_ == OverlayMode::Help || overlayMode_ == OverlayMode::Victory) {
                overlayMode_ = OverlayMode::None;
                controlMode_ = ControlMode::Human;
                ClearBufferedInput();
                RecomputeInputGate(input.animationBlocksInput);
            } else {
                actions.exitRequested = true;
            }
            return actions;

        case InputCommand::None:
            break;
    }

    if (overlayMode_ == OverlayMode::Help || overlayMode_ == OverlayMode::GameOver) {
        return actions;
    }

    if (overlayMode_ == OverlayMode::Victory && input.pressedMove.has_value()) {
        overlayMode_ = OverlayMode::None;
        controlMode_ = ControlMode::Human;
        ClearBufferedInput();
        actions.hintInvalidated = true;
        actions.moveToExecute = input.pressedMove;
        RecomputeInputGate(input.animationBlocksInput);
        return actions;
    }

    if (controlMode_ != ControlMode::AIAutoplay) {
        if (!input.animationBlocksInput) {
            if (const auto buffered = ReleaseBufferedMove(); buffered.has_value()) {
                actions.moveToExecute = buffered;
                actions.hintInvalidated = true;
                return actions;
            }
        }

        if (input.pressedMove.has_value()) {
            if (input.animationBlocksInput) {
                BufferMove(*input.pressedMove);
            } else {
                actions.moveToExecute = input.pressedMove;
            }
            actions.hintInvalidated = true;
            return actions;
        }

        if (controlMode_ == ControlMode::Human) {
            if (input.heldMove != lastHeldDir_) {
                lastHeldDir_ = input.heldMove;
                repeatDeadline_ = input.nowSeconds + kRepeatDelaySeconds;
            } else if (input.heldMove.has_value() && input.nowSeconds >= repeatDeadline_) {
                if (input.animationBlocksInput) {
                    BufferMove(*input.heldMove);
                } else {
                    actions.moveToExecute = input.heldMove;
                }
                actions.hintInvalidated = true;
                repeatDeadline_ = input.nowSeconds + kRepeatPeriodSeconds;
                return actions;
            }
        }
    }

    if (overlayMode_ == OverlayMode::None && !input.animationBlocksInput && !input.gameOver) {
        if (controlMode_ == ControlMode::AIAutoplay) {
            actions.aiMoveRequested = true;
        } else if (controlMode_ == ControlMode::AISingleStep) {
            actions.aiMoveRequested = true;
            controlMode_ = ControlMode::Human;
        }
    }

    return actions;
}

void InteractionSession::OnReset(std::uint64_t seed) {
    seed_ = seed;
    controlMode_ = ControlMode::Human;
    overlayMode_ = OverlayMode::None;
    inputGate_ = InputGate::Accepting;
    victoryOverlayShown_ = false;
    ClearBufferedInput();
}

ControlMode InteractionSession::Control() const {
    return controlMode_;
}

OverlayMode InteractionSession::Overlay() const {
    return overlayMode_;
}

InputGate InteractionSession::Gate() const {
    return inputGate_;
}

bool InteractionSession::CanDismissOverlay() const {
    return overlayMode_ == OverlayMode::Help || overlayMode_ == OverlayMode::Victory;
}

bool InteractionSession::ShowContinueHint() const {
    return victoryOverlayShown_ && overlayMode_ != OverlayMode::GameOver;
}

bool InteractionSession::HasShownVictoryOverlay() const {
    return victoryOverlayShown_;
}

std::uint64_t InteractionSession::Seed() const {
    return seed_;
}

std::optional<Direction> InteractionSession::PendingMove() const {
    return pendingMove_;
}

void InteractionSession::SyncGameState(const InteractionInput& input) {
    if (!input.reached2048Ever) {
        victoryOverlayShown_ = false;
        if (overlayMode_ == OverlayMode::Victory) {
            overlayMode_ = OverlayMode::None;
        }
    }

    if (input.gameOver) {
        if (overlayMode_ != OverlayMode::GameOver) {
            overlayMode_ = OverlayMode::GameOver;
            ClearBufferedInput();
        }
        return;
    }

    if (overlayMode_ == OverlayMode::GameOver) {
        overlayMode_ = OverlayMode::None;
    }

    if (input.reached2048Ever && !victoryOverlayShown_ && overlayMode_ == OverlayMode::None) {
        victoryOverlayShown_ = true;
        overlayMode_ = OverlayMode::Victory;
        controlMode_ = ControlMode::Human;
        ClearBufferedInput();
    }
}

void InteractionSession::RecomputeInputGate(bool animationBlocksInput) {
    if (IsModalOverlay()) {
        inputGate_ = InputGate::BlockedByOverlay;
    } else if (animationBlocksInput) {
        inputGate_ = InputGate::BlockedByAnimation;
    } else {
        inputGate_ = InputGate::Accepting;
    }
}

void InteractionSession::ClearBufferedInput() {
    pendingMove_.reset();
    lastHeldDir_.reset();
    repeatDeadline_ = 0.0;
}

void InteractionSession::BufferMove(Direction direction) {
    pendingMove_ = direction;
}

std::optional<Direction> InteractionSession::ReleaseBufferedMove() {
    const auto pending = pendingMove_;
    pendingMove_.reset();
    return pending;
}

bool InteractionSession::IsModalOverlay() const {
    return overlayMode_ != OverlayMode::None;
}

}  // namespace game2048
