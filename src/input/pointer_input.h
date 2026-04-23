#pragma once

#include <optional>

#include "input/input_types.h"
#include "ui/layout.h"

namespace game2048 {

struct PointerGestureState {
    bool tracking = false;
    bool resolved = false;
    InputPoint start {};
    std::uint64_t pointerId = 0;
};

class PointerInputRouter {
public:
    void UpdateGesture(const RawPointerState& pointer, const LayoutMetrics& layout, PointerGestureState& state) const;
    std::optional<Direction> ResolveMove(const RawPointerState& pointer, const LayoutMetrics& layout, PointerGestureState& state) const;
    ControlId HitTestControl(const RawPointerState& pointer, const LayoutMetrics& layout) const;
};

}  // namespace game2048
