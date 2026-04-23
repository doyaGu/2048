#pragma once

namespace game2048 {

struct LayoutMetrics;
struct RuntimeSnapshot;

void DrawPanelsView(const LayoutMetrics& layout, const RuntimeSnapshot& state);

}  // namespace game2048
