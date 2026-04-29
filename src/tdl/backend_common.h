#pragma once

#include <array>
#include <cstddef>
#include <limits>

#include "core/board_fast.h"
#include "value/ntuple.h"

namespace game2048::ai::tdl_backend_detail {

inline constexpr double kInvalidMoveValue = -std::numeric_limits<double>::infinity();

inline bool IsValidMoveValue(double value) {
    return value != kInvalidMoveValue;
}

inline std::size_t SingleStageUpdateCount(const NtupleNetwork&) {
    return 1;
}

template <typename BestMove, typename Evaluator, typename AssignBest>
BestMove ChooseBestByTdlOrder(const FastBoard& board, Evaluator&& evaluate, AssignBest&& assignBest) {
    BestMove best;
    double bestValue = kInvalidMoveValue;
    bool found = false;
    std::array<FastMoveResult, 4> moves {};
    board.TdlOrderMoves(moves);
    for (std::size_t index = 0; index < moves.size(); ++index) {
        const FastMoveResult& move = moves[index];
        if (!move.changed) {
            continue;
        }
        const double value = static_cast<double>(move.scoreDelta) + evaluate(move);
        if (!found || value > bestValue) {
            found = true;
            bestValue = value;
            assignBest(best, index, move, value);
        }
    }
    return best;
}

}  // namespace game2048::ai::tdl_backend_detail
