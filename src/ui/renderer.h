#pragma once

#include "ui/animation.h"
#include "core/board.h"
#include "ui/layout.h"

namespace game2048 {

class Renderer {
public:
    void DrawBoard(const LayoutMetrics& layout, const Board& board, const AnimationController& animation) const;
};

}  // namespace game2048
