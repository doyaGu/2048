#include "value/tdl_training_backend.h"

#include <array>
#include <limits>
#include <stdexcept>

#include "value/tdl_move_order.h"

namespace game2048::ai {

bool Fixed6TdBackend::Supports(NtupleMutableFixed6View view) {
    return view.valid && view.weights != nullptr && view.patternOffsets != nullptr &&
           view.shifts != nullptr && view.patternCount > 0 && view.patternCount <= kMaxPatterns;
}

Fixed6TdBackend::Fixed6TdBackend(NtupleNetwork& network)
    : view_(network.MutableFixed6SingleStageView(LearningMode::TD)) {
    if (!Supports(view_)) {
        throw std::invalid_argument("fixed-6 TD backend requires a single-stage fixed-6 TD view");
    }
}

double Fixed6TdBackend::Evaluate(std::uint64_t bits) const {
    return ntuple_kernel::EvaluateFixed6(bits, view_.weights, view_.patternOffsets,
                                         view_.shifts, view_.patternCount);
}

Fixed6TdMove Fixed6TdBackend::ChooseBest(const FastBoard& board) const {
    Fixed6TdMove best;
    std::array<FastMoveResult, 4> moves {};
    board.TdlOrderMoves(moves);
    for (const FastMoveResult& move : moves) {
        if (!move.changed) {
            continue;
        }
        const double value = static_cast<double>(move.scoreDelta) + Evaluate(move.board);
        if (value > best.value) {
            best.board = move.board;
            best.scoreDelta = move.scoreDelta;
            best.value = value;
        }
    }
    return best;
}

NtupleUpdateStats Fixed6TdBackend::Update(std::uint64_t bits, double target, double alpha) const {
    std::array<std::size_t, kMaxKeys> keys {};
    double before = 0.0;
    const std::size_t keyCount =
        ntuple_kernel::CollectFixed6KeysAndValue(bits, view_.weights, view_.patternOffsets,
                                                 view_.shifts, view_.patternCount,
                                                 keys.data(), before);
    const float error = static_cast<float>(target) - static_cast<float>(before);
    const float delta = static_cast<float>(alpha) * error / static_cast<float>(keyCount);
    for (std::size_t index = 0; index < keyCount; ++index) {
        const std::size_t key = keys[index];
        view_.weights[key] += delta;
    }
    return {before, 0.0, error};
}

NetworkTdlBackend::NetworkTdlBackend(NtupleNetwork& network, LearningMode mode)
    : network_(network), mode_(mode) {}

TdlCandidateMove NetworkTdlBackend::ChooseBest(const FastBoard& board) const {
    return tdl_backend_detail::ChooseBestWithNetworkEvaluator(board, network_);
}

NtupleUpdateStats NetworkTdlBackend::Update(std::uint64_t bits, double target, double alpha) const {
    return network_.UpdateTowardFast(FastBoard(bits), target, alpha, mode_, false);
}

TdlCandidateMove NetworkEvalBackend::ChooseBest(const FastBoard& board) const {
    return tdl_backend_detail::ChooseBestWithNetworkEvaluator(board, network_);
}

namespace tdl_backend_detail {

TdlCandidateMove ChooseBestWithNetworkEvaluator(const FastBoard& board, const NtupleNetwork& network) {
    TdlCandidateMove best;
    best.value = -std::numeric_limits<double>::infinity();
    std::array<FastMoveResult, 4> moves {};
    board.TdlOrderMoves(moves);
    for (std::size_t index = 0; index < moves.size(); ++index) {
        const FastMoveResult& move = moves[index];
        if (!move.changed) {
            continue;
        }
        const FastBoard afterstate(move.board);
        const double value = static_cast<double>(move.scoreDelta) + network.Evaluate(afterstate);
        if (!best.valid || value > best.value) {
            best.valid = true;
            best.direction = kTdlMoveDirections[index];
            best.move = move;
            best.value = value;
        }
    }
    return best;
}

void AccumulateUpdateStats(NtupleTrainingStats& stats, const FastBoard& board,
                           const NtupleUpdateStats& update, const NtupleNetwork& network) {
    if (stats.stageUpdates.size() == 1U) {
        ++stats.stageUpdates[0];
    } else {
        const std::size_t stage = network.StageFor(board);
        if (stats.stageUpdates.size() <= stage) {
            stats.stageUpdates.resize(stage + 1U);
        }
        ++stats.stageUpdates[stage];
    }
    ++stats.updates;
    const double absError = std::abs(update.error);
    stats.meanAbsTdError += absError;
    stats.rmsTdError += update.error * update.error;
    stats.maxAbsTdError = std::max(stats.maxAbsTdError, absError);
}

void FinalizeUpdateStats(NtupleTrainingStats& stats, const NtupleNetwork& network) {
    if (stats.updates > 0) {
        stats.meanAbsTdError /= static_cast<double>(stats.updates);
        stats.rmsTdError = std::sqrt(stats.rmsTdError / static_cast<double>(stats.updates));
    }
    stats.tcTouchedEntries = network.TouchedTcEntries();
}

}  // namespace tdl_backend_detail

}  // namespace game2048::ai
