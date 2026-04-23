#include "../src/animation.h"
#include "test_framework.h"

namespace {

using game2048::AnimationController;
using game2048::AnimationSpeed;
using game2048::Board;
using game2048::Direction;
using game2048::SpawnEvent;

TEST_CASE(Animation_Reset_ClearsActiveMoveAndSpawnState) {
    AnimationController animation;
    animation.SetSpeed(AnimationSpeed::Normal);

    Board before = Board::FromRows({{{2, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}});
    Board after = before;
    const auto move = after.ApplyMove(Direction::Right);

    animation.Start(before, after, move.trace, SpawnEvent{{0, 0}, 2});
    EXPECT_TRUE(animation.Active());
    EXPECT_TRUE(animation.IsMovingTo({0, 3}));
    EXPECT_TRUE(animation.IsSpawnCell({0, 0}));

    animation.Reset();

    EXPECT_FALSE(animation.Active());
    EXPECT_FALSE(animation.IsMovingTo({0, 3}));
    EXPECT_FALSE(animation.IsSpawnCell({0, 0}));
}

}  // namespace
