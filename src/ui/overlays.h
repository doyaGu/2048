#pragma once

namespace game2048 {

struct LayoutMetrics;
struct RuntimeSnapshot;

void DrawOverlayView(const LayoutMetrics& layout, const RuntimeSnapshot& state);

}  // namespace game2048
