#pragma once

#include <array>

#include <raylib.h>

#include "config.h"

namespace game2048 {

struct LayoutMetrics {
    Rectangle boardRect {};
    Rectangle panelRect {};
    Rectangle topBarRect {};
    float tileGap = 0.0F;
    float tileSize = 0.0F;
    std::array<Rectangle, kCellCount> tileRects {};
};

LayoutMetrics ComputeLayout(int screenWidth, int screenHeight);

}  // namespace game2048
