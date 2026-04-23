#include "layout.h"

#include <algorithm>

namespace game2048 {

namespace {

constexpr float kOuterPadding = 28.0F;
constexpr float kTopBandGap = 16.0F;
constexpr float kControlGap = 14.0F;
constexpr float kDesktopControlBandHeight = 62.0F;
constexpr float kTouchControlBandHeight = 164.0F;
constexpr float kSingleRowTopBarHeight = 56.0F;
constexpr float kTwoRowTopBarHeight = 88.0F;

float ComputeTitleFontSize(float topBarHeight) {
    return std::clamp(topBarHeight * 0.70F, 20.0F, 42.0F);
}

float ComputePanelWidth(float contentWidth, float interColumnGap) {
    const float maxWidth = std::max(0.0F, std::min(360.0F, contentWidth - interColumnGap));
    const float minWidth = std::min(maxWidth, std::min(300.0F, std::max(96.0F, contentWidth * 0.25F)));
    const float preferredWidth = std::min(contentWidth * 0.34F, maxWidth);
    if (maxWidth <= 0.0F) {
        return 0.0F;
    }
    return std::clamp(preferredWidth, minWidth, maxWidth);
}

float ComputeScoreWidth(float clusterWidth, float scoreGap) {
    float scoreWidth = std::clamp(clusterWidth * 0.11F, 48.0F, 118.0F);
    scoreWidth = std::min(scoreWidth, std::max(0.0F, (clusterWidth - scoreGap) * 0.5F));
    return scoreWidth;
}

Rectangle MakeBoardGestureRect(const Rectangle& boardRect, bool showTouchHud) {
    if (!showTouchHud) {
        return boardRect;
    }

    return {
        boardRect.x + 12.0F,
        boardRect.y + 12.0F,
        std::max(0.0F, boardRect.width - 24.0F),
        std::max(0.0F, boardRect.height - 72.0F)
    };
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
    const float interColumnGap = std::clamp(contentWidth * 0.024F, 8.0F, 28.0F);
    const float panelWidth = ComputePanelWidth(contentWidth, interColumnGap);
    auto computeBoardSize = [&](float topBarHeight) {
        const float maxBoardByHeight = std::max(0.0F, contentHeight - topBarHeight - kTopBandGap - controlBandHeight);
        const float maxBoardByWidth = std::max(0.0F, contentWidth - interColumnGap - panelWidth);
        return std::min(maxBoardByHeight, maxBoardByWidth);
    };

    float boardSize = computeBoardSize(kSingleRowTopBarHeight);
    float clusterWidth = boardSize + interColumnGap + panelWidth;
    float topBarHeight = kSingleRowTopBarHeight;
    const float scoreGap = 6.0F;
    float scoreWidth = ComputeScoreWidth(clusterWidth, scoreGap);
    const float scoreGroupWidth = scoreWidth * 2.0F + scoreGap;
    const int titleFs = static_cast<int>(ComputeTitleFontSize(topBarHeight));
    const float titleWidth = static_cast<float>(MeasureText("2048 Engine", titleFs));
    const float minInternalGap = 12.0F;
    const bool topBarUsesTwoRows = titleWidth + minInternalGap + scoreGroupWidth > clusterWidth || clusterWidth < 860.0F;
    if (topBarUsesTwoRows) {
        topBarHeight = kTwoRowTopBarHeight;
        boardSize = computeBoardSize(topBarHeight);
        clusterWidth = boardSize + interColumnGap + panelWidth;
        scoreWidth = ComputeScoreWidth(clusterWidth, scoreGap);
    }

    const float clusterX = contentLeft + (contentWidth - clusterWidth) * 0.5F;
    const float boardY = contentTop + topBarHeight + kTopBandGap;

    layout.clusterRect = {clusterX, contentTop, clusterWidth, topBarHeight + kTopBandGap + boardSize + controlBandHeight};
    layout.boardRect = {clusterX, boardY, boardSize, boardSize};
    layout.boardGestureRect = MakeBoardGestureRect(layout.boardRect, showTouchHud);
    layout.panelRect = {layout.boardRect.x + boardSize + interColumnGap, boardY, panelWidth, boardSize};
    layout.topBarRect = {clusterX, contentTop, clusterWidth, topBarHeight};
    layout.showTouchHud = showTouchHud;

    const float scoreHeight = std::min(layout.topBarRect.height - 10.0F, 46.0F);
    const float actualScoreGroupWidth = scoreWidth * 2.0F + scoreGap;
    layout.topBarUsesTwoRows = topBarUsesTwoRows;

    if (!layout.topBarUsesTwoRows) {
        layout.topBarTitleRect = {
            layout.topBarRect.x,
            layout.topBarRect.y,
            std::max(0.0F, clusterWidth - actualScoreGroupWidth - minInternalGap),
            layout.topBarRect.height
        };
        layout.scoreBoxRects[0] = {
            layout.topBarRect.x + layout.topBarRect.width - actualScoreGroupWidth,
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
        layout.topBarRect.height = kTwoRowTopBarHeight;
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
        layout.boardGestureRect = MakeBoardGestureRect(layout.boardRect, showTouchHud);
    }

    const float rawTileGap = std::clamp(boardSize * 0.03F, 2.0F, 18.0F);
    layout.tileGap = std::min(rawTileGap, boardSize / static_cast<float>(kBoardSize + 1));
    layout.tileSize = std::max(0.0F, (boardSize - layout.tileGap * (kBoardSize + 1)) / static_cast<float>(kBoardSize));

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
    const float gap = std::clamp(layout.boardRect.width * 0.03F, 4.0F, 10.0F);

    if (showTouchHud) {
        const float topWidth = (layout.boardRect.width - gap * 5.0F) / 6.0F;
        const float bottomWidth = (layout.boardRect.width - gap * 4.0F) / 5.0F;
        const float buttonHeight = std::clamp(controlBandHeight * 0.36F, 24.0F, 62.0F);
        AddControlRow(layout, layout.boardRect.x, bandY, topWidth, buttonHeight, gap,
                      {ControlId::MoveUp, ControlId::MoveLeft, ControlId::MoveDown, ControlId::MoveRight, ControlId::Restart, ControlId::Undo});
        AddControlRow(layout, layout.boardRect.x, bandY + buttonHeight + gap, bottomWidth, buttonHeight, gap,
                      {ControlId::ToggleAutoAI, ControlId::StepAI, ControlId::ToggleHelp, ControlId::CycleAgent, ControlId::CycleAnimationSpeed});
    } else {
        const float buttonWidth = (layout.boardRect.width - gap * 3.0F) / 4.0F;
        const float buttonHeight = std::clamp(controlBandHeight * 0.78F, 24.0F, 48.0F);
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
