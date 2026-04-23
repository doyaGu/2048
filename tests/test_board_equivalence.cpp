#include <array>

#include "../src/core/board.h"
#include "../src/core/board_fast.h"
#include "../src/core/rng.h"
#include "test_framework.h"

namespace {

using game2048::Board;
using game2048::Direction;
using game2048::FastBoard;
using game2048::Random;

Board RandomBoard(Random& rng) {
    std::array<int, 16> cells {};
    for (auto& cell : cells) {
        const std::uint32_t bucket = rng.NextU32() % 8U;
        if (bucket == 0U) {
            cell = 0;
        } else {
            cell = 1 << bucket;
        }
    }

    Board board;
    std::size_t index = 0;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            board.Set(row, col, cells[index]);
            ++index;
        }
    }
    return board;
}

game2048::FastMoveResult MoveFast(const FastBoard& board, Direction direction) {
    switch (direction) {
        case Direction::Left:
            return board.MoveLeft();
        case Direction::Right:
            return board.MoveRight();
        case Direction::Up:
            return board.MoveUp();
        case Direction::Down:
            return board.MoveDown();
    }
    return {};
}

}  // namespace

TEST_CASE(FastBoard_Matches_ReferenceBoard_On_Random_States) {
    Random rng(424242);
    constexpr std::array<Direction, 4> kDirections {
        Direction::Left, Direction::Right, Direction::Up, Direction::Down
    };

    for (int iteration = 0; iteration < 256; ++iteration) {
        const Board original = RandomBoard(rng);
        const FastBoard fast = FastBoard::FromReference(original);

        EXPECT_EQ(fast.ToReference(), original);
        EXPECT_EQ(fast.CanMove(), original.CanMove());

        for (const Direction direction : kDirections) {
            Board ref = original;
            const auto refMove = ref.ApplyMove(direction);
            const auto fastMove = MoveFast(fast, direction);

            EXPECT_EQ(fastMove.changed, refMove.changed);
            EXPECT_EQ(fastMove.scoreDelta, refMove.scoreDelta);
            EXPECT_EQ(FastBoard(fastMove.board).ToReference(), ref);
        }
    }
}
