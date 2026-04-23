#pragma once

#include "ui/layout.h"
#include "app/view_state.h"

namespace game2048 {

class UI {
public:
    void DrawPanels(const LayoutMetrics& layout, const HUDState& state) const;
    void DrawOverlay(const LayoutMetrics& layout, const HUDState& state) const;
};

}  // namespace game2048
