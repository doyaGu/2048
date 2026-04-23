#pragma once

#include <array>
#include <cstddef>

#include <raylib.h>

#include "core/config.h"
#include "input/input_types.h"
#include "app/session_types.h"

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
    std::size_t controlCount = 0;
    std::array<Rectangle, 3> overlayActionRects {};
    std::size_t overlayActionCount = 0;
    bool showTouchHud = false;
};

LayoutMetrics ComputeLayout(int screenWidth, int screenHeight, bool showTouchHud = false);
std::size_t OverlayActionCount(OverlayMode overlayMode);
Rectangle OverlayActionRect(const LayoutMetrics& layout, OverlayMode overlayMode, std::size_t index);

}  // namespace game2048
