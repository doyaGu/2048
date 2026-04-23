#pragma once

#include "animation.h"
#include "board.h"
#include "layout.h"

namespace game2048 {

class Renderer {
public:
    void DrawBoard(const LayoutMetrics& layout, const Board& board, const AnimationController& animation) const;
};

}  // namespace game2048
