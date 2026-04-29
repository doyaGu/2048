#pragma once

#include <array>

#include "core/board.h"

namespace game2048::ai {

inline constexpr std::array<Direction, 4> kTdlMoveDirections {
    Direction::Up,
    Direction::Right,
    Direction::Down,
    Direction::Left,
};

}  // namespace game2048::ai
