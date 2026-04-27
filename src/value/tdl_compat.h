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
};

TdlCandidateMove ChooseTdlBestMove(const FastBoard& board, const NtupleNetwork& network);
NtupleTrainingStats TrainTdlForward(NtupleNetwork& network, TdlRandom& rng,
                                    const TdlForwardTrainingOptions& options);
NtupleTrainingStats TrainTdlForward(NtupleNetwork& network, const TdlForwardTrainingOptions& options);
NtupleTrainingStats EvaluateTdlBest(const NtupleNetwork& network, TdlRandom& rng, std::size_t games,
                                    std::size_t maxMovesPerGame = 0);
NtupleTrainingStats EvaluateTdlBest(const NtupleNetwork& network, std::uint32_t seed, std::size_t games,
                                    std::size_t maxMovesPerGame = 0);

}  // namespace game2048::ai
