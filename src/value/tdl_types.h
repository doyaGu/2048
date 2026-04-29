#pragma once

#include <cstddef>
#include <cstdint>
#include <random>

#include "core/board_fast.h"
#include "value/ntuple.h"

namespace game2048::ai {

class TdlRandom {
public:
    explicit TdlRandom(std::uint32_t seed);

    std::uint32_t NextU32();
    FastBoard InitBoard();
    bool SpawnNext(FastBoard& board);

private:
    std::mt19937 engine_;
};

struct TdlCandidateMove {
    FastMoveResult move {};
    Direction direction = Direction::Up;
    double value = 0.0;
    bool valid = false;
};

struct TdlForwardTrainingOptions {
    std::size_t games = 0;
    std::uint32_t seed = 0;
    double alpha = 0.1;
    LearningMode learningMode = LearningMode::TD;
    std::size_t maxMovesPerGame = 0;
    bool fastPath = false;
};

}  // namespace game2048::ai
