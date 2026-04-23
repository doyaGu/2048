#include <cmath>

#include "../src/input/input_bindings.h"
#include "../src/input/input_system.h"
#include "../src/input/input_types.h"
#include "../src/ui/layout.h"
#include "test_framework.h"

namespace {

using game2048::ControlId;
using game2048::ComputeLayout;
using game2048::DefaultGamepadBindings;
using game2048::GamepadBindingMap;
using game2048::InputFrame;
using game2048::InputSystem;
using game2048::OverlayMode;
using game2048::RawInputState;
using game2048::RawCommandKeySlot;

TEST_CASE(InputTypes_DefaultBindings_AreStable) {
    RawInputState raw {};
    InputFrame frame {};
    const GamepadBindingMap bindings = DefaultGamepadBindings();

    EXPECT_EQ(raw.gamepads[0].connected, false);
    EXPECT_EQ(frame.primaryControl, ControlId::None);
    EXPECT_TRUE(bindings.faceBottom >= 0);
    EXPECT_TRUE(bindings.faceRight >= 0);
    EXPECT_TRUE(bindings.dpadUp >= 0);
}

TEST_CASE(InputSystem_Collapses_To_One_Command_And_One_Move) {
    InputSystem system;
    RawInputState raw {};
    const auto layout = ComputeLayout(1280, 720);

    raw.keyboard.pressedCommands[static_cast<std::size_t>(RawCommandKeySlot::Reset)] = true;
    raw.pointers[0].connected = true;
    raw.pointers[0].pressed = true;
    raw.pointers[0].down = true;
    raw.pointers[0].position = {layout.boardRect.x + 40.0F, layout.boardRect.y + 40.0F};

    const auto frame = system.BuildFrame(raw, layout, false, false, OverlayMode::None);

    EXPECT_EQ(frame.command, game2048::InputCommand::Reset);
    EXPECT_FALSE(frame.pressedMove.has_value());
}

TEST_CASE(InputSystem_GamepadCommands_TakePriority_OverKeyboardCommands) {
    InputSystem system;
    RawInputState raw {};
    const auto layout = ComputeLayout(1280, 720);
    const auto bindings = DefaultGamepadBindings();

    raw.keyboard.pressedCommands[static_cast<std::size_t>(RawCommandKeySlot::Reset)] = true;
    raw.gamepads[0].connected = true;
    raw.gamepads[0].pressed[static_cast<std::size_t>(bindings.back)] = true;

    const auto frame = system.BuildFrame(raw, layout, false, false, OverlayMode::None);

    EXPECT_EQ(frame.command, game2048::InputCommand::ToggleAutoAI);
}

TEST_CASE(Layout_Desktop_Controls_ArePresent) {
    const auto layout = ComputeLayout(1280, 720);
    EXPECT_TRUE(layout.controlCount >= 4);
    EXPECT_EQ(layout.showTouchHud, false);
}

TEST_CASE(Layout_DesktopMode_Hides_DirectionalButtons) {
    const auto layout = ComputeLayout(1280, 720, false);
    bool sawMoveControl = false;
    bool sawRestart = false;
    bool sawUndo = false;
    bool sawAuto = false;
    bool sawHelp = false;

    for (std::size_t index = 0; index < layout.controlCount; ++index) {
        const auto id = layout.controlIds[index];
        if (id == ControlId::MoveUp || id == ControlId::MoveDown ||
            id == ControlId::MoveLeft || id == ControlId::MoveRight) {
            sawMoveControl = true;
        }
        sawRestart |= id == ControlId::Restart;
        sawUndo |= id == ControlId::Undo;
        sawAuto |= id == ControlId::ToggleAutoAI;
        sawHelp |= id == ControlId::ToggleHelp;
    }

    EXPECT_FALSE(sawMoveControl);
    EXPECT_TRUE(sawRestart);
    EXPECT_TRUE(sawUndo);
    EXPECT_TRUE(sawAuto);
    EXPECT_TRUE(sawHelp);
}

TEST_CASE(Layout_TouchHud_Exposes_MoreControls_And_Shrinks_GestureZone) {
    const auto desktop = ComputeLayout(900, 1200, false);
    const auto touch = ComputeLayout(900, 1200, true);

    EXPECT_TRUE(touch.showTouchHud);
    EXPECT_TRUE(touch.controlCount > desktop.controlCount);
    EXPECT_TRUE(touch.boardGestureRect.height < touch.boardRect.height);
}

TEST_CASE(Layout_DesktopCluster_IsCenteredWithinTolerance) {
    const auto layout = ComputeLayout(1280, 720, false);
    const float contentLeft = 28.0F;
    const float contentRight = 1280.0F - 28.0F;
    const float contentCenter = (contentLeft + contentRight) * 0.5F;
    const float clusterCenter = layout.clusterRect.x + layout.clusterRect.width * 0.5F;

    EXPECT_TRUE(std::fabs(clusterCenter - contentCenter) <= 8.0F);
}

TEST_CASE(Layout_DesktopOuterMargins_AreBalanced) {
    const auto layout = ComputeLayout(1280, 720, false);
    const float contentLeft = 28.0F;
    const float contentRight = 1280.0F - 28.0F;
    const float leftMargin = layout.clusterRect.x - contentLeft;
    const float rightMargin = contentRight - (layout.clusterRect.x + layout.clusterRect.width);

    EXPECT_TRUE(std::fabs(leftMargin - rightMargin) <= 24.0F);
}

TEST_CASE(Layout_TopBand_FallsBackToTwoRows_WhenNarrow) {
    const auto layout = ComputeLayout(900, 720, false);

    EXPECT_TRUE(layout.topBarTitleRect.height > 0.0F);
    EXPECT_TRUE(layout.scoreBoxRects[0].height > 0.0F);
    EXPECT_TRUE(layout.scoreBoxRects[1].height > 0.0F);
    EXPECT_TRUE(layout.topBarUsesTwoRows);
    EXPECT_TRUE(layout.topBarTitleRect.y + layout.topBarTitleRect.height <= layout.scoreBoxRects[0].y ||
                layout.scoreBoxRects[0].y + layout.scoreBoxRects[0].height <= layout.topBarTitleRect.y);
}

TEST_CASE(Layout_DesktopPanel_RemainsWithinClusterBounds) {
    const auto layout = ComputeLayout(1280, 720, false);

    EXPECT_TRUE(layout.panelRect.x >= layout.clusterRect.x);
    EXPECT_TRUE(layout.panelRect.x + layout.panelRect.width <= layout.clusterRect.x + layout.clusterRect.width);
    EXPECT_TRUE(layout.panelRect.width >= 280.0F);
}

TEST_CASE(Layout_NarrowWindow_StaysWithinContentBounds) {
    const auto layout = ComputeLayout(320, 300, false);
    const float contentLeft = 28.0F;
    const float contentTop = 28.0F;
    const float contentRight = 320.0F - 28.0F;
    const float contentBottom = 300.0F - 28.0F;

    EXPECT_TRUE(layout.clusterRect.x >= contentLeft);
    EXPECT_TRUE(layout.clusterRect.y >= contentTop);
    EXPECT_TRUE(layout.clusterRect.x + layout.clusterRect.width <= contentRight);
    EXPECT_TRUE(layout.clusterRect.y + layout.clusterRect.height <= contentBottom);
}

TEST_CASE(Layout_HelpOverlay_UsesSingleCenteredActionButton) {
    const auto layout = ComputeLayout(1280, 720, false);
    const Rectangle helpButton = OverlayActionRect(layout, OverlayMode::Help, 0);
    const float boardCenter = layout.boardRect.x + layout.boardRect.width * 0.5F;
    const float buttonCenter = helpButton.x + helpButton.width * 0.5F;

    EXPECT_EQ(OverlayActionCount(OverlayMode::Help), std::size_t {1});
    EXPECT_NEAR(buttonCenter, boardCenter, 0.01);
}

TEST_CASE(InputSystem_HelpOverlay_GamepadButtons_DoNotLeak_ToGameplayCommands) {
    InputSystem system;
    RawInputState raw {};
    const auto layout = ComputeLayout(1280, 720);
    const auto bindings = DefaultGamepadBindings();

    raw.gamepads[0].connected = true;
    raw.gamepads[0].pressed[static_cast<std::size_t>(bindings.back)] = true;

    const auto frame = system.BuildFrame(raw, layout, false, false, OverlayMode::Help);

    EXPECT_EQ(frame.command, game2048::InputCommand::Exit);
    EXPECT_EQ(frame.primaryControl, ControlId::OverlayPrimary);
}

TEST_CASE(InputSystem_HelpOverlay_OnlyCenteredButtonIsClickable) {
    InputSystem system;
    RawInputState raw {};
    const auto layout = ComputeLayout(1280, 720);
    const Rectangle helpButton = OverlayActionRect(layout, OverlayMode::Help, 0);

    raw.pointers[0].connected = true;
    raw.pointers[0].pressed = true;
    raw.pointers[0].position = {helpButton.x + helpButton.width * 0.5F, helpButton.y + helpButton.height * 0.5F};

    const auto centered = system.BuildFrame(raw, layout, false, false, OverlayMode::Help);
    EXPECT_EQ(centered.command, game2048::InputCommand::Exit);

    raw.pointers[0].position = {layout.overlayActionRects[1].x + layout.overlayActionRects[1].width * 0.5F,
                                layout.overlayActionRects[1].y + layout.overlayActionRects[1].height * 0.5F};

    const auto stray = system.BuildFrame(raw, layout, false, false, OverlayMode::Help);
    EXPECT_EQ(stray.command, game2048::InputCommand::None);
}

TEST_CASE(InputSystem_VictoryOverlay_BackButton_TogglesAutoplay) {
    InputSystem system;
    RawInputState raw {};
    const auto layout = ComputeLayout(1280, 720);
    const auto bindings = DefaultGamepadBindings();

    raw.gamepads[0].connected = true;
    raw.gamepads[0].pressed[static_cast<std::size_t>(bindings.back)] = true;

    const auto frame = system.BuildFrame(raw, layout, false, false, OverlayMode::Victory);

    EXPECT_EQ(frame.command, game2048::InputCommand::ToggleAutoAI);
    EXPECT_EQ(frame.primaryControl, ControlId::ToggleAutoAI);
}

TEST_CASE(InputSystem_GameOverOverlay_BackButton_Exits) {
    InputSystem system;
    RawInputState raw {};
    const auto layout = ComputeLayout(1280, 720);
    const auto bindings = DefaultGamepadBindings();

    raw.gamepads[0].connected = true;
    raw.gamepads[0].pressed[static_cast<std::size_t>(bindings.back)] = true;

    const auto frame = system.BuildFrame(raw, layout, false, false, OverlayMode::GameOver);

    EXPECT_EQ(frame.command, game2048::InputCommand::Exit);
    EXPECT_EQ(frame.primaryControl, ControlId::Exit);
}

}  // namespace
