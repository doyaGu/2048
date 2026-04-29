#include "tdl/backend_fixed6.h"

#include <stdexcept>

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
    return tdl_backend_detail::ChooseBestByTdlOrder<Fixed6TdMove>(
        board,
        [this](const FastMoveResult& move) {
            return Evaluate(move.board);
        },
        [](Fixed6TdMove& best, std::size_t, const FastMoveResult& move, double value) {
            best.board = move.board;
            best.scoreDelta = move.scoreDelta;
            best.value = value;
        });
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

}  // namespace game2048::ai
