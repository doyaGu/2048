#include "tdl/training_loop.h"

namespace game2048::ai::tdl_backend_detail {

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

}  // namespace game2048::ai::tdl_backend_detail
