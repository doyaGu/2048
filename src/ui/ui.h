#pragma once

#include "ui/layout.h"
#include "runtime/runtime_types.h"

namespace game2048 {

class UI {
public:
    void DrawPanels(const LayoutMetrics& layout, const RuntimeSnapshot& state) const;
    void DrawOverlay(const LayoutMetrics& layout, const RuntimeSnapshot& state) const;
};

}  // namespace game2048
