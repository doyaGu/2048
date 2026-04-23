#include "../src/ui/layout.h"
#include "../src/input/pointer_input.h"
#include "test_framework.h"

namespace {

using game2048::ComputeLayout;
using game2048::Direction;
using game2048::PointerGestureState;
using game2048::PointerInputRouter;
using game2048::RawPointerState;

TEST_CASE(PointerInput_Swipe_Resolves_DominantAxis) {
    const auto layout = ComputeLayout(1280, 720);
    PointerGestureState gesture {};
    PointerInputRouter router;

    RawPointerState begin {};
    begin.connected = true;
    begin.pressed = true;
    begin.down = true;
    begin.position = {layout.boardRect.x + 40.0F, layout.boardRect.y + 40.0F};
    router.UpdateGesture(begin, layout, gesture);

    RawPointerState drag = begin;
    drag.pressed = false;
    drag.position.x += 160.0F;
    drag.position.y += 20.0F;
    const auto resolved = router.ResolveMove(drag, layout, gesture);

    EXPECT_EQ(resolved, Direction::Right);
}

TEST_CASE(PointerInput_Tap_BelowThreshold_DoesNotMove) {
    const auto layout = ComputeLayout(1280, 720);
    PointerGestureState gesture {};
    PointerInputRouter router;

    RawPointerState begin {};
    begin.connected = true;
    begin.pressed = true;
    begin.down = true;
    begin.position = {layout.boardRect.x + 60.0F, layout.boardRect.y + 60.0F};
    router.UpdateGesture(begin, layout, gesture);

    RawPointerState end = begin;
    end.pressed = false;
    end.down = false;
    end.released = true;
    end.position.x += 8.0F;
    const auto resolved = router.ResolveMove(end, layout, gesture);

    EXPECT_FALSE(resolved.has_value());
}

}  // namespace
