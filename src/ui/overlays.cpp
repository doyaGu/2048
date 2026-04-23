#include "ui/overlays.h"

#include <algorithm>
#include <string>

#include <raylib.h>

#include "ui/layout.h"
#include "ui/ui.h"
#include "ui/widgets.h"

namespace game2048 {

namespace {

constexpr OverlayMode ToLayoutOverlayMode(HUDOverlayMode mode) {
    switch (mode) {
        case HUDOverlayMode::None:
            return OverlayMode::None;
        case HUDOverlayMode::Help:
            return OverlayMode::Help;
        case HUDOverlayMode::Victory:
            return OverlayMode::Victory;
        case HUDOverlayMode::GameOver:
            return OverlayMode::GameOver;
    }
    return OverlayMode::None;
}

}  // namespace

void DrawOverlayView(const LayoutMetrics& layout, const HUDState& state) {
    if (state.session.overlayMode == HUDOverlayMode::GameOver || state.session.overlayMode == HUDOverlayMode::Victory) {
        const bool isGameOver = state.session.overlayMode == HUDOverlayMode::GameOver;
        const Color tint = isGameOver ? Fade(Color{220, 210, 198, 255}, 0.85F)
                                      : Fade(Color{56, 202, 168, 255}, 0.48F);
        DrawRectangleRounded(layout.boardRect, 0.05F, 10, tint);

        const std::string title = isGameOver ? "No More Moves" : "2048 Reached!";
        const std::string subtitle = isGameOver
            ? "R same-seed restart  |  Esc exit"
            : "Move to continue  |  Space for AI  |  Esc dismiss";

        const float boardWidth = layout.boardRect.width;
        const int titleFs = std::clamp(static_cast<int>(boardWidth * 0.075F), 28, 52);
        const int subtitleFs = std::clamp(static_cast<int>(boardWidth * 0.034F), 16, 22);
        const int fittedTitleFs = FitFontSize(title, boardWidth - 40.0F, titleFs, 20, 2);

        const int titleWidth = MeasureText(title.c_str(), fittedTitleFs);
        const int subtitleWidth = MeasureText(subtitle.c_str(), subtitleFs);
        const int titleX = static_cast<int>(layout.boardRect.x + (boardWidth - static_cast<float>(titleWidth)) * 0.5F);
        const int titleY = static_cast<int>(layout.boardRect.y + layout.boardRect.height * 0.38F);
        DrawText(title.c_str(), titleX, titleY, fittedTitleFs, Color{80, 72, 64, 255});

        const float subtitleX = layout.boardRect.x + (boardWidth - static_cast<float>(subtitleWidth)) * 0.5F;
        const float subtitleY = static_cast<float>(titleY) + static_cast<float>(fittedTitleFs) + 14.0F;
        DrawRectangleRounded({subtitleX - 16.0F, subtitleY - 6.0F, static_cast<float>(subtitleWidth) + 32.0F, static_cast<float>(subtitleFs) + 14.0F},
                             0.40F,
                             8,
                             Fade(Color{100, 88, 76, 255}, 0.14F));
        DrawText(subtitle.c_str(), static_cast<int>(subtitleX), static_cast<int>(subtitleY), subtitleFs, Color{100, 88, 76, 255});

        const OverlayMode overlayMode = ToLayoutOverlayMode(state.session.overlayMode);
        if (state.session.overlayMode == HUDOverlayMode::Victory) {
            DrawControlButton(OverlayActionRect(layout, overlayMode, 0), "Continue", false, false);
            DrawControlButton(OverlayActionRect(layout, overlayMode, 1), "Auto AI", state.session.controlMode == HUDControlMode::AIAutoplay, false);
        } else {
            DrawControlButton(OverlayActionRect(layout, overlayMode, 0), "Restart", false, false);
            DrawControlButton(OverlayActionRect(layout, overlayMode, 1), "Exit", false, false);
        }
    }

    if (state.session.overlayMode != HUDOverlayMode::Help) {
        return;
    }

    const Rectangle modal {
        layout.boardRect.x + 28.0F,
        layout.boardRect.y + 28.0F,
        layout.boardRect.width - 56.0F,
        layout.boardRect.height - 56.0F
    };
    DrawRectangleRounded(modal, 0.05F, 10, Fade(Color{249, 246, 242, 255}, 0.96F));
    DrawRectangleLinesEx(modal, 1.5F, Fade(Color{143, 122, 102, 255}, 0.22F));

    const float modalWidth = modal.width;
    const int headerFs = std::clamp(static_cast<int>(modalWidth * 0.058F), 20, 34);
    const int bodyFs = std::clamp(static_cast<int>(modalWidth * 0.038F), 14, 22);
    const int smallFs = std::clamp(static_cast<int>(modalWidth * 0.030F), 12, 18);
    const float pad = 22.0F;
    const float lineH = static_cast<float>(bodyFs) + 8.0F;
    const float smallLineH = static_cast<float>(smallFs) + 6.0F;

    BeginScissorMode(static_cast<int>(modal.x),
                     static_cast<int>(modal.y),
                     static_cast<int>(modal.width),
                     static_cast<int>(modal.height));

    float y = modal.y + pad;
    DrawText("Help", static_cast<int>(modal.x + pad), static_cast<int>(y), headerFs, Color{119, 110, 101, 255});
    y += static_cast<float>(headerFs) + 14.0F;

    DrawText("Deterministic 2048 with AI and benchmark mode.", static_cast<int>(modal.x + pad), static_cast<int>(y), bodyFs, Color{58, 59, 84, 255});
    y += lineH + 8.0F;

    DrawText("Move: Arrows or WASD", static_cast<int>(modal.x + pad), static_cast<int>(y), bodyFs, Color{58, 59, 84, 255});
    y += lineH;
    DrawText("R: same-seed restart   U: undo   Space: autoplay   N: AI step", static_cast<int>(modal.x + pad), static_cast<int>(y), bodyFs, Color{58, 59, 84, 255});
    y += lineH;
    DrawText("Tab: switch AI (greedy / expectimax)", static_cast<int>(modal.x + pad), static_cast<int>(y), bodyFs, Color{58, 59, 84, 255});
    y += lineH;
    DrawText("T: cycle animation speed", static_cast<int>(modal.x + pad), static_cast<int>(y), bodyFs, Color{58, 59, 84, 255});
    y += lineH;
    DrawText("Esc / H / F1: close help", static_cast<int>(modal.x + pad), static_cast<int>(y), bodyFs, Color{58, 59, 84, 255});
    y += lineH + 10.0F;

    DrawText("CLI benchmark:", static_cast<int>(modal.x + pad), static_cast<int>(y), smallFs, Color{143, 122, 102, 255});
    y += smallLineH;
    DrawText("--benchmark N  --ai expectimax  --time-budget-ms 10", static_cast<int>(modal.x + pad), static_cast<int>(y), smallFs, Color{143, 122, 102, 255});
    y += smallLineH + 8.0F;

    if (state.session.showContinueHint) {
        DrawText("You can keep playing after hitting 2048.", static_cast<int>(modal.x + pad), static_cast<int>(y), smallFs, Color{143, 122, 102, 255});
    }

    EndScissorMode();
    DrawControlButton(OverlayActionRect(layout, ToLayoutOverlayMode(state.session.overlayMode), 0), "Close", false, false);
}

}  // namespace game2048
