#include "layout.h"

#include <algorithm>

namespace game2048 {

LayoutMetrics ComputeLayout(int screenWidth, int screenHeight) {
    LayoutMetrics layout;
    const float padding = 28.0F;
    const float contentWidth = static_cast<float>(screenWidth) - padding * 2.0F;
    const float contentHeight = static_cast<float>(screenHeight) - padding * 2.0F;
    const float panelWidth = std::clamp(contentWidth * 0.30F, 280.0F, 380.0F);
    const float boardSize = std::min(contentHeight - 80.0F, contentWidth - panelWidth - 24.0F);

    layout.boardRect = {padding, padding + 72.0F, boardSize, boardSize};
    layout.panelRect = {layout.boardRect.x + boardSize + 24.0F, padding + 72.0F, panelWidth, boardSize};
    layout.topBarRect = {padding, padding, static_cast<float>(screenWidth) - padding * 2.0F, 56.0F};
    layout.tileGap = std::clamp(boardSize * 0.03F, 10.0F, 18.0F);
    layout.tileSize = (boardSize - layout.tileGap * (kBoardSize + 1)) / static_cast<float>(kBoardSize);

    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            const float x = layout.boardRect.x + layout.tileGap + col * (layout.tileSize + layout.tileGap);
            const float y = layout.boardRect.y + layout.tileGap + row * (layout.tileSize + layout.tileGap);
            layout.tileRects[row * kBoardSize + col] = {x, y, layout.tileSize, layout.tileSize};
        }
    }

    return layout;
}

}  // namespace game2048
