#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace game2048 {

constexpr int kBoardSize = 4;
constexpr int kCellCount = kBoardSize * kBoardSize;
constexpr int kDefaultWindowWidth = 1280;
constexpr int kDefaultWindowHeight = 800;
constexpr int kTargetFps = 60;

constexpr double kSpawnProbability4 = 0.10;
constexpr std::uint64_t kDefaultSeed = 0x2048ULL;

constexpr int kUndoCapacity = 128;

constexpr int kDefaultBenchmarkGames = 100;
constexpr int kDefaultExpectimaxDepth = 4;
constexpr int kDefaultExpectimaxTimeBudgetMs = 12;
constexpr std::size_t kDefaultTranspositionTableEntries = 1u << 20;

struct EvaluatorWeights {
    double emptyTiles = 290.0;
    double monotonicity = 47.0;
    double smoothness = 14.0;
    double cornerMax = 620.0;
    double mergePotential = 96.0;
    double snakePattern = 1.15;
    double trapPenalty = 240.0;
};

inline constexpr EvaluatorWeights kDefaultEvaluatorWeights {};

inline constexpr std::array<std::array<double, kCellCount>, 4> kSnakeWeightTables {{
    {65536.0, 32768.0, 16384.0, 8192.0,
      512.0, 1024.0, 2048.0, 4096.0,
      256.0, 128.0, 64.0, 32.0,
        2.0,   4.0,  8.0, 16.0},
    {8192.0, 16384.0, 32768.0, 65536.0,
     4096.0,  2048.0,  1024.0,  512.0,
       32.0,    64.0,   128.0,  256.0,
       16.0,     8.0,     4.0,    2.0},
    {   2.0,   16.0,   256.0,  4096.0,
        4.0,  128.0,  2048.0, 32768.0,
        8.0,   64.0,  1024.0, 16384.0,
       16.0,   32.0,   512.0, 65536.0},
    {4096.0,  256.0,   16.0,    2.0,
     32768.0, 2048.0,  128.0,   4.0,
     16384.0, 1024.0,   64.0,   8.0,
     65536.0,  512.0,   32.0,  16.0}
}};

}  // namespace game2048
