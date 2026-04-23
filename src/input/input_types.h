#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "core/board.h"
#include "input/input.h"

namespace game2048 {

enum class RawDirectionKeySlot : std::size_t {
    UpArrow = 0,
    DownArrow,
    LeftArrow,
    RightArrow,
    W,
    S,
    A,
    D,
    Count
};

enum class RawCommandKeySlot : std::size_t {
    Reset = 0,
    Undo,
    ToggleAutoAI,
    StepAI,
    CycleAgent,
    CycleAnimationSpeed,
    ToggleHelp,
    Exit,
    Count
};

enum class ControlId {
    None,
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    Restart,
    Undo,
    ToggleAutoAI,
    StepAI,
    CycleAgent,
    CycleAnimationSpeed,
    ToggleHelp,
    Exit,
    OverlayPrimary,
    OverlaySecondary
};

struct InputPoint {
    float x = 0.0F;
    float y = 0.0F;
};

struct RawPointerState {
    bool connected = false;
    bool isTouch = false;
    bool down = false;
    bool pressed = false;
    bool released = false;
    InputPoint position {};
    std::uint64_t pointerId = 0;
};

struct RawGamepadState {
    bool connected = false;
    std::array<bool, 16> pressed {};
    std::array<bool, 16> down {};
    float leftX = 0.0F;
    float leftY = 0.0F;
};

struct RawKeyboardState {
    std::array<bool, static_cast<std::size_t>(RawDirectionKeySlot::Count)> pressedDirections {};
    std::array<bool, static_cast<std::size_t>(RawDirectionKeySlot::Count)> heldDirections {};
    std::array<bool, static_cast<std::size_t>(RawCommandKeySlot::Count)> pressedCommands {};
};

struct RawInputState {
    RawKeyboardState keyboard {};
    std::array<RawPointerState, 8> pointers {};
    std::array<RawGamepadState, 4> gamepads {};
    double nowSeconds = 0.0;
};

struct InputFrame {
    InputCommand command = InputCommand::None;
    std::optional<Direction> pressedMove {};
    std::optional<Direction> heldMove {};
    ControlId primaryControl = ControlId::None;
};

}  // namespace game2048
