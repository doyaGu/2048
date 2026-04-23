#pragma once

#include <optional>

#include "core/board.h"

namespace game2048 {

enum class InputCommand {
    None,
    Reset,
    Undo,
    ToggleAutoAI,
    StepAI,
    CycleAgent,
    CycleAnimationSpeed,
    ToggleHelp,
    Exit
};

std::optional<Direction> PollMoveInput();
std::optional<Direction> PollMoveInputHeld();   // IsKeyDown — for key-repeat logic
InputCommand PollCommandInput();

}  // namespace game2048
