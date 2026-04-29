#pragma once

#include <cstddef>
#include <cstdint>

#include "value/ntuple.h"
#include "tdl/types.h"

namespace game2048::ai {

TdlCandidateMove ChooseTdlBestMove(const FastBoard& board, const NtupleNetwork& network);
NtupleTrainingStats TrainTdlForward(NtupleNetwork& network, TdlRandom& rng,
                                    const TdlForwardTrainingOptions& options);
NtupleTrainingStats TrainTdlForward(NtupleNetwork& network, const TdlForwardTrainingOptions& options);
NtupleTrainingStats EvaluateTdlBest(const NtupleNetwork& network, TdlRandom& rng, std::size_t games,
                                    std::size_t maxMovesPerGame = 0);
NtupleTrainingStats EvaluateTdlBest(const NtupleNetwork& network, std::uint32_t seed, std::size_t games,
                                    std::size_t maxMovesPerGame = 0);

}  // namespace game2048::ai
