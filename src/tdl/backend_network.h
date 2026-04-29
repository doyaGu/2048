#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "core/board_fast.h"
#include "tdl/types.h"
#include "value/ntuple.h"

namespace game2048::ai {

class NetworkTdlBackend {
public:
    NetworkTdlBackend(NtupleNetwork& network, LearningMode mode);

    TdlCandidateMove ChooseBest(const FastBoard& board) const;
    NtupleUpdateStats Update(std::uint64_t bits, double target, double alpha) const;
    NtupleUpdateStats Update(const TdlCandidateMove& move, double target, double alpha) const {
        return Update(move.move.board, target, alpha);
    }

    static bool Valid(const TdlCandidateMove& move) { return move.valid; }
    static std::uint64_t AfterstateBits(const TdlCandidateMove& move) { return move.move.board; }
    static std::uint32_t ScoreDelta(const TdlCandidateMove& move) { return move.move.scoreDelta; }
    static double TargetValue(const TdlCandidateMove& move) { return move.value; }
    static std::size_t InitialStageUpdateCount(const NtupleNetwork& network) {
        return std::max<std::size_t>(1, network.StageCount());
    }

private:
    NtupleNetwork& network_;
    LearningMode mode_;
};

class NetworkEvalBackend {
public:
    explicit NetworkEvalBackend(const NtupleNetwork& network)
        : network_(network) {}

    TdlCandidateMove ChooseBest(const FastBoard& board) const;

    static bool Valid(const TdlCandidateMove& move) { return move.valid; }
    static std::uint64_t AfterstateBits(const TdlCandidateMove& move) { return move.move.board; }
    static std::uint32_t ScoreDelta(const TdlCandidateMove& move) { return move.move.scoreDelta; }

private:
    const NtupleNetwork& network_;
};

namespace tdl_backend_detail {

TdlCandidateMove ChooseBestWithNetworkEvaluator(const FastBoard& board, const NtupleNetwork& network);

}  // namespace tdl_backend_detail

}  // namespace game2048::ai
