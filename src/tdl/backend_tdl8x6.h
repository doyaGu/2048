#pragma once

#include <cstddef>
#include <cstdint>

#include "tdl/backend_common.h"
#include "tdl/tdl8x6_kernel.h"
#include "value/ntuple.h"

namespace game2048::ai {

class Tdl8x6TdBackend {
public:
    explicit Tdl8x6TdBackend(NtupleNetwork& network)
        : kernel_(network.MutableFixed6SingleStageView(LearningMode::TD)) {}

    Tdl8x6BestMove ChooseBest(const FastBoard& board) const {
        return kernel_.ChooseBest(board);
    }

    NtupleUpdateStats Update(const Tdl8x6BestMove& move, double target, double alpha) const {
        return kernel_.UpdateKnownValue(move.board, move.estimate, target, alpha);
    }

    static bool Valid(const Tdl8x6BestMove& move) { return move.Valid(); }
    static std::uint64_t AfterstateBits(const Tdl8x6BestMove& move) { return move.board; }
    static std::uint32_t ScoreDelta(const Tdl8x6BestMove& move) { return move.scoreDelta; }
    static double TargetValue(const Tdl8x6BestMove& move) { return move.value; }
    static std::size_t InitialStageUpdateCount(const NtupleNetwork& network) {
        return tdl_backend_detail::SingleStageUpdateCount(network);
    }

private:
    Tdl8x6Kernel kernel_;
};

class Tdl8x6EvalBackend {
public:
    explicit Tdl8x6EvalBackend(const NtupleNetwork& network)
        : kernel_(network.Fixed6SingleStageView(LearningMode::TD)) {}

    Tdl8x6BestMove ChooseBest(const FastBoard& board) const {
        return kernel_.ChooseBest(board);
    }

    static bool Valid(const Tdl8x6BestMove& move) { return move.Valid(); }
    static std::uint64_t AfterstateBits(const Tdl8x6BestMove& move) { return move.board; }
    static std::uint32_t ScoreDelta(const Tdl8x6BestMove& move) { return move.scoreDelta; }

private:
    Tdl8x6Kernel kernel_;
};

}  // namespace game2048::ai
