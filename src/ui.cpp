#include "ui.h"

#include <algorithm>
#include <array>
#include <string>

#include <raylib.h>

namespace game2048 {

namespace {

// ---------------------------------------------------------------------------
// Name helpers
// ---------------------------------------------------------------------------
const char* AgentName(ai::AgentKind kind) {
    switch (kind) {
        case ai::AgentKind::Human:      return "Human";
        case ai::AgentKind::Greedy:     return "Greedy";
        case ai::AgentKind::Expectimax: return "Expectimax";
    }
    return "Unknown";
}

const char* DirectionName(Direction direction) {
    switch (direction) {
        case Direction::Up:    return "Up";
        case Direction::Down:  return "Down";
        case Direction::Left:  return "Left";
        case Direction::Right: return "Right";
    }
    return "-";
}

const char* AnimationName(AnimationSpeed speed) {
    switch (speed) {
        case AnimationSpeed::Normal: return "Normal";
        case AnimationSpeed::Slow:   return "Slow";
        case AnimationSpeed::Turbo:  return "Turbo";
    }
    return "Unknown";
}

// ---------------------------------------------------------------------------
// PanelStyle — all sizes derived from available panel height so nothing overflows
//
// Base content height at scale = 1.0:
//   topPad(12) + 4*(headerH=32) + 3*divBlock(6+1+6=13) + statusRows(6*24=144)
//   + searchRows(6*24=144) + evalRows(7*27=189) + keyRows(6*20=120) + bottomPad(8)
//   ≈ 12 + 128 + 39 + 144 + 144 + 189 + 120 + 8 = 784
// ---------------------------------------------------------------------------
static constexpr float kBasePanelH = 784.0F;

struct PanelStyle {
    float scale;
    int   infoFs;    // info label/value font size
    int   headerFs;  // section header font size
    int   evalFs;    // evaluator label font size
    int   keyFs;     // keys-section font size
    float infoRowH;  // total height per info row
    float headerH;   // total height per section header (including band)
    float evalRowH;  // total height per evaluator row (text + bar + gap)
    float keyRowH;   // total height per key row
    float divGap;    // padding on each side of divider line
    float labelColW; // width of the label column
};

PanelStyle MakeStyle(float panelH, float innerW) {
    const float s = std::clamp(panelH / kBasePanelH, 0.52F, 1.0F);
    PanelStyle st;
    st.scale    = s;
    st.infoFs   = std::max(11, static_cast<int>(18.0F * s));
    st.headerFs = std::max(11, static_cast<int>(20.0F * s));
    st.evalFs   = std::max(10, static_cast<int>(15.0F * s));
    st.keyFs    = std::max(10, static_cast<int>(16.0F * s));
    st.infoRowH  = std::max(14.0F, 24.0F * s);
    st.headerH   = std::max(18.0F, 32.0F * s);
    st.evalRowH  = std::max(13.0F, 27.0F * s);
    st.keyRowH   = std::max(13.0F, 20.0F * s);
    st.divGap    = std::max(3.0F,   6.0F * s);
    // label column: half the inner width, clamped so values always have room
    st.labelColW = std::clamp(innerW * 0.48F, 60.0F, 130.0F);
    return st;
}

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

// Score / best metric box with auto-shrinking value font
void DrawMetricBox(Rectangle rect, const char* label, const std::string& value, Color fill) {
    DrawRectangleRounded(rect, 0.18F, 8, fill);
    const int labelFs = std::max(12, static_cast<int>(rect.height * 0.28F));
    DrawText(label, static_cast<int>(rect.x + 10), static_cast<int>(rect.y + 5), labelFs, Fade(RAYWHITE, 0.72F));

    // Shrink value font until it fits horizontally
    int valFs = std::max(14, static_cast<int>(rect.height * 0.46F));
    const int maxValW = static_cast<int>(rect.width - 18);
    while (valFs > 12 && MeasureText(value.c_str(), valFs) > maxValW) {
        valFs -= 2;
    }
    const int valY = static_cast<int>(rect.y + rect.height - static_cast<float>(valFs) - 5.0F);
    DrawText(value.c_str(), static_cast<int>(rect.x + 10), valY, valFs, RAYWHITE);
}

// Section header: faint tinted band + title text; advances y by headerH
void DrawSectionHeader(const Rectangle& panelRect, float& y, const char* title, const PanelStyle& st) {
    DrawRectangle(
        static_cast<int>(panelRect.x + 1),
        static_cast<int>(y - 3.0F),
        static_cast<int>(panelRect.width - 2),
        static_cast<int>(st.headerH),
        Fade(Color{143, 122, 102, 255}, 0.13F)
    );
    DrawText(title, static_cast<int>(panelRect.x + 14.0F), static_cast<int>(y), st.headerFs, Color{100, 88, 76, 255});
    y += st.headerH + 2.0F;
}

// Thin horizontal divider
void DrawDivider(const Rectangle& panelRect, float y) {
    DrawLineEx(
        {panelRect.x + 8.0F,                      y},
        {panelRect.x + panelRect.width - 8.0F, y},
        1.0F,
        Fade(Color{143, 122, 102, 255}, 0.30F)
    );
}

// Info row: label left, value right of label column; auto-shrinks value font
void DrawInfoLine(float x, float& y, float innerW,
                  const std::string& label, const std::string& value,
                  const PanelStyle& st) {
    DrawText(label.c_str(), static_cast<int>(x), static_cast<int>(y), st.infoFs, Color{119, 110, 101, 255});

    const float valX    = x + st.labelColW;
    const int   maxValW = static_cast<int>(innerW - st.labelColW);
    int valFs = st.infoFs;
    while (valFs > 9 && MeasureText(value.c_str(), valFs) > maxValW) {
        valFs -= 1;
    }
    DrawText(value.c_str(), static_cast<int>(valX), static_cast<int>(y), valFs, Color{58, 59, 84, 255});
    y += st.infoRowH;
}

// Evaluator row: label + value + thin progress bar; advances y by evalRowH
void DrawEvalRow(float x, float& y, float innerW,
                 const char* label, double value,
                 const PanelStyle& st) {
    const float maxExpected = 400.0F;
    const float clamped     = std::clamp(static_cast<float>(value), 0.0F, maxExpected);
    const float barW        = innerW * (clamped / maxExpected);
    const float barH        = std::max(3.0F, 5.0F * st.scale);

    DrawText(label, static_cast<int>(x), static_cast<int>(y), st.evalFs, Color{119, 110, 101, 255});

    const char* valStr  = TextFormat("%.1f", value);
    const float valX    = x + st.labelColW;
    const int   maxValW = static_cast<int>(innerW - st.labelColW);
    int valFs = st.evalFs;
    while (valFs > 9 && MeasureText(valStr, valFs) > maxValW) valFs -= 1;
    DrawText(valStr, static_cast<int>(valX), static_cast<int>(y), valFs, Color{58, 59, 84, 255});

    const float barY = y + static_cast<float>(st.evalFs) + 2.0F;
    DrawRectangleRounded({x,    barY, innerW, barH}, 1.0F, 4, Fade(Color{143, 122, 102, 255}, 0.18F));
    if (barW > 0.5F) {
        DrawRectangleRounded({x, barY, barW,   barH}, 1.0F, 4, Fade(Color{119, 110, 101, 255}, 0.55F));
    }
    y += st.evalRowH;
}

}  // namespace

// ---------------------------------------------------------------------------
// DrawPanels — top bar + side panel, fully adaptive to window size
// ---------------------------------------------------------------------------
void UI::DrawPanels(const LayoutMetrics& layout, const HUDState& state) const {
    // ---- Top bar ----
    const float topW   = layout.topBarRect.width;
    // Scale title/subtitle with available width
    const int titleFs  = std::clamp(static_cast<int>(topW * 0.034F), 22, 42);
    const int subFs    = std::clamp(static_cast<int>(topW * 0.015F), 12, 18);

    DrawText("2048 Engine",
             static_cast<int>(layout.topBarRect.x),
             static_cast<int>(layout.topBarRect.y + 4.0F),
             titleFs, Color{119, 110, 101, 255});
    DrawText("raylib · deterministic · expectimax",
             static_cast<int>(layout.topBarRect.x),
             static_cast<int>(layout.topBarRect.y + static_cast<float>(titleFs) + 5.0F),
             subFs, Color{143, 122, 102, 255});

    // Score/Best boxes — width scales with bar so large numbers always fit
    const float boxW  = std::clamp(topW * 0.095F, 96.0F, 138.0F);
    const float boxH  = layout.topBarRect.height - 4.0F;
    const float boxGap = 8.0F;
    const float right  = layout.topBarRect.x + topW;
    DrawMetricBox({right - boxW * 2.0F - boxGap, layout.topBarRect.y + 2.0F, boxW, boxH},
                  "Score", std::to_string(state.score),      Color{246, 124, 95,  255});
    DrawMetricBox({right - boxW,                  layout.topBarRect.y + 2.0F, boxW, boxH},
                  "Best",  std::to_string(state.bestScore),  Color{237, 200, 80,  255});

    // ---- Side panel background ----
    DrawRectangleRounded(layout.panelRect, 0.05F, 10, Fade(Color{249, 246, 242, 255}, 0.92F));
    DrawRectangleLinesEx(layout.panelRect, 1.5F, Fade(Color{143, 122, 102, 255}, 0.18F));

    // ---- Panel content — clipped to panel rect ----
    const float innerW = layout.panelRect.width - 28.0F;
    const float x      = layout.panelRect.x + 14.0F;
    const PanelStyle st = MakeStyle(layout.panelRect.height, innerW);

    BeginScissorMode(
        static_cast<int>(layout.panelRect.x),
        static_cast<int>(layout.panelRect.y),
        static_cast<int>(layout.panelRect.width),
        static_cast<int>(layout.panelRect.height)
    );

    float y = layout.panelRect.y + std::max(8.0F, 12.0F * st.scale);

    // Status
    DrawSectionHeader(layout.panelRect, y, "Status", st);
    DrawInfoLine(x, y, innerW, "Mode",     state.autoPlay ? "Auto AI" : "Human",                          st);
    DrawInfoLine(x, y, innerW, "AI",       AgentName(state.agentKind),                                     st);
    DrawInfoLine(x, y, innerW, "Anim",     AnimationName(state.animationSpeed),                            st);
    DrawInfoLine(x, y, innerW, "Max Tile", std::to_string(state.maxTile),                                  st);
    DrawInfoLine(x, y, innerW, "Seed",     std::to_string(state.seed),                                     st);
    DrawInfoLine(x, y, innerW, "Hint",     state.recommendation.valid ? DirectionName(state.recommendation.direction) : "-", st);

    // Search
    y += st.divGap;
    DrawDivider(layout.panelRect, y);
    y += st.divGap;
    DrawSectionHeader(layout.panelRect, y, "Search", st);
    DrawInfoLine(x, y, innerW, "Depth",    std::to_string(state.lastSearch.maxDepthReached), st);
    DrawInfoLine(x, y, innerW, "Nodes",    std::to_string(state.lastSearch.nodes),           st);
    DrawInfoLine(x, y, innerW, "TT Hits",  std::to_string(state.lastSearch.cacheHits),       st);
    DrawInfoLine(x, y, innerW, "Eval",     TextFormat("%.1f",  state.lastSearch.evaluation), st);
    DrawInfoLine(x, y, innerW, "Think ms", TextFormat("%.2f",  state.lastSearch.elapsedMs),  st);
    DrawInfoLine(x, y, innerW, "FPS",      std::to_string(GetFPS()),                          st);

    // Evaluator
    y += st.divGap;
    DrawDivider(layout.panelRect, y);
    y += st.divGap;
    DrawSectionHeader(layout.panelRect, y, "Evaluator", st);
    DrawEvalRow(x, y, innerW, "Empty",  state.evaluatorBreakdown.emptyTiles,    st);
    DrawEvalRow(x, y, innerW, "Mono",   state.evaluatorBreakdown.monotonicity,   st);
    DrawEvalRow(x, y, innerW, "Smooth", state.evaluatorBreakdown.smoothness,     st);
    DrawEvalRow(x, y, innerW, "Corner", state.evaluatorBreakdown.cornerMax,      st);
    DrawEvalRow(x, y, innerW, "Merge",  state.evaluatorBreakdown.mergePotential, st);
    DrawEvalRow(x, y, innerW, "Snake",  state.evaluatorBreakdown.snakePattern,   st);
    DrawEvalRow(x, y, innerW, "Trap",   state.evaluatorBreakdown.trapPenalty,    st);

    // Keys
    y += st.divGap;
    DrawDivider(layout.panelRect, y);
    y += st.divGap;
    DrawSectionHeader(layout.panelRect, y, "Keys", st);
    const Color kc = {58, 59, 84, 255};
    DrawText("Arrows/WASD  Move",    static_cast<int>(x), static_cast<int>(y), st.keyFs, kc); y += st.keyRowH;
    DrawText("Space  Autoplay",      static_cast<int>(x), static_cast<int>(y), st.keyFs, kc); y += st.keyRowH;
    DrawText("N  AI step",           static_cast<int>(x), static_cast<int>(y), st.keyFs, kc); y += st.keyRowH;
    DrawText("R/U  Reset / Undo",    static_cast<int>(x), static_cast<int>(y), st.keyFs, kc); y += st.keyRowH;
    DrawText("Tab/T  AI / anim",     static_cast<int>(x), static_cast<int>(y), st.keyFs, kc); y += st.keyRowH;
    DrawText("H or F1  Help",        static_cast<int>(x), static_cast<int>(y), st.keyFs, kc);

    EndScissorMode();
}

// ---------------------------------------------------------------------------
// DrawOverlay — game-over / 2048-reached / help modal, all adaptive
// ---------------------------------------------------------------------------
void UI::DrawOverlay(const LayoutMetrics& layout, const HUDState& state) const {
    if (state.gameOver || state.showVictoryOverlay) {
        Color tint = state.gameOver
            ? Fade(Color{220, 210, 198, 255}, 0.85F)
            : Fade(Color{56,  202, 168, 255}, 0.48F);
        DrawRectangleRounded(layout.boardRect, 0.05F, 10, tint);

        const char* title    = state.gameOver ? "No More Moves" : "2048 Reached!";
        const char* subtitle = state.gameOver ? "R to restart  |  U to rewind"
                                              : "Keep playing — Space for AI";

        // Scale with board width so text always fits
        const float bw    = layout.boardRect.width;
        const int titleFs = std::clamp(static_cast<int>(bw * 0.075F), 28, 52);
        const int subFs   = std::clamp(static_cast<int>(bw * 0.034F), 16, 22);

        // Shrink title if it still overflows
        int tfs = titleFs;
        while (tfs > 20 && MeasureText(title, tfs) > static_cast<int>(bw - 40)) tfs -= 2;

        const int titleW = MeasureText(title,    tfs);
        const int subW   = MeasureText(subtitle, subFs);

        const int titleX = static_cast<int>(layout.boardRect.x + (bw - static_cast<float>(titleW)) * 0.5F);
        const int titleY = static_cast<int>(layout.boardRect.y + layout.boardRect.height * 0.38F);
        DrawText(title, titleX, titleY, tfs, Color{80, 72, 64, 255});

        const float subX = layout.boardRect.x + (bw - static_cast<float>(subW)) * 0.5F;
        const float subY = static_cast<float>(titleY) + static_cast<float>(tfs) + 14.0F;
        DrawRectangleRounded(
            {subX - 16.0F, subY - 6.0F, static_cast<float>(subW) + 32.0F, static_cast<float>(subFs) + 14.0F},
            0.40F, 8, Fade(Color{100, 88, 76, 255}, 0.14F));
        DrawText(subtitle, static_cast<int>(subX), static_cast<int>(subY), subFs, Color{100, 88, 76, 255});
    }

    if (!state.showHelp) {
        return;
    }

    const Rectangle modal {
        layout.boardRect.x + 28.0F,
        layout.boardRect.y + 28.0F,
        layout.boardRect.width  - 56.0F,
        layout.boardRect.height - 56.0F
    };
    DrawRectangleRounded(modal, 0.05F, 10, Fade(Color{249, 246, 242, 255}, 0.96F));
    DrawRectangleLinesEx(modal, 1.5F, Fade(Color{143, 122, 102, 255}, 0.22F));

    // Scale all modal text with modal width
    const float mw     = modal.width;
    const int headFs   = std::clamp(static_cast<int>(mw * 0.058F), 20, 34);
    const int bodyFs   = std::clamp(static_cast<int>(mw * 0.038F), 14, 22);
    const int smallFs  = std::clamp(static_cast<int>(mw * 0.030F), 12, 18);
    const float pad    = 22.0F;
    const float lineH  = static_cast<float>(bodyFs) + 8.0F;
    const float sLineH = static_cast<float>(smallFs) + 6.0F;

    BeginScissorMode(
        static_cast<int>(modal.x), static_cast<int>(modal.y),
        static_cast<int>(modal.width), static_cast<int>(modal.height)
    );

    float y = modal.y + pad;
    DrawText("Help", static_cast<int>(modal.x + pad), static_cast<int>(y), headFs, Color{119, 110, 101, 255});
    y += static_cast<float>(headFs) + 14.0F;

    DrawText("Deterministic 2048 with AI and benchmark mode.",
             static_cast<int>(modal.x + pad), static_cast<int>(y), bodyFs, Color{58, 59, 84, 255});
    y += lineH + 8.0F;

    DrawText("Move: Arrows or WASD",
             static_cast<int>(modal.x + pad), static_cast<int>(y), bodyFs, Color{58, 59, 84, 255}); y += lineH;
    DrawText("R: restart   U: undo   Space: autoplay   N: AI step",
             static_cast<int>(modal.x + pad), static_cast<int>(y), bodyFs, Color{58, 59, 84, 255}); y += lineH;
    DrawText("Tab: switch AI (greedy / expectimax)",
             static_cast<int>(modal.x + pad), static_cast<int>(y), bodyFs, Color{58, 59, 84, 255}); y += lineH;
    DrawText("T: cycle animation speed",
             static_cast<int>(modal.x + pad), static_cast<int>(y), bodyFs, Color{58, 59, 84, 255}); y += lineH;
    DrawText("Esc / H / F1: close help",
             static_cast<int>(modal.x + pad), static_cast<int>(y), bodyFs, Color{58, 59, 84, 255}); y += lineH + 10.0F;

    DrawText("CLI benchmark:",
             static_cast<int>(modal.x + pad), static_cast<int>(y), smallFs, Color{143, 122, 102, 255}); y += sLineH;
    DrawText("--benchmark N  --ai expectimax  --time-budget-ms 10",
             static_cast<int>(modal.x + pad), static_cast<int>(y), smallFs, Color{143, 122, 102, 255});

    EndScissorMode();
}

}  // namespace game2048
