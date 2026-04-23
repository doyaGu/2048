#include "input/input_source.h"

#include <raylib.h>

namespace game2048 {

namespace {

void SetDirectionPressed(RawKeyboardState& keyboard, RawDirectionKeySlot slot, bool pressed, bool held) {
    keyboard.pressedDirections[static_cast<std::size_t>(slot)] = pressed;
    keyboard.heldDirections[static_cast<std::size_t>(slot)] = held;
}

void SetCommandPressed(RawKeyboardState& keyboard, RawCommandKeySlot slot, bool pressed) {
    keyboard.pressedCommands[static_cast<std::size_t>(slot)] = pressed;
}

}  // namespace

RawInputState InputSource::Poll() {
    RawInputState state {};
    state.nowSeconds = GetTime();

    SetDirectionPressed(state.keyboard, RawDirectionKeySlot::UpArrow, IsKeyPressed(KEY_UP), IsKeyDown(KEY_UP));
    SetDirectionPressed(state.keyboard, RawDirectionKeySlot::DownArrow, IsKeyPressed(KEY_DOWN), IsKeyDown(KEY_DOWN));
    SetDirectionPressed(state.keyboard, RawDirectionKeySlot::LeftArrow, IsKeyPressed(KEY_LEFT), IsKeyDown(KEY_LEFT));
    SetDirectionPressed(state.keyboard, RawDirectionKeySlot::RightArrow, IsKeyPressed(KEY_RIGHT), IsKeyDown(KEY_RIGHT));
    SetDirectionPressed(state.keyboard, RawDirectionKeySlot::W, IsKeyPressed(KEY_W), IsKeyDown(KEY_W));
    SetDirectionPressed(state.keyboard, RawDirectionKeySlot::S, IsKeyPressed(KEY_S), IsKeyDown(KEY_S));
    SetDirectionPressed(state.keyboard, RawDirectionKeySlot::A, IsKeyPressed(KEY_A), IsKeyDown(KEY_A));
    SetDirectionPressed(state.keyboard, RawDirectionKeySlot::D, IsKeyPressed(KEY_D), IsKeyDown(KEY_D));

    SetCommandPressed(state.keyboard, RawCommandKeySlot::Reset, IsKeyPressed(KEY_R));
    SetCommandPressed(state.keyboard, RawCommandKeySlot::Undo, IsKeyPressed(KEY_U));
    SetCommandPressed(state.keyboard, RawCommandKeySlot::ToggleAutoAI, IsKeyPressed(KEY_SPACE));
    SetCommandPressed(state.keyboard, RawCommandKeySlot::StepAI, IsKeyPressed(KEY_N));
    SetCommandPressed(state.keyboard, RawCommandKeySlot::CycleAgent, IsKeyPressed(KEY_TAB));
    SetCommandPressed(state.keyboard, RawCommandKeySlot::CycleAnimationSpeed, IsKeyPressed(KEY_T));
    SetCommandPressed(state.keyboard, RawCommandKeySlot::ToggleHelp, IsKeyPressed(KEY_H) || IsKeyPressed(KEY_F1));
    SetCommandPressed(state.keyboard, RawCommandKeySlot::Exit, IsKeyPressed(KEY_ESCAPE));

    const int touchCount = GetTouchPointCount();
    if (touchCount > 0) {
        state.pointers[0].connected = true;
        state.pointers[0].isTouch = true;
        state.pointers[0].down = true;
        state.pointers[0].pressed = !previousTouchDown_;
        state.pointers[0].released = false;
        state.pointers[0].position = {GetTouchPosition(0).x, GetTouchPosition(0).y};
        state.pointers[0].pointerId = 1;
        previousTouchDown_ = true;
        previousTouchPosition_ = state.pointers[0].position;
    } else if (previousTouchDown_) {
        state.pointers[0].connected = true;
        state.pointers[0].isTouch = true;
        state.pointers[0].released = true;
        state.pointers[0].position = previousTouchPosition_;
        state.pointers[0].pointerId = 1;
        previousTouchDown_ = false;
    } else {
        state.pointers[0].connected = true;
        state.pointers[0].isTouch = false;
        state.pointers[0].down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        state.pointers[0].pressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        state.pointers[0].released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
        state.pointers[0].position = {static_cast<float>(GetMouseX()), static_cast<float>(GetMouseY())};
        state.pointers[0].pointerId = 0;
    }

    const int gamepad = 0;
    state.gamepads[0].connected = IsGamepadAvailable(gamepad);
    if (state.gamepads[0].connected) {
        for (int button = 0; button < static_cast<int>(state.gamepads[0].pressed.size()); ++button) {
            state.gamepads[0].pressed[static_cast<std::size_t>(button)] = IsGamepadButtonPressed(gamepad, button);
            state.gamepads[0].down[static_cast<std::size_t>(button)] = IsGamepadButtonDown(gamepad, button);
        }
        state.gamepads[0].leftX = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_X);
        state.gamepads[0].leftY = GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_Y);
    }

    return state;
}

}  // namespace game2048
