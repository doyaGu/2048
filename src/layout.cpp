#include "layout.h"

#include <algorithm>

namespace game2048 {

namespace {

constexpr float kOuterPadding = 28.0F;
constexpr float kTopBandGap = 16.0F;
constexpr float kControlGap = 14.0F;
constexpr float kDesktopControlBandHeight = 62.0F;
constexpr float kTouchControlBandHeight = 164.0F;

float ComputeTitleFontSize(float topBarHeight) {
    return std::clamp(topBarHeight * 0.70F, 20.0F, 42.0F);
}

void AddControl(LayoutMetrics& layout, Rectangle rect, ControlId id) {
    if (layout.controlCount >= static_cast<int>(layout.controlRects.size())) {
        return;
    }
    layout.controlRects[static_cast<std::size_t>(layout.controlCount)] = rect;
    layout.controlIds[static_cast<std::size_t>(layout.controlCount)] = id;
    ++layout.controlCount;
}

void AddControlRow(LayoutMetrics& layout,
                   float startX,
                   float y,
                   float buttonWidth,
                   float buttonHeight,
                   float gap,
                   std::initializer_list<ControlId> ids) {
    float x = startX;
    for (const auto id : ids) {
        AddControl(layout, {x, y, buttonWidth, buttonHeight}, id);
        x += buttonWidth + gap;
    }
}

}  // namespace

LayoutMetrics ComputeLayout(int screenWidth, int screenHeight, bool showTouchHud) {
    LayoutMetrics layout {};
    const float contentLeft = kOuterPadding;
    const float contentTop = kOuterPadding;
    const float contentWidth = static_cast<float>(screenWidth) - kOuterPadding * 2.0F;
    const float contentHeight = static_cast<float>(screenHeight) - kOuterPadding * 2.0F;
    const float controlBandHeight = showTouchHud ? kTouchControlBandHeight : kDesktopControlBandHeight;
    const float interColumnGap = std::clamp(contentWidth * 0.024F, 18.0F, 28.0F);
    const float panelWidth = std::clamp(contentWidth * 0.34F, 300.0F, 360.0F);
    const float topBarHeight = 56.0F;
    const float maxBoardByHeight = std::max(220.0F, contentHeight - topBarHeight - kTopBandGap - controlBandHeight);
    const float boardSize = std::max(220.0F, std::min(maxBoardByHeight, contentWidth - interColumnGap - panelWidth));
    const float clusterWidth = boardSize + interColumnGap + panelWidth;
    const float clusterX = contentLeft + (contentWidth - clusterWidth) * 0.5F;
    const float boardY = contentTop + topBarHeight + kTopBandGap;

    layout.clusterRect = {clusterX, contentTop, clusterWidth, topBarHeight + kTopBandGap + boardSize + controlBandHeight};
    layout.boardRect = {clusterX, boardY, boardSize, boardSize};
    layout.boardGestureRect = showTouchHud
        ? Rectangle{layout.boardRect.x + 12.0F, layout.boardRect.y + 12.0F, layout.boardRect.width - 24.0F, layout.boardRect.height - 72.0F}
        : layout.boardRect;
    layout.panelRect = {layout.boardRect.x + boardSize + interColumnGap, boardY, panelWidth, boardSize};
    layout.topBarRect = {clusterX, contentTop, clusterWidth, topBarHeight};
    layout.showTouchHud = showTouchHud;

    const float scoreGap = 6.0F;
    const float scoreWidth = std::clamp(clusterWidth * 0.11F, 86.0F, 118.0F);
    const float scoreHeight = layout.topBarRect.height - 10.0F;
    const float scoreGroupWidth = scoreWidth * 2.0F + scoreGap;
    const int titleFs = static_cast<int>(ComputeTitleFontSize(layout.topBarRect.height));
    const float titleWidth = static_cast<float>(MeasureText("2048 Engine", titleFs));
    const float minInternalGap = 12.0F;
    layout.topBarUsesTwoRows = titleWidth + minInternalGap + scoreGroupWidth > clusterWidth || clusterWidth < 860.0F;

    if (!layout.topBarUsesTwoRows) {
        layout.topBarTitleRect = {
            layout.topBarRect.x,
            layout.topBarRect.y,
            std::max(80.0F, clusterWidth - scoreGroupWidth - minInternalGap),
            layout.topBarRect.height
        };
        layout.scoreBoxRects[0] = {
            layout.topBarRect.x + layout.topBarRect.width - scoreGroupWidth,
            layout.topBarRect.y + 5.0F,
            scoreWidth,
            scoreHeight
        };
        layout.scoreBoxRects[1] = {
            layout.scoreBoxRects[0].x + scoreWidth + scoreGap,
            layout.topBarRect.y + 5.0F,
            scoreWidth,
            scoreHeight
        };
    } else {
        layout.topBarRect.height = 88.0F;
        layout.clusterRect.height = layout.topBarRect.height + kTopBandGap + boardSize + controlBandHeight;
        layout.topBarTitleRect = {
            layout.topBarRect.x,
            layout.topBarRect.y + scoreHeight + 10.0F,
            layout.topBarRect.width,
            28.0F
        };
        layout.scoreBoxRects[0] = {
            layout.topBarRect.x + layout.topBarRect.width - scoreGroupWidth,
            layout.topBarRect.y + 5.0F,
            scoreWidth,
            scoreHeight
        };
        layout.scoreBoxRects[1] = {
            layout.scoreBoxRects[0].x + scoreWidth + scoreGap,
            layout.topBarRect.y + 5.0F,
            scoreWidth,
            scoreHeight
        };
        layout.boardRect.y = contentTop + layout.topBarRect.height + kTopBandGap;
        layout.panelRect.y = layout.boardRect.y;
        layout.boardGestureRect = showTouchHud
            ? Rectangle{layout.boardRect.x + 12.0F, layout.boardRect.y + 12.0F, layout.boardRect.width - 24.0F, layout.boardRect.height - 72.0F}
            : layout.boardRect;
    }

    layout.tileGap = std::clamp(boardSize * 0.03F, 10.0F, 18.0F);
    layout.tileSize = (boardSize - layout.tileGap * (kBoardSize + 1)) / static_cast<float>(kBoardSize);

    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            const float x = layout.boardRect.x + layout.tileGap +
                            static_cast<float>(col) * (layout.tileSize + layout.tileGap);
            const float y = layout.boardRect.y + layout.tileGap +
                            static_cast<float>(row) * (layout.tileSize + layout.tileGap);
            layout.tileRects[row * kBoardSize + col] = {x, y, layout.tileSize, layout.tileSize};
        }
    }

    const float bandY = layout.boardRect.y + layout.boardRect.height + kControlGap;
    const float gap = 10.0F;

    if (showTouchHud) {
        const float topWidth = (layout.boardRect.width - gap * 5.0F) / 6.0F;
        const float bottomWidth = (layout.boardRect.width - gap * 4.0F) / 5.0F;
        const float buttonHeight = 62.0F;
        AddControlRow(layout, layout.boardRect.x, bandY, topWidth, buttonHeight, gap,
                      {ControlId::MoveUp, ControlId::MoveLeft, ControlId::MoveDown, ControlId::MoveRight, ControlId::Restart, ControlId::Undo});
        AddControlRow(layout, layout.boardRect.x, bandY + buttonHeight + gap, bottomWidth, buttonHeight, gap,
                      {ControlId::ToggleAutoAI, ControlId::StepAI, ControlId::ToggleHelp, ControlId::CycleAgent, ControlId::CycleAnimationSpeed});
    } else {
        const float buttonWidth = (layout.boardRect.width - gap * 3.0F) / 4.0F;
        const float buttonHeight = 48.0F;
        AddControlRow(layout, layout.boardRect.x, bandY, buttonWidth, buttonHeight, gap,
                      {ControlId::Restart, ControlId::Undo, ControlId::ToggleAutoAI, ControlId::ToggleHelp});
    }

    const float overlayButtonWidth = std::min(160.0F, layout.boardRect.width * 0.32F);
    const float overlayButtonHeight = 44.0F;
    const float overlayGap = 16.0F;
    const float overlayY = layout.boardRect.y + layout.boardRect.height * 0.68F;
    const float totalOverlayWidth = overlayButtonWidth * 2.0F + overlayGap;
    const float overlayX = layout.boardRect.x + (layout.boardRect.width - totalOverlayWidth) * 0.5F;
    layout.overlayActionCount = 2;
    layout.overlayActionRects[0] = {overlayX, overlayY, overlayButtonWidth, overlayButtonHeight};
    layout.overlayActionRects[1] = {overlayX + overlayButtonWidth + overlayGap, overlayY, overlayButtonWidth, overlayButtonHeight};

    return layout;
}

}  // namespace game2048
