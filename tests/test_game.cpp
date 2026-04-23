#include <array>

#include "../src/board.h"
#include "../src/game.h"
#include "../src/rng.h"
#include "test_framework.h"

namespace {

using game2048::Board;
using game2048::Direction;
using game2048::Game;
using game2048::GameSnapshot;
using game2048::Random;

Board MakeBoard(const std::array<std::array<int, 4>, 4>& rows) {
    return Board::FromRows(rows);
}

}  // namespace

TEST_CASE(Game_InvalidMove_DoesNotSpawn) {
    Game game(999);
    Random rng(123456);
    const Board invalidLeft = MakeBoard({{{2, 0, 0, 0}, {2, 0, 0, 0}, {4, 8, 16, 32}, {64, 128, 256, 512}}});
    const GameSnapshot snapshot {invalidLeft, 777, false, false, rng.State(), 123456};

    game.Restore(snapshot);
    const Board before = game.GetBoard();
    const auto turn = game.ApplyMove(Direction::Left);

    EXPECT_FALSE(turn.moved);
    EXPECT_FALSE(turn.spawn.has_value());
    EXPECT_EQ(turn.scoreDelta, 0U);
    EXPECT_EQ(game.GetBoard(), before);
    EXPECT_EQ(game.Score(), 777U);
    EXPECT_FALSE(game.CanUndo());
}

TEST_CASE(Board_GameOver_Correctness) {
    const Board dead = MakeBoard({{{2, 4, 2, 4}, {4, 2, 4, 2}, {2, 4, 2, 4}, {4, 2, 4, 2}}});
    const Board hasEmpty = MakeBoard({{{2, 4, 2, 4}, {4, 2, 0, 2}, {2, 4, 2, 4}, {4, 2, 4, 2}}});
    const Board hasMerge = MakeBoard({{{2, 4, 2, 4}, {4, 2, 4, 2}, {2, 4, 4, 4}, {4, 2, 4, 2}}});

    EXPECT_FALSE(dead.CanMove());
    EXPECT_TRUE(hasEmpty.CanMove());
    EXPECT_TRUE(hasMerge.CanMove());
}

TEST_CASE(Game_Reset_Is_Reproducible_With_Same_Seed) {
    Game first(12345);
    Game second(12345);

    EXPECT_EQ(first.GetBoard(), second.GetBoard());

    const auto firstTurn = first.ApplyMove(Direction::Left);
    const auto secondTurn = second.ApplyMove(Direction::Left);

    EXPECT_EQ(firstTurn.moved, secondTurn.moved);
    EXPECT_EQ(firstTurn.scoreDelta, secondTurn.scoreDelta);
    EXPECT_EQ(first.GetBoard(), second.GetBoard());
}

TEST_CASE(Game_Undo_Restores_Rng_Timeline) {
    Game game(123);
    const auto firstTurn = game.ApplyMove(Direction::Left);
    EXPECT_TRUE(firstTurn.moved);
    const Board afterFirst = game.GetBoard();

    EXPECT_TRUE(game.Undo());
    const auto replayTurn = game.ApplyMove(Direction::Left);
    EXPECT_TRUE(replayTurn.moved);
    EXPECT_EQ(replayTurn.scoreDelta, firstTurn.scoreDelta);
    EXPECT_EQ(game.GetBoard(), afterFirst);
}

TEST_CASE(Board_Reaching2048_DoesNotPrevent_FurtherMoves) {
    Board board = MakeBoard({{{1024, 1024, 4, 4}, {2, 4, 8, 16}, {0, 0, 0, 0}, {0, 0, 0, 0}}});

    const auto first = board.ApplyMove(Direction::Left);
    EXPECT_TRUE(first.changed);
    EXPECT_EQ(board.At(0, 0), 2048);
    EXPECT_TRUE(board.CanMove());

    const auto second = board.ApplyMove(Direction::Down);
    EXPECT_TRUE(second.changed);
}

TEST_CASE(Game_Reset_Recreates_SameOpening_ForSameSeed) {
    Game game(314159);
    const Board firstOpening = game.GetBoard();

    game.Reset(314159);

    EXPECT_EQ(game.GetBoard(), firstOpening);
}
