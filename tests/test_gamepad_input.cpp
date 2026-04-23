#include "../src/gamepad_input.h"
#include "test_framework.h"

namespace {

using game2048::DefaultGamepadBindings;
using game2048::Direction;
using game2048::GamepadInputRouter;
using game2048::InputCommand;
using game2048::RawGamepadState;

TEST_CASE(GamepadInput_Stick_Recenter_IsRequired_BetweenMoves) {
    GamepadInputRouter router(DefaultGamepadBindings());
    RawGamepadState state {};
    state.connected = true;
    state.leftX = 0.85F;

    const auto first = router.ResolvePressedMove(state);
    const auto second = router.ResolvePressedMove(state);

    EXPECT_EQ(first, Direction::Right);
    EXPECT_FALSE(second.has_value());

    state.leftX = 0.0F;
    router.ResolvePressedMove(state);
    state.leftX = 0.85F;
    const auto third = router.ResolvePressedMove(state);
    EXPECT_EQ(third, Direction::Right);
}

TEST_CASE(GamepadInput_DefaultButtons_MapToExpectedCommands) {
    GamepadInputRouter router(DefaultGamepadBindings());
    RawGamepadState state {};
    state.connected = true;
    state.pressed[7] = true;

    EXPECT_EQ(router.ResolveCommand(state), InputCommand::ToggleHelp);
}

}  // namespace
