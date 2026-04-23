#include "input/gamepad_input.h"

#include <cmath>

namespace game2048 {

namespace {

constexpr float kStickThreshold = 0.60F;
constexpr float kStickReleaseThreshold = 0.30F;

std::optional<Direction> ResolveStickDirection(const RawGamepadState& state) {
    if (std::fabs(state.leftX) >= std::fabs(state.leftY) && std::fabs(state.leftX) >= kStickThreshold) {
        return state.leftX >= 0.0F ? Direction::Right : Direction::Left;
    }
    if (std::fabs(state.leftY) >= kStickThreshold) {
        return state.leftY >= 0.0F ? Direction::Down : Direction::Up;
    }
    return std::nullopt;
}

bool StickCentered(const RawGamepadState& state) {
    return std::fabs(state.leftX) <= kStickReleaseThreshold && std::fabs(state.leftY) <= kStickReleaseThreshold;
}

}  // namespace

GamepadInputRouter::GamepadInputRouter(GamepadBindingMap bindings)
    : bindings_(bindings) {}

std::optional<Direction> GamepadInputRouter::ResolvePressedMove(const RawGamepadState& state) {
    if (!state.connected) {
        stickReady_ = true;
        return std::nullopt;
    }

    if (state.pressed[static_cast<std::size_t>(bindings_.dpadUp)]) {
        return Direction::Up;
    }
    if (state.pressed[static_cast<std::size_t>(bindings_.dpadDown)]) {
        return Direction::Down;
    }
    if (state.pressed[static_cast<std::size_t>(bindings_.dpadLeft)]) {
        return Direction::Left;
    }
    if (state.pressed[static_cast<std::size_t>(bindings_.dpadRight)]) {
        return Direction::Right;
    }

    if (!stickReady_) {
        if (StickCentered(state)) {
            stickReady_ = true;
        }
        return std::nullopt;
    }

    const auto direction = ResolveStickDirection(state);
    if (direction.has_value()) {
        stickReady_ = false;
    }
    return direction;
}

std::optional<Direction> GamepadInputRouter::ResolveHeldMove(const RawGamepadState& state) const {
    if (!state.connected) {
        return std::nullopt;
    }
    return ResolveStickDirection(state);
}

InputCommand GamepadInputRouter::ResolveCommand(const RawGamepadState& state) const {
    if (!state.connected) {
        return InputCommand::None;
    }

    if (state.pressed[static_cast<std::size_t>(bindings_.start)]) {
        return InputCommand::ToggleHelp;
    }
    if (state.pressed[static_cast<std::size_t>(bindings_.back)]) {
        return InputCommand::ToggleAutoAI;
    }
    if (state.pressed[static_cast<std::size_t>(bindings_.faceLeft)]) {
        return InputCommand::Undo;
    }
    if (state.pressed[static_cast<std::size_t>(bindings_.faceTop)]) {
        return InputCommand::Reset;
    }
    if (state.pressed[static_cast<std::size_t>(bindings_.leftShoulder)]) {
        return InputCommand::CycleAgent;
    }
    if (state.pressed[static_cast<std::size_t>(bindings_.rightShoulder)]) {
        return InputCommand::CycleAnimationSpeed;
    }
    return InputCommand::None;
}

}  // namespace game2048
