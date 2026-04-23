#include "ui/panels.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>

#include <raylib.h>

#include "ui/names.h"
#include "ui/ui.h"
#include "ui/widgets.h"

namespace game2048 {

void DrawPanelsView(const LayoutMetrics& layout, const HUDState& state) {
    const int titleFs = std::clamp(static_cast<int>(layout.topBarTitleRect.height * 0.70F), 20, 42);
    DrawFittedText("2048 Engine",
                   layout.topBarTitleRect.x,
                   layout.topBarTitleRect.y + (layout.topBarTitleRect.height - static_cast<float>(titleFs)) * 0.5F,
                   layout.topBarTitleRect.width,
                   titleFs,
                   Color{119, 110, 101, 255});

    DrawMetricBox(layout.scoreBoxRects[0], "Score", std::to_string(state.game.score), Color{246, 124, 95, 255});
    DrawMetricBox(layout.scoreBoxRects[1], "Best", std::to_string(state.game.bestScore), Color{237, 200, 80, 255});

    DrawRectangleRounded(layout.panelRect, 0.05F, 10, Fade(Color{249, 246, 242, 255}, 0.92F));
    DrawRectangleLinesEx(layout.panelRect, 1.5F, Fade(Color{143, 122, 102, 255}, 0.18F));

    const float innerWidth = layout.panelRect.width - 28.0F;
    const float x = layout.panelRect.x + 14.0F;
    const PanelStyle style = MakeStyle(layout.panelRect.height, innerWidth);

    BeginScissorMode(static_cast<int>(layout.panelRect.x),
                     static_cast<int>(layout.panelRect.y),
                     static_cast<int>(layout.panelRect.width),
                     static_cast<int>(layout.panelRect.height));

    float y = layout.panelRect.y + std::max(8.0F, 12.0F * style.scale);

    DrawSectionHeader(layout.panelRect, y, "Status", style);
    const std::array<std::pair<const char*, std::string>, 8> statusLines {{
        {"Mode", std::string(ControlModeName(state.session.controlMode))},
        {"AI", std::string(AgentName(state.ai.agentKind))},
        {"Gate", std::string(InputGateName(state.session.inputGate))},
        {"Anim", std::string(AnimationSpeedName(state.session.animationSpeed))},
        {"Max Tile", std::to_string(state.game.maxTile)},
        {"Goal", state.game.achieved2048 ? "2048 reached" : "In progress"},
        {"Seed", std::to_string(state.game.seed)},
        {"Hint", state.ai.recommendation.valid ? std::string(DirectionName(state.ai.recommendation.direction)) : "-"},
    }};
    for (const auto& [label, value] : statusLines) {
        DrawInfoLine(x, y, innerWidth, label, value, style);
    }

    y += style.divGap;
    DrawDivider(layout.panelRect, y);
    y += style.divGap;
    DrawSectionHeader(layout.panelRect, y, "Search", style);
    const std::array<std::pair<const char*, std::string>, 6> searchLines {{
        {"Depth", std::to_string(state.ai.lastSearch.maxDepthReached)},
        {"Nodes", std::to_string(state.ai.lastSearch.nodes)},
        {"TT Hits", std::to_string(state.ai.lastSearch.cacheHits)},
        {"Eval", TextFormat("%.1f", state.ai.lastSearch.evaluation)},
        {"Think ms", TextFormat("%.2f", state.ai.lastSearch.elapsedMs)},
        {"FPS", std::to_string(GetFPS())},
    }};
    for (const auto& [label, value] : searchLines) {
        DrawInfoLine(x, y, innerWidth, label, value, style);
    }

    y += style.divGap;
    DrawDivider(layout.panelRect, y);
    y += style.divGap;
    DrawSectionHeader(layout.panelRect, y, "Evaluator", style);
    struct EvalRowDef { const char* label; double EvaluatorHUD::* field; };
    static constexpr std::array<EvalRowDef, 7> kEvalRows {{
        {"Empty", &EvaluatorHUD::emptyTiles},
        {"Mono", &EvaluatorHUD::monotonicity},
        {"Smooth", &EvaluatorHUD::smoothness},
        {"Corner", &EvaluatorHUD::cornerMax},
        {"Merge", &EvaluatorHUD::mergePotential},
        {"Snake", &EvaluatorHUD::snakePattern},
        {"Trap", &EvaluatorHUD::trapPenalty},
    }};
    for (const auto& row : kEvalRows) {
        DrawEvalRow(x, y, innerWidth, row.label, state.ai.evaluatorBreakdown.*row.field, style);
    }

    y += style.divGap;
    DrawDivider(layout.panelRect, y);
    y += style.divGap;
    DrawSectionHeader(layout.panelRect, y, "Keys", style);
    static constexpr std::array<const char*, 6> kKeyLines {{
        "Move: Arrows / WASD",
        "Auto: Space",
        "Step: N",
        "Restart: R    Undo: U",
        "AI: Tab    Speed: T",
        "Help: H / F1",
    }};
    const Color keyColor = {58, 59, 84, 255};
    for (const char* line : kKeyLines) {
        DrawFittedText(line, x, y, innerWidth, style.keyFs, keyColor);
        y += style.keyRowH;
    }

    EndScissorMode();

    for (std::size_t index = 0; index < layout.controlCount; ++index) {
        const ControlId id = layout.controlIds[index];
        const bool active = id == ControlId::ToggleAutoAI && state.session.controlMode == HUDControlMode::AIAutoplay;
        const bool disabled = state.session.overlayMode != HUDOverlayMode::None;
        const auto label = ControlLabel(id);
        DrawControlButton(layout.controlRects[index], label.data(), active, disabled);
    }
}

}  // namespace game2048
