#include <array>

#include "../src/core/board.h"
#include "../src/core/game.h"
#include "../src/core/rng.h"
#include "test_framework.h"

namespace {

using game2048::Board;
using game2048::Direction;

Board MakeBoard(const std::array<std::array<int, 4>, 4>& rows) {
    return Board::FromRows(rows);
}

std::array<int, 16> Flatten(const Board& board) {
    std::array<int, 16> out {};
    std::size_t index = 0;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            out[index] = board.At(row, col);
            ++index;
        }
    }
    return out;
}

}  // namespace

TEST_CASE(Move_Left_Merges_2220_To_4200) {
    auto board = MakeBoard({{{2, 2, 2, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}});
    const auto result = board.ApplyMove(Direction::Left);

    EXPECT_TRUE(result.changed);
    EXPECT_EQ(result.scoreDelta, 4U);
    EXPECT_EQ(Flatten(board), (std::array<int, 16>{4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}));
}

TEST_CASE(Move_Left_Merges_2222_To_4400) {
    auto board = MakeBoard({{{2, 2, 2, 2}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}});
    const auto result = board.ApplyMove(Direction::Left);

    EXPECT_TRUE(result.changed);
    EXPECT_EQ(result.scoreDelta, 8U);
    EXPECT_EQ(Flatten(board), (std::array<int, 16>{4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}));
}

TEST_CASE(Move_Left_Merges_4044_To_8400) {
    auto board = MakeBoard({{{4, 0, 4, 4}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}});
    const auto result = board.ApplyMove(Direction::Left);

    EXPECT_TRUE(result.changed);
    EXPECT_EQ(result.scoreDelta, 8U);
    EXPECT_EQ(Flatten(board), (std::array<int, 16>{8, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}));
}

TEST_CASE(Move_Left_Merges_2022_To_4200) {
    auto board = MakeBoard({{{2, 0, 2, 2}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}});
    const auto result = board.ApplyMove(Direction::Left);

    EXPECT_TRUE(result.changed);
    EXPECT_EQ(result.scoreDelta, 4U);
    EXPECT_EQ(Flatten(board), (std::array<int, 16>{4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}));
}

TEST_CASE(Move_Invalid_Does_Not_Change_Board) {
    auto board = MakeBoard({{{2, 4, 8, 16}, {32, 64, 128, 256}, {512, 1024, 2, 4}, {8, 16, 32, 64}}});
    const auto before = Flatten(board);
    const auto result = board.ApplyMove(Direction::Left);

    EXPECT_FALSE(result.changed);
    EXPECT_EQ(result.scoreDelta, 0U);
    EXPECT_EQ(Flatten(board), before);
}

TEST_CASE(Move_Up_Compresses_And_Merges) {
    auto board = MakeBoard({{{2, 0, 0, 0}, {2, 0, 0, 0}, {4, 0, 0, 0}, {4, 0, 0, 0}}});
    const auto result = board.ApplyMove(Direction::Up);

    EXPECT_TRUE(result.changed);
    EXPECT_EQ(result.scoreDelta, 12U);
    EXPECT_EQ(Flatten(board), (std::array<int, 16>{4, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}));
}

TEST_CASE(Game_Reset_Spawns_Two_Tiles) {
    game2048::Game game(123);

    int nonZero = 0;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            if (game.GetBoard().At(row, col) != 0) {
                ++nonZero;
            }
        }
    }

    EXPECT_EQ(nonZero, 2);
    EXPECT_EQ(game.Score(), 0U);
}
