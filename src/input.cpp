#include "input.h"

#include <raylib.h>

namespace game2048 {

std::optional<Direction> PollMoveInput() {
    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) {
        return Direction::Up;
    }
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) {
        return Direction::Down;
    }
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) {
        return Direction::Left;
    }
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) {
        return Direction::Right;
    }
    return std::nullopt;
}

std::optional<Direction> PollMoveInputHeld() {
    if (IsKeyDown(KEY_UP)    || IsKeyDown(KEY_W)) return Direction::Up;
    if (IsKeyDown(KEY_DOWN)  || IsKeyDown(KEY_S)) return Direction::Down;
    if (IsKeyDown(KEY_LEFT)  || IsKeyDown(KEY_A)) return Direction::Left;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) return Direction::Right;
    return std::nullopt;
}

InputCommand PollCommandInput() {
    if (IsKeyPressed(KEY_R)) {
        return InputCommand::Reset;
    }
    if (IsKeyPressed(KEY_U)) {
        return InputCommand::Undo;
    }
    if (IsKeyPressed(KEY_SPACE)) {
        return InputCommand::ToggleAutoAI;
    }
    if (IsKeyPressed(KEY_N)) {
        return InputCommand::StepAI;
    }
    if (IsKeyPressed(KEY_TAB)) {
        return InputCommand::CycleAgent;
    }
    if (IsKeyPressed(KEY_T)) {
        return InputCommand::CycleAnimationSpeed;
    }
    if (IsKeyPressed(KEY_H) || IsKeyPressed(KEY_F1)) {
        return InputCommand::ToggleHelp;
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        return InputCommand::Exit;
    }
    return InputCommand::None;
}

}  // namespace game2048
