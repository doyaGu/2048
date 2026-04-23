#include "ui/ui.h"

#include "ui/overlays.h"
#include "ui/panels.h"

namespace game2048 {

void UI::DrawPanels(const LayoutMetrics& layout, const HUDState& state) const {
    DrawPanelsView(layout, state);
}

void UI::DrawOverlay(const LayoutMetrics& layout, const HUDState& state) const {
    DrawOverlayView(layout, state);
}

}  // namespace game2048
