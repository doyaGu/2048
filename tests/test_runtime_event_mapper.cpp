#include "../src/app/runtime_event_mapper.h"
#include "test_framework.h"

namespace {

using game2048::Direction;
using game2048::InputFrame;
using game2048::OverlayMode;
using game2048::RuntimeEventMapper;
using game2048::RuntimeEventType;

}  // namespace

TEST_CASE(RuntimeEventMapper_PressedMove_EmitsMoveImmediately) {
    RuntimeEventMapper mapper;
    InputFrame frame;
    frame.pressedMove = Direction::Left;

    const auto events = mapper.BuildEvents(frame, OverlayMode::None, 1.0);

    EXPECT_EQ(events.size(), std::size_t {1});
    EXPECT_EQ(events[0].type, RuntimeEventType::Move);
    EXPECT_EQ(events[0].direction, Direction::Left);
}

TEST_CASE(RuntimeEventMapper_HeldMove_RepeatsAfterDelay) {
    RuntimeEventMapper mapper;
    InputFrame frame;
    frame.heldMove = Direction::Right;

    EXPECT_TRUE(mapper.BuildEvents(frame, OverlayMode::None, 1.0).empty());
    EXPECT_TRUE(mapper.BuildEvents(frame, OverlayMode::None, 1.10).empty());

    const auto repeated = mapper.BuildEvents(frame, OverlayMode::None, 1.20);
    EXPECT_EQ(repeated.size(), std::size_t {1});
    EXPECT_EQ(repeated[0].type, RuntimeEventType::Move);
    EXPECT_EQ(repeated[0].direction, Direction::Right);

    EXPECT_TRUE(mapper.BuildEvents(frame, OverlayMode::None, 1.25).empty());
    const auto second = mapper.BuildEvents(frame, OverlayMode::None, 1.275);
    EXPECT_EQ(second.size(), std::size_t {1});
    EXPECT_EQ(second[0].direction, Direction::Right);
}

TEST_CASE(RuntimeEventMapper_Command_ClearsHeldRepeat) {
    RuntimeEventMapper mapper;
    InputFrame frame;
    frame.heldMove = Direction::Up;

    mapper.BuildEvents(frame, OverlayMode::None, 1.0);
    frame.command = game2048::InputCommand::Undo;
    frame.heldMove.reset();
    const auto command = mapper.BuildEvents(frame, OverlayMode::None, 1.1);
    EXPECT_EQ(command.size(), std::size_t {1});
    EXPECT_EQ(command[0].type, RuntimeEventType::Undo);

    frame.command = game2048::InputCommand::None;
    frame.heldMove = Direction::Up;
    EXPECT_TRUE(mapper.BuildEvents(frame, OverlayMode::None, 1.2).empty());
}

