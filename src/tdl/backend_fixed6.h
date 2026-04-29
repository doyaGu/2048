#pragma once

#include <cstddef>
#include <cstdint>

#include "core/board_fast.h"
#include "tdl/backend_common.h"
#include "value/ntuple.h"
#include "value/ntuple_kernel.h"

namespace game2048::ai {

struct Fixed6TdMove {
    std::uint64_t board = 0;
    double value = tdl_backend_detail::kInvalidMoveValue;
    std::uint32_t scoreDelta = 0;

    bool Valid() const { return tdl_backend_detail::IsValidMoveValue(value); }
};

class Fixed6TdBackend {
public:
    static constexpr std::size_t kMaxKeys = 128;
    static constexpr std::size_t kMaxPatterns = kMaxKeys / ntuple_kernel::kFixed6Transforms;

    static bool Supports(NtupleMutableFixed6View view);

    explicit Fixed6TdBackend(NtupleNetwork& network);

    double Evaluate(std::uint64_t bits) const;
    Fixed6TdMove ChooseBest(const FastBoard& board) const;
    NtupleUpdateStats Update(std::uint64_t bits, double target, double alpha) const;
    NtupleUpdateStats Update(const Fixed6TdMove& move, double target, double alpha) const {
        return Update(move.board, target, alpha);
    }

    static bool Valid(const Fixed6TdMove& move) { return move.Valid(); }
    static std::uint64_t AfterstateBits(const Fixed6TdMove& move) { return move.board; }
    static std::uint32_t ScoreDelta(const Fixed6TdMove& move) { return move.scoreDelta; }
    static double TargetValue(const Fixed6TdMove& move) { return move.value; }
    static std::size_t InitialStageUpdateCount(const NtupleNetwork& network) {
        return tdl_backend_detail::SingleStageUpdateCount(network);
    }

private:
    NtupleMutableFixed6View view_ {};
};

}  // namespace game2048::ai
