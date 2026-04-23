#include "ui.h"

#include <algorithm>
#include <array>
#include <string>

#include <raylib.h>

namespace game2048 {

namespace {

const char* AgentName(ai::AgentKind kind) {
    switch (kind) {
        case ai::AgentKind::Human: return "Human";
        case ai::AgentKind::Greedy: return "Greedy";
        case ai::AgentKind::Expectimax: return "Expectimax";
    }
    return "Unknown";
}

const char* ControlModeName(ControlMode mode) {
    switch (mode) {
        case ControlMode::Human: return "Human";
        case ControlMode::AIAutoplay: return "Auto AI";
        case ControlMode::AISingleStep: return "AI Step";
    }
    return "Unknown";
}

const char* InputGateName(InputGate gate) {
    switch (gate) {
        case InputGate::Accepting: return "Ready";
        case InputGate::BlockedByOverlay: return "Overlay";
        case InputGate::BlockedByAnimation: return "Animating";
    }
    return "Unknown";
}

const char* DirectionName(Direction direction) {
    switch (direction) {
        case Direction::Up: return "Up";
        case Direction::Down: return "Down";
        case Direction::Left: return "Left";
        case Direction::Right: return "Right";
    }
    return "-";
}

const char* AnimationName(AnimationSpeed speed) {
    switch (speed) {
        case AnimationSpeed::Normal: return "Normal";
        case AnimationSpeed::Slow: return "Slow";
        case AnimationSpeed::Turbo: return "Turbo";
    }
    return "Unknown";
}

static constexpr float kBasePanelH = 784.0F;

struct PanelStyle {
    float scale;
    int infoFs;
    int headerFs;
    int evalFs;
    int keyFs;
    float infoRowH;
    float headerH;
    float evalRowH;
    float keyRowH;
    float divGap;
    float labelColW;
};

PanelStyle MakeStyle(float panelH, float innerW) {
    const float scale = std::clamp(panelH / kBasePanelH, 0.52F, 1.0F);
    PanelStyle style {};
    style.scale = scale;
    style.infoFs = std::max(11, static_cast<int>(18.0F * scale));
    style.headerFs = std::max(11, static_cast<int>(20.0F * scale));
    style.evalFs = std::max(10, static_cast<int>(15.0F * scale));
    style.keyFs = std::max(10, static_cast<int>(16.0F * scale));
    style.infoRowH = std::max(14.0F, 24.0F * scale);
    style.headerH = std::max(18.0F, 32.0F * scale);
    style.evalRowH = std::max(13.0F, 27.0F * scale);
    style.keyRowH = std::max(13.0F, 20.0F * scale);
    style.divGap = std::max(3.0F, 6.0F * scale);
    style.labelColW = std::clamp(innerW * 0.48F, 60.0F, 130.0F);
    return style;
}

void DrawMetricBox(Rectangle rect, const char* label, const std::string& value, Color fill) {
    DrawRectangleRounded(rect, 0.18F, 8, fill);
    const int labelFs = std::max(12, static_cast<int>(rect.height * 0.28F));
    DrawText(label, static_cast<int>(rect.x + 10.0F), static_cast<int>(rect.y + 5.0F), labelFs, Fade(RAYWHITE, 0.72F));

    int valueFs = std::max(14, static_cast<int>(rect.height * 0.46F));
    const int maxValueWidth = static_cast<int>(rect.width - 18.0F);
    while (valueFs > 12 && MeasureText(value.c_str(), valueFs) > maxValueWidth) {
        valueFs -= 2;
    }

    const int valueY = static_cast<int>(rect.y + rect.height - static_cast<float>(valueFs) - 5.0F);
    DrawText(value.c_str(), static_cast<int>(rect.x + 10.0F), valueY, valueFs, RAYWHITE);
}

void DrawSectionHeader(const Rectangle& panelRect, float& y, const char* title, const PanelStyle& style) {
    DrawRectangle(
        static_cast<int>(panelRect.x + 1.0F),
        static_cast<int>(y - 3.0F),
        static_cast<int>(panelRect.width - 2.0F),
        static_cast<int>(style.headerH),
        Fade(Color{143, 122, 102, 255}, 0.13F));
    DrawText(title, static_cast<int>(panelRect.x + 14.0F), static_cast<int>(y), style.headerFs, Color{100, 88, 76, 255});
    y += style.headerH + 2.0F;
}

void DrawDivider(const Rectangle& panelRect, float y) {
    DrawLineEx(
        {panelRect.x + 8.0F, y},
        {panelRect.x + panelRect.width - 8.0F, y},
        1.0F,
        Fade(Color{143, 122, 102, 255}, 0.30F));
}

void DrawInfoLine(float x, float& y, float innerW, const std::string& label, const std::string& value, const PanelStyle& style) {
    DrawText(label.c_str(), static_cast<int>(x), static_cast<int>(y), style.infoFs, Color{119, 110, 101, 255});

    const float valueX = x + style.labelColW;
    const int maxValueWidth = static_cast<int>(innerW - style.labelColW);
    int valueFs = style.infoFs;
    while (valueFs > 9 && MeasureText(value.c_str(), valueFs) > maxValueWidth) {
        valueFs -= 1;
    }

    DrawText(value.c_str(), static_cast<int>(valueX), static_cast<int>(y), valueFs, Color{58, 59, 84, 255});
    y += style.infoRowH;
}

void DrawEvalRow(float x, float& y, float innerW, const char* label, double value, const PanelStyle& style) {
    const float maxExpected = 400.0F;
    const float clamped = std::clamp(static_cast<float>(value), 0.0F, maxExpected);
    const float barWidth = innerW * (clamped / maxExpected);
    const float barHeight = std::max(3.0F, 5.0F * style.scale);

    DrawText(label, static_cast<int>(x), static_cast<int>(y), style.evalFs, Color{119, 110, 101, 255});

    const char* valueText = TextFormat("%.1f", value);
    const float valueX = x + style.labelColW;
    const int maxValueWidth = static_cast<int>(innerW - style.labelColW);
    int valueFs = style.evalFs;
    while (valueFs > 9 && MeasureText(valueText, valueFs) > maxValueWidth) {
        valueFs -= 1;
    }
    DrawText(valueText, static_cast<int>(valueX), static_cast<int>(y), valueFs, Color{58, 59, 84, 255});

    const float barY = y + static_cast<float>(style.evalFs) + 2.0F;
    DrawRectangleRounded({x, barY, innerW, barHeight}, 1.0F, 4, Fade(Color{143, 122, 102, 255}, 0.18F));
    if (barWidth > 0.5F) {
        DrawRectangleRounded({x, barY, barWidth, barHeight}, 1.0F, 4, Fade(Color{119, 110, 101, 255}, 0.55F));
    }
    y += style.evalRowH;
}

}  // namespace

void UI::DrawPanels(const LayoutMetrics& layout, const HUDState& state) const {
    const float topWidth = layout.topBarRect.width;
    const int titleFs = std::clamp(static_cast<int>(topWidth * 0.034F), 22, 42);
    const int subtitleFs = std::clamp(static_cast<int>(topWidth * 0.015F), 12, 18);

    DrawText("2048 Engine",
             static_cast<int>(layout.topBarRect.x),
             static_cast<int>(layout.topBarRect.y + 4.0F),
             titleFs,
             Color{119, 110, 101, 255});
    DrawText("raylib / deterministic / expectimax",
             static_cast<int>(layout.topBarRect.x),
             static_cast<int>(layout.topBarRect.y + static_cast<float>(titleFs) + 5.0F),
             subtitleFs,
             Color{143, 122, 102, 255});

    const float boxWidth = std::clamp(topWidth * 0.095F, 96.0F, 138.0F);
    const float boxHeight = layout.topBarRect.height - 4.0F;
    const float boxGap = 8.0F;
    const float right = layout.topBarRect.x + topWidth;
    DrawMetricBox({right - boxWidth * 2.0F - boxGap, layout.topBarRect.y + 2.0F, boxWidth, boxHeight},
                  "Score",
                  std::to_string(state.score),
                  Color{246, 124, 95, 255});
    DrawMetricBox({right - boxWidth, layout.topBarRect.y + 2.0F, boxWidth, boxHeight},
                  "Best",
                  std::to_string(state.bestScore),
                  Color{237, 200, 80, 255});

    DrawRectangleRounded(layout.panelRect, 0.05F, 10, Fade(Color{249, 246, 242, 255}, 0.92F));
    DrawRectangleLinesEx(layout.panelRect, 1.5F, Fade(Color{143, 122, 102, 255}, 0.18F));

    const float innerW = layout.panelRect.width - 28.0F;
    const float x = layout.panelRect.x + 14.0F;
    const PanelStyle style = MakeStyle(layout.panelRect.height, innerW);

    BeginScissorMode(
        static_cast<int>(layout.panelRect.x),
        static_cast<int>(layout.panelRect.y),
        static_cast<int>(layout.panelRect.width),
        static_cast<int>(layout.panelRect.height));

    float y = layout.panelRect.y + std::max(8.0F, 12.0F * style.scale);

    DrawSectionHeader(layout.panelRect, y, "Status", style);
    DrawInfoLine(x, y, innerW, "Mode", ControlModeName(state.controlMode), style);
    DrawInfoLine(x, y, innerW, "AI", AgentName(state.agentKind), style);
    DrawInfoLine(x, y, innerW, "Gate", InputGateName(state.inputGate), style);
    DrawInfoLine(x, y, innerW, "Anim", AnimationName(state.animationSpeed), style);
    DrawInfoLine(x, y, innerW, "Max Tile", std::to_string(state.maxTile), style);
    DrawInfoLine(x, y, innerW, "Goal", state.achieved2048 ? "2048 reached" : "In progress", style);
    DrawInfoLine(x, y, innerW, "Seed", std::to_string(state.seed), style);
    DrawInfoLine(x, y, innerW, "Hint", state.recommendation.valid ? DirectionName(state.recommendation.direction) : "-", style);

    y += style.divGap;
    DrawDivider(layout.panelRect, y);
    y += style.divGap;
    DrawSectionHeader(layout.panelRect, y, "Search", style);
    DrawInfoLine(x, y, innerW, "Depth", std::to_string(state.lastSearch.maxDepthReached), style);
    DrawInfoLine(x, y, innerW, "Nodes", std::to_string(state.lastSearch.nodes), style);
    DrawInfoLine(x, y, innerW, "TT Hits", std::to_string(state.lastSearch.cacheHits), style);
    DrawInfoLine(x, y, innerW, "Eval", TextFormat("%.1f", state.lastSearch.evaluation), style);
    DrawInfoLine(x, y, innerW, "Think ms", TextFormat("%.2f", state.lastSearch.elapsedMs), style);
    DrawInfoLine(x, y, innerW, "FPS", std::to_string(GetFPS()), style);

    y += style.divGap;
    DrawDivider(layout.panelRect, y);
    y += style.divGap;
    DrawSectionHeader(layout.panelRect, y, "Evaluator", style);
    DrawEvalRow(x, y, innerW, "Empty", state.evaluatorBreakdown.emptyTiles, style);
    DrawEvalRow(x, y, innerW, "Mono", state.evaluatorBreakdown.monotonicity, style);
    DrawEvalRow(x, y, innerW, "Smooth", state.evaluatorBreakdown.smoothness, style);
    DrawEvalRow(x, y, innerW, "Corner", state.evaluatorBreakdown.cornerMax, style);
    DrawEvalRow(x, y, innerW, "Merge", state.evaluatorBreakdown.mergePotential, style);
    DrawEvalRow(x, y, innerW, "Snake", state.evaluatorBreakdown.snakePattern, style);
    DrawEvalRow(x, y, innerW, "Trap", state.evaluatorBreakdown.trapPenalty, style);

    y += style.divGap;
    DrawDivider(layout.panelRect, y);
    y += style.divGap;
    DrawSectionHeader(layout.panelRect, y, "Keys", style);
    const Color keyColor = {58, 59, 84, 255};
    DrawText("Arrows/WASD  Move", static_cast<int>(x), static_cast<int>(y), style.keyFs, keyColor);
    y += style.keyRowH;
    DrawText("Space  Autoplay", static_cast<int>(x), static_cast<int>(y), style.keyFs, keyColor);
    y += style.keyRowH;
    DrawText("N  AI step", static_cast<int>(x), static_cast<int>(y), style.keyFs, keyColor);
    y += style.keyRowH;
    DrawText("R/U  Restart / Undo", static_cast<int>(x), static_cast<int>(y), style.keyFs, keyColor);
    y += style.keyRowH;
    DrawText("Tab/T  AI / anim", static_cast<int>(x), static_cast<int>(y), style.keyFs, keyColor);
    y += style.keyRowH;
    DrawText("H or F1  Help", static_cast<int>(x), static_cast<int>(y), style.keyFs, keyColor);

    EndScissorMode();
}

void UI::DrawOverlay(const LayoutMetrics& layout, const HUDState& state) const {
    if (state.overlayMode == OverlayMode::GameOver || state.overlayMode == OverlayMode::Victory) {
        const bool isGameOver = state.overlayMode == OverlayMode::GameOver;
        const Color tint = isGameOver ? Fade(Color{220, 210, 198, 255}, 0.85F)
                                      : Fade(Color{56, 202, 168, 255}, 0.48F);
        DrawRectangleRounded(layout.boardRect, 0.05F, 10, tint);

        const char* title = isGameOver ? "No More Moves" : "2048 Reached!";
        const char* subtitle = isGameOver
            ? "R same-seed restart  |  Esc exit"
            : "Move to continue  |  Space for AI  |  Esc dismiss";

        const float boardWidth = layout.boardRect.width;
        const int titleFs = std::clamp(static_cast<int>(boardWidth * 0.075F), 28, 52);
        const int subtitleFs = std::clamp(static_cast<int>(boardWidth * 0.034F), 16, 22);

        int fittedTitleFs = titleFs;
        while (fittedTitleFs > 20 && MeasureText(title, fittedTitleFs) > static_cast<int>(boardWidth - 40.0F)) {
            fittedTitleFs -= 2;
        }

        const int titleWidth = MeasureText(title, fittedTitleFs);
        const int subtitleWidth = MeasureText(subtitle, subtitleFs);
        const int titleX = static_cast<int>(layout.boardRect.x + (boardWidth - static_cast<float>(titleWidth)) * 0.5F);
        const int titleY = static_cast<int>(layout.boardRect.y + layout.boardRect.height * 0.38F);
        DrawText(title, titleX, titleY, fittedTitleFs, Color{80, 72, 64, 255});

        const float subtitleX = layout.boardRect.x + (boardWidth - static_cast<float>(subtitleWidth)) * 0.5F;
        const float subtitleY = static_cast<float>(titleY) + static_cast<float>(fittedTitleFs) + 14.0F;
        DrawRectangleRounded(
            {subtitleX - 16.0F, subtitleY - 6.0F, static_cast<float>(subtitleWidth) + 32.0F, static_cast<float>(subtitleFs) + 14.0F},
            0.40F,
            8,
            Fade(Color{100, 88, 76, 255}, 0.14F));
        DrawText(subtitle, static_cast<int>(subtitleX), static_cast<int>(subtitleY), subtitleFs, Color{100, 88, 76, 255});
    }

    if (state.overlayMode != OverlayMode::Help) {
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

    BeginScissorMode(
        static_cast<int>(modal.x),
        static_cast<int>(modal.y),
        static_cast<int>(modal.width),
        static_cast<int>(modal.height));

    float y = modal.y + pad;
    DrawText("Help", static_cast<int>(modal.x + pad), static_cast<int>(y), headerFs, Color{119, 110, 101, 255});
    y += static_cast<float>(headerFs) + 14.0F;

    DrawText("Deterministic 2048 with AI and benchmark mode.",
             static_cast<int>(modal.x + pad),
             static_cast<int>(y),
             bodyFs,
             Color{58, 59, 84, 255});
    y += lineH + 8.0F;

    DrawText("Move: Arrows or WASD",
             static_cast<int>(modal.x + pad),
             static_cast<int>(y),
             bodyFs,
             Color{58, 59, 84, 255});
    y += lineH;
    DrawText("R: same-seed restart   U: undo   Space: autoplay   N: AI step",
             static_cast<int>(modal.x + pad),
             static_cast<int>(y),
             bodyFs,
             Color{58, 59, 84, 255});
    y += lineH;
    DrawText("Tab: switch AI (greedy / expectimax)",
             static_cast<int>(modal.x + pad),
             static_cast<int>(y),
             bodyFs,
             Color{58, 59, 84, 255});
    y += lineH;
    DrawText("T: cycle animation speed",
             static_cast<int>(modal.x + pad),
             static_cast<int>(y),
             bodyFs,
             Color{58, 59, 84, 255});
    y += lineH;
    DrawText("Esc / H / F1: close help",
             static_cast<int>(modal.x + pad),
             static_cast<int>(y),
             bodyFs,
             Color{58, 59, 84, 255});
    y += lineH + 10.0F;

    DrawText("CLI benchmark:",
             static_cast<int>(modal.x + pad),
             static_cast<int>(y),
             smallFs,
             Color{143, 122, 102, 255});
    y += smallLineH;
    DrawText("--benchmark N  --ai expectimax  --time-budget-ms 10",
             static_cast<int>(modal.x + pad),
             static_cast<int>(y),
             smallFs,
             Color{143, 122, 102, 255});
    y += smallLineH + 8.0F;

    if (state.showContinueHint) {
        DrawText("You can keep playing after hitting 2048.",
                 static_cast<int>(modal.x + pad),
                 static_cast<int>(y),
                 smallFs,
                 Color{143, 122, 102, 255});
    }

    EndScissorMode();
}

}  // namespace game2048
