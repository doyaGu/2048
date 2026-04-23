#pragma once

#include <array>

#include <raylib.h>

#include "config.h"
#include "input_types.h"

namespace game2048 {

struct LayoutMetrics {
    Rectangle clusterRect {};
    Rectangle boardRect {};
    Rectangle boardGestureRect {};
    Rectangle panelRect {};
    Rectangle topBarRect {};
    Rectangle topBarTitleRect {};
    std::array<Rectangle, 2> scoreBoxRects {};
    bool topBarUsesTwoRows = false;
    float tileGap = 0.0F;
    float tileSize = 0.0F;
    std::array<Rectangle, kCellCount> tileRects {};
    std::array<Rectangle, 16> controlRects {};
    std::array<ControlId, 16> controlIds {};
    int controlCount = 0;
    std::array<Rectangle, 3> overlayActionRects {};
    int overlayActionCount = 0;
    bool showTouchHud = false;
};

LayoutMetrics ComputeLayout(int screenWidth, int screenHeight, bool showTouchHud = false);

}  // namespace game2048
