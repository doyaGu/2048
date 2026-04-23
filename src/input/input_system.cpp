#include "input/input_system.h"

namespace game2048 {

namespace {

bool Contains(const Rectangle& rect, const InputPoint& point) {
    return point.x >= rect.x && point.x <= rect.x + rect.width &&
           point.y >= rect.y && point.y <= rect.y + rect.height;
}

InputCommand CommandForControl(ControlId control) {
    switch (control) {
        case ControlId::Restart: return InputCommand::Reset;
        case ControlId::Undo: return InputCommand::Undo;
        case ControlId::ToggleAutoAI: return InputCommand::ToggleAutoAI;
        case ControlId::StepAI: return InputCommand::StepAI;
        case ControlId::CycleAgent: return InputCommand::CycleAgent;
        case ControlId::CycleAnimationSpeed: return InputCommand::CycleAnimationSpeed;
        case ControlId::ToggleHelp: return InputCommand::ToggleHelp;
        case ControlId::Exit: return InputCommand::Exit;
        default: return InputCommand::None;
    }
}

std::optional<Direction> MoveForControl(ControlId control) {
    switch (control) {
        case ControlId::MoveUp: return Direction::Up;
        case ControlId::MoveDown: return Direction::Down;
        case ControlId::MoveLeft: return Direction::Left;
        case ControlId::MoveRight: return Direction::Right;
        default: return std::nullopt;
    }
}

ControlId OverlayPointerControl(const RawPointerState& pointer, const LayoutMetrics& layout, OverlayMode overlayMode) {
    if (!pointer.connected || !pointer.pressed) {
        return ControlId::None;
    }
    for (std::size_t index = 0; index < OverlayActionCount(overlayMode); ++index) {
        if (!Contains(OverlayActionRect(layout, overlayMode, index), pointer.position)) {
            continue;
        }
        if (overlayMode == OverlayMode::GameOver) {
            return index == 0 ? ControlId::Restart : ControlId::Exit;
        }
        if (overlayMode == OverlayMode::Victory) {
            return index == 0 ? ControlId::Exit : ControlId::ToggleAutoAI;
        }
        if (overlayMode == OverlayMode::Help) {
            return ControlId::Exit;
        }
    }
    return ControlId::None;
}

}  // namespace

InputSystem::InputSystem()
    : gamepad_(DefaultGamepadBindings()) {}

InputFrame InputSystem::BuildFrame(const RawInputState& raw,
                                   const LayoutMetrics& layout,
                                   bool /*touchHudActive*/,
                                   bool /*animationBlocksInput*/,
                                   OverlayMode overlayMode) {
    InputFrame frame {};

    const ControlId overlayPointer = OverlayPointerControl(raw.pointers[0], layout, overlayMode);
    if (overlayPointer != ControlId::None) {
        frame.primaryControl = overlayPointer;
        frame.command = CommandForControl(overlayPointer);
        return frame;
    }

    const ControlId pointerControl = pointer_.HitTestControl(raw.pointers[0], layout);
    if (pointerControl != ControlId::None && raw.pointers[0].pressed) {
        frame.primaryControl = pointerControl;
        frame.command = CommandForControl(pointerControl);
        if (frame.command == InputCommand::None) {
            frame.pressedMove = MoveForControl(pointerControl);
        }
        if (frame.command != InputCommand::None || frame.pressedMove.has_value()) {
            return frame;
        }
    }

    ControlId gamepadPrimary = ControlId::None;
    InputCommand command = ResolveGamepadCommand(raw, overlayMode, gamepadPrimary);
    if (gamepadPrimary != ControlId::None) {
        frame.primaryControl = gamepadPrimary;
    }
    if (command == InputCommand::None) {
        command = ResolveKeyboardCommand(raw);
    }
    if (command != InputCommand::None) {
        frame.command = command;
        return frame;
    }

    pointer_.UpdateGesture(raw.pointers[0], layout, gesture_);
    if (const auto move = pointer_.ResolveMove(raw.pointers[0], layout, gesture_); move.has_value()) {
        frame.pressedMove = move;
        frame.primaryControl = ControlId::None;
        return frame;
    }

    if (const auto move = gamepad_.ResolvePressedMove(raw.gamepads[0]); move.has_value()) {
        frame.pressedMove = move;
        return frame;
    }

    if (const auto move = ResolveKeyboardPressedMove(raw); move.has_value()) {
        frame.pressedMove = move;
        return frame;
    }

    if (const auto move = gamepad_.ResolveHeldMove(raw.gamepads[0]); move.has_value()) {
        frame.heldMove = move;
        return frame;
    }

    frame.heldMove = ResolveKeyboardHeldMove(raw);
    return frame;
}

InputCommand InputSystem::ResolveKeyboardCommand(const RawInputState& raw) const {
    const auto& commands = raw.keyboard.pressedCommands;
    if (commands[static_cast<std::size_t>(RawCommandKeySlot::Reset)]) {
        return InputCommand::Reset;
    }
    if (commands[static_cast<std::size_t>(RawCommandKeySlot::Undo)]) {
        return InputCommand::Undo;
    }
    if (commands[static_cast<std::size_t>(RawCommandKeySlot::ToggleAutoAI)]) {
        return InputCommand::ToggleAutoAI;
    }
    if (commands[static_cast<std::size_t>(RawCommandKeySlot::StepAI)]) {
        return InputCommand::StepAI;
    }
    if (commands[static_cast<std::size_t>(RawCommandKeySlot::CycleAgent)]) {
        return InputCommand::CycleAgent;
    }
    if (commands[static_cast<std::size_t>(RawCommandKeySlot::CycleAnimationSpeed)]) {
        return InputCommand::CycleAnimationSpeed;
    }
    if (commands[static_cast<std::size_t>(RawCommandKeySlot::ToggleHelp)]) {
        return InputCommand::ToggleHelp;
    }
    if (commands[static_cast<std::size_t>(RawCommandKeySlot::Exit)]) {
        return InputCommand::Exit;
    }
    return InputCommand::None;
}

std::optional<Direction> InputSystem::ResolveKeyboardPressedMove(const RawInputState& raw) const {
    const auto& pressed = raw.keyboard.pressedDirections;
    if (pressed[static_cast<std::size_t>(RawDirectionKeySlot::UpArrow)] ||
        pressed[static_cast<std::size_t>(RawDirectionKeySlot::W)]) {
        return Direction::Up;
    }
    if (pressed[static_cast<std::size_t>(RawDirectionKeySlot::DownArrow)] ||
        pressed[static_cast<std::size_t>(RawDirectionKeySlot::S)]) {
        return Direction::Down;
    }
    if (pressed[static_cast<std::size_t>(RawDirectionKeySlot::LeftArrow)] ||
        pressed[static_cast<std::size_t>(RawDirectionKeySlot::A)]) {
        return Direction::Left;
    }
    if (pressed[static_cast<std::size_t>(RawDirectionKeySlot::RightArrow)] ||
        pressed[static_cast<std::size_t>(RawDirectionKeySlot::D)]) {
        return Direction::Right;
    }
    return std::nullopt;
}

std::optional<Direction> InputSystem::ResolveKeyboardHeldMove(const RawInputState& raw) const {
    const auto& held = raw.keyboard.heldDirections;
    if (held[static_cast<std::size_t>(RawDirectionKeySlot::UpArrow)] ||
        held[static_cast<std::size_t>(RawDirectionKeySlot::W)]) {
        return Direction::Up;
    }
    if (held[static_cast<std::size_t>(RawDirectionKeySlot::DownArrow)] ||
        held[static_cast<std::size_t>(RawDirectionKeySlot::S)]) {
        return Direction::Down;
    }
    if (held[static_cast<std::size_t>(RawDirectionKeySlot::LeftArrow)] ||
        held[static_cast<std::size_t>(RawDirectionKeySlot::A)]) {
        return Direction::Left;
    }
    if (held[static_cast<std::size_t>(RawDirectionKeySlot::RightArrow)] ||
        held[static_cast<std::size_t>(RawDirectionKeySlot::D)]) {
        return Direction::Right;
    }
    return std::nullopt;
}

InputCommand InputSystem::ResolveGamepadCommand(const RawInputState& raw,
                                                OverlayMode overlayMode,
                                                ControlId& primaryControl) const {
    const auto& state = raw.gamepads[0];
    if (!state.connected) {
        return InputCommand::None;
    }

    const auto bindings = DefaultGamepadBindings();
    if (overlayMode == OverlayMode::Help) {
        if (state.pressed[static_cast<std::size_t>(bindings.faceBottom)] ||
            state.pressed[static_cast<std::size_t>(bindings.faceRight)] ||
            state.pressed[static_cast<std::size_t>(bindings.back)] ||
            state.pressed[static_cast<std::size_t>(bindings.start)]) {
            primaryControl = ControlId::OverlayPrimary;
            return InputCommand::Exit;
        }
        return InputCommand::None;
    } else if (overlayMode == OverlayMode::Victory) {
        if (state.pressed[static_cast<std::size_t>(bindings.faceBottom)] ||
            state.pressed[static_cast<std::size_t>(bindings.faceRight)]) {
            primaryControl = ControlId::OverlayPrimary;
            return InputCommand::Exit;
        }
        if (state.pressed[static_cast<std::size_t>(bindings.back)]) {
            primaryControl = ControlId::ToggleAutoAI;
            return InputCommand::ToggleAutoAI;
        }
        return InputCommand::None;
    } else if (overlayMode == OverlayMode::GameOver) {
        if (state.pressed[static_cast<std::size_t>(bindings.faceBottom)] ||
            state.pressed[static_cast<std::size_t>(bindings.faceTop)]) {
            primaryControl = ControlId::Restart;
            return InputCommand::Reset;
        }
        if (state.pressed[static_cast<std::size_t>(bindings.faceRight)] ||
            state.pressed[static_cast<std::size_t>(bindings.back)]) {
            primaryControl = ControlId::Exit;
            return InputCommand::Exit;
        }
        return InputCommand::None;
    } else if (state.pressed[static_cast<std::size_t>(bindings.faceBottom)]) {
        primaryControl = ControlId::StepAI;
        return InputCommand::StepAI;
    }

    return gamepad_.ResolveCommand(state);
}

}  // namespace game2048
