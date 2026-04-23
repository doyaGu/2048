#include <array>
#include <filesystem>
#include <fstream>
#include <string>

#include "../src/app/game_controller.h"
#include "../src/core/game.h"
#include "test_framework.h"

namespace {

using game2048::Direction;
using game2048::Game;
using game2048::GameController;

struct ScoringOpening {
    std::uint64_t seed = 0;
    Direction direction = Direction::Left;
};

struct ScopedFileCleanup {
    std::filesystem::path path;

    ~ScopedFileCleanup() {
        std::error_code error;
        std::filesystem::remove(path, error);
    }
};

std::filesystem::path TestFilePath(const char* name) {
    return std::filesystem::temp_directory_path() / name;
}

std::uint32_t ReadPersistedScore(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::uint32_t value = 0;
    input >> value;
    return value;
}

ScoringOpening FindScoringOpening() {
    constexpr std::array<Direction, 4> kDirections {
        Direction::Up,
        Direction::Left,
        Direction::Right,
        Direction::Down,
    };

    for (std::uint64_t seed = 1; seed < 10000; ++seed) {
        for (Direction direction : kDirections) {
            Game game(seed);
            const auto turn = game.ApplyMove(direction);
            if (turn.moved && turn.scoreDelta > 0) {
                return {seed, direction};
            }
        }
    }

    ::game2048::test::Fail("failed to find scoring opening for GameController test");
}

}  // namespace

TEST_CASE(GameController_LoadsInjectedBestScore) {
    const auto path = TestFilePath("game2048_test_best_score_load.txt");
    ScopedFileCleanup cleanup {path};

    {
        std::ofstream output(path, std::ios::trunc);
        output << 512;
    }

    const GameController controller(123, path);

    EXPECT_EQ(controller.BestScore(), 512U);
}

TEST_CASE(GameController_PersistsNewBestScoreToInjectedPath) {
    const auto path = TestFilePath("game2048_test_best_score_save.txt");
    ScopedFileCleanup cleanup {path};

    {
        std::ofstream output(path, std::ios::trunc);
        output << 0;
    }

    const auto opening = FindScoringOpening();
    GameController controller(opening.seed, path);
    const auto execution = controller.ExecuteMove(opening.direction);

    EXPECT_TRUE(execution.moved);
    EXPECT_TRUE(controller.Score() > 0U);
    EXPECT_EQ(controller.BestScore(), controller.Score());
    EXPECT_EQ(ReadPersistedScore(path), controller.Score());
}