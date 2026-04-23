#include "input/pointer_input.h"

#include <cmath>

namespace game2048 {

namespace {

constexpr float kSwipeThreshold = 30.0F;

bool Contains(const Rectangle& rect, const InputPoint& point) {
    return point.x >= rect.x && point.x <= rect.x + rect.width &&
           point.y >= rect.y && point.y <= rect.y + rect.height;
}

}  // namespace

void PointerInputRouter::UpdateGesture(const RawPointerState& pointer, const LayoutMetrics& layout, PointerGestureState& state) const {
    if (pointer.pressed && Contains(layout.boardGestureRect, pointer.position)) {
        state.tracking = true;
        state.resolved = false;
        state.start = pointer.position;
        state.pointerId = pointer.pointerId;
    } else if (!pointer.down && !pointer.pressed && !pointer.released) {
        state.tracking = false;
        state.resolved = false;
    }
}

std::optional<Direction> PointerInputRouter::ResolveMove(const RawPointerState& pointer,
                                                         const LayoutMetrics& layout,
                                                         PointerGestureState& state) const {
    if (!state.tracking || state.resolved || !Contains(layout.boardGestureRect, state.start)) {
        return std::nullopt;
    }

    const float dx = pointer.position.x - state.start.x;
    const float dy = pointer.position.y - state.start.y;
    if (std::fabs(dx) < kSwipeThreshold && std::fabs(dy) < kSwipeThreshold) {
        if (pointer.released) {
            state.tracking = false;
        }
        return std::nullopt;
    }

    state.resolved = true;
    state.tracking = pointer.down;
    if (std::fabs(dx) >= std::fabs(dy)) {
        return dx >= 0.0F ? Direction::Right : Direction::Left;
    }
    return dy >= 0.0F ? Direction::Down : Direction::Up;
}

ControlId PointerInputRouter::HitTestControl(const RawPointerState& pointer, const LayoutMetrics& layout) const {
    if (!pointer.connected) {
        return ControlId::None;
    }

    for (std::size_t index = 0; index < layout.controlCount; ++index) {
        if (Contains(layout.controlRects[index], pointer.position)) {
            return layout.controlIds[index];
        }
    }
    return ControlId::None;
}

}  // namespace game2048
