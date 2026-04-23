#include "../src/app/interaction_session.h"
#include "test_framework.h"

namespace {

using game2048::ControlMode;
using game2048::Direction;
using game2048::InputCommand;
using game2048::InputGate;
using game2048::InteractionInput;
using game2048::InteractionSession;
using game2048::OverlayMode;

InteractionInput MakeInput(bool reached2048Ever = false, bool gameOver = false) {
    InteractionInput input;
    input.reached2048Ever = reached2048Ever;
    input.gameOver = gameOver;
    return input;
}

}  // namespace

TEST_CASE(Interaction_FirstReach2048_ShowsVictoryOnlyOncePerTimeline) {
    InteractionSession session(123);

    session.Tick(MakeInput(false, false));
    EXPECT_EQ(session.Overlay(), OverlayMode::None);

    session.Tick(MakeInput(true, false));
    EXPECT_EQ(session.Overlay(), OverlayMode::Victory);

    InteractionInput dismiss = MakeInput(true, false);
    dismiss.command = InputCommand::Exit;
    session.Tick(dismiss);
    EXPECT_EQ(session.Overlay(), OverlayMode::None);

    session.Tick(MakeInput(true, false));
    EXPECT_EQ(session.Overlay(), OverlayMode::None);

    session.Tick(MakeInput(false, false));
    session.Tick(MakeInput(true, false));
    EXPECT_EQ(session.Overlay(), OverlayMode::Victory);
}

TEST_CASE(Interaction_HelpOverlay_BlocksAndClearsBufferedMoves) {
    InteractionSession session(123);

    InteractionInput animateLeft = MakeInput(false, false);
    animateLeft.pressedMove = Direction::Left;
    animateLeft.animationBlocksInput = true;
    session.Tick(animateLeft);
    EXPECT_TRUE(session.PendingMove().has_value());

    InteractionInput openHelp = MakeInput(false, false);
    openHelp.command = InputCommand::ToggleHelp;
    session.Tick(openHelp);
    EXPECT_EQ(session.Overlay(), OverlayMode::Help);
    EXPECT_EQ(session.Gate(), InputGate::BlockedByOverlay);
    EXPECT_FALSE(session.PendingMove().has_value());

    InteractionInput moveWhileHelp = MakeInput(false, false);
    moveWhileHelp.pressedMove = Direction::Up;
    session.Tick(moveWhileHelp);
    EXPECT_FALSE(session.PendingMove().has_value());

    InteractionInput closeHelp = MakeInput(false, false);
    closeHelp.command = InputCommand::Exit;
    session.Tick(closeHelp);
    EXPECT_EQ(session.Overlay(), OverlayMode::None);

    const auto actions = session.Tick(MakeInput(false, false));
    EXPECT_FALSE(actions.moveToExecute.has_value());
}

TEST_CASE(Interaction_GameOverOverlay_PausesAutoplay) {
    InteractionSession session(123);

    InteractionInput enableAuto = MakeInput(false, false);
    enableAuto.command = InputCommand::ToggleAutoAI;
    const auto enabled = session.Tick(enableAuto);
    EXPECT_EQ(session.Control(), ControlMode::AIAutoplay);
    EXPECT_TRUE(enabled.aiMoveRequested);

    session.Tick(MakeInput(false, true));
    EXPECT_EQ(session.Overlay(), OverlayMode::GameOver);
    EXPECT_EQ(session.Gate(), InputGate::BlockedByOverlay);

    const auto paused = session.Tick(MakeInput(false, true));
    EXPECT_FALSE(paused.aiMoveRequested);
}

TEST_CASE(Interaction_AnimationBlock_KeepsOnlyLatestBufferedMove) {
    InteractionSession session(123);

    InteractionInput first = MakeInput(false, false);
    first.pressedMove = Direction::Left;
    first.animationBlocksInput = true;
    session.Tick(first);
    EXPECT_EQ(session.PendingMove(), Direction::Left);

    InteractionInput second = MakeInput(false, false);
    second.pressedMove = Direction::Up;
    second.animationBlocksInput = true;
    session.Tick(second);
    EXPECT_EQ(session.PendingMove(), Direction::Up);

    const auto actions = session.Tick(MakeInput(false, false));
    EXPECT_EQ(actions.moveToExecute, Direction::Up);
    EXPECT_FALSE(session.PendingMove().has_value());
}

TEST_CASE(Interaction_VictoryOverlay_MoveDismissesAndExecutes) {
    InteractionSession session(123);

    session.Tick(MakeInput(true, false));
    EXPECT_EQ(session.Overlay(), OverlayMode::Victory);

    InteractionInput move = MakeInput(true, false);
    move.pressedMove = Direction::Right;
    const auto actions = session.Tick(move);

    EXPECT_EQ(session.Overlay(), OverlayMode::None);
    EXPECT_EQ(session.Control(), ControlMode::Human);
    EXPECT_EQ(actions.moveToExecute, Direction::Right);
}

TEST_CASE(Interaction_UndoWithoutHistory_DoesNotInvalidateHint) {
    InteractionSession session(123);

    InteractionInput undo = MakeInput(false, false);
    undo.command = InputCommand::Undo;
    const auto actions = session.Tick(undo);

    EXPECT_TRUE(actions.undoRequested);
    EXPECT_FALSE(actions.hintInvalidated);
}
