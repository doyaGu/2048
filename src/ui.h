#pragma once

#include <cstdint>
#include <string>

#include "ai/ai_engine.h"
#include "ai/evaluator.h"
#include "animation.h"
#include "interaction_session.h"
#include "layout.h"
#include "stats.h"

namespace game2048 {

struct HUDState {
    std::uint32_t score = 0;
    std::uint32_t bestScore = 0;
    int maxTile = 0;
    bool gameOver = false;
    bool achieved2048 = false;
    bool canDismissOverlay = false;
    bool showContinueHint = false;
    std::uint64_t seed = 0;
    ControlMode controlMode = ControlMode::Human;
    OverlayMode overlayMode = OverlayMode::None;
    InputGate inputGate = InputGate::Accepting;
    ai::AgentKind agentKind = ai::AgentKind::Expectimax;
    ai::MoveDecision recommendation {};
    SearchStats lastSearch {};
    ai::FeatureBreakdown evaluatorBreakdown {};
    AnimationSpeed animationSpeed = AnimationSpeed::Normal;
};

class UI {
public:
    void DrawPanels(const LayoutMetrics& layout, const HUDState& state) const;
    void DrawOverlay(const LayoutMetrics& layout, const HUDState& state) const;
};

}  // namespace game2048
