#include "gui/runtime_event_mapper.h"

namespace game2048 {

std::vector<RuntimeEvent> RuntimeEventMapper::BuildEvents(const InputFrame& frame,
                                                          OverlayMode overlayMode,
                                                          double nowSeconds) {
    std::vector<RuntimeEvent> events;
    if (const auto commandEvent = EventForCommand(frame.command, overlayMode); commandEvent.has_value()) {
        events.push_back(*commandEvent);
        lastHeldDir_.reset();
        repeatDeadline_ = 0.0;
        return events;
    }

    if (frame.pressedMove.has_value()) {
        events.push_back({RuntimeEventType::Move, *frame.pressedMove});
        lastHeldDir_ = frame.pressedMove;
        repeatDeadline_ = nowSeconds + kRepeatDelaySeconds;
        return events;
    }

    if (frame.heldMove != lastHeldDir_) {
        lastHeldDir_ = frame.heldMove;
        repeatDeadline_ = nowSeconds + kRepeatDelaySeconds;
        return events;
    }

    if (frame.heldMove.has_value() && nowSeconds >= repeatDeadline_) {
        events.push_back({RuntimeEventType::Move, *frame.heldMove});
        repeatDeadline_ = nowSeconds + kRepeatPeriodSeconds;
        return events;
    }

    return events;
}

std::optional<RuntimeEvent> RuntimeEventMapper::EventForCommand(InputCommand command, OverlayMode overlayMode) const {
    switch (command) {
        case InputCommand::Reset:
            return RuntimeEvent {RuntimeEventType::Reset, Direction::Left};
        case InputCommand::Undo:
            return RuntimeEvent {RuntimeEventType::Undo, Direction::Left};
        case InputCommand::ToggleAutoAI:
            return RuntimeEvent {RuntimeEventType::ToggleAutoplay, Direction::Left};
        case InputCommand::StepAI:
            return RuntimeEvent {RuntimeEventType::StepAI, Direction::Left};
        case InputCommand::CycleAgent:
            return RuntimeEvent {RuntimeEventType::CycleAgent, Direction::Left};
        case InputCommand::CycleAnimationSpeed:
            return RuntimeEvent {RuntimeEventType::CycleAnimation, Direction::Left};
        case InputCommand::ToggleHelp:
            return overlayMode == OverlayMode::Help
                ? RuntimeEvent {RuntimeEventType::CloseOverlay, Direction::Left}
                : RuntimeEvent {RuntimeEventType::OpenHelp, Direction::Left};
        case InputCommand::Exit:
            return (overlayMode == OverlayMode::Help || overlayMode == OverlayMode::Victory)
                ? RuntimeEvent {RuntimeEventType::CloseOverlay, Direction::Left}
                : RuntimeEvent {RuntimeEventType::Quit, Direction::Left};
        case InputCommand::None:
            return std::nullopt;
    }
    return std::nullopt;
}

}  // namespace game2048
