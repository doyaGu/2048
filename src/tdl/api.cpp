#include "tdl/api.h"

#include "tdl/backend_fixed6.h"
#include "tdl/backend_network.h"
#include "tdl/backend_tdl8x6.h"
#include "tdl/move_order.h"
#include "tdl/training_loop.h"

#include <array>

namespace game2048::ai {

TdlCandidateMove ChooseTdlBestMove(const FastBoard& board, const NtupleNetwork& network) {
    const NtupleFixed6View view = network.Fixed6SingleStageView(LearningMode::TD);
    if (Tdl8x6Kernel::Supports(view)) {
        const Tdl8x6Kernel kernel(view);
        const Tdl8x6BestMove best = kernel.ChooseBest(board);
        if (!best.Valid()) {
            return {};
        }
        std::array<FastMoveResult, 4> moves {};
        board.TdlOrderMoves(moves);
        for (std::size_t index = 0; index < moves.size(); ++index) {
            if (moves[index].changed && moves[index].board == best.board &&
                moves[index].scoreDelta == best.scoreDelta) {
                return {moves[index], kTdlMoveDirections[index], best.value, true};
            }
        }
        return {FastMoveResult {best.board, best.scoreDelta, true}, Direction::Up, best.value, true};
    }

    return tdl_backend_detail::ChooseBestWithNetworkEvaluator(board, network);
}

NtupleTrainingStats TrainTdlForward(NtupleNetwork& network, TdlRandom& rng,
                                    const TdlForwardTrainingOptions& options) {
    if (options.fastPath) {
        Tdl8x6TdBackend backend(network);
        return TrainTdlForwardWithBackend(backend, network, rng, options);
    }

    if (options.learningMode == LearningMode::TD) {
        const NtupleMutableFixed6View view = network.MutableFixed6SingleStageView(LearningMode::TD);
        if (Tdl8x6Kernel::Supports(view)) {
            Tdl8x6TdBackend backend(network);
            return TrainTdlForwardWithBackend(backend, network, rng, options);
        }
        if (Fixed6TdBackend::Supports(view)) {
            Fixed6TdBackend backend(network);
            return TrainTdlForwardWithBackend(backend, network, rng, options);
        }
    }

    NetworkTdlBackend backend(network, options.learningMode);
    return TrainTdlForwardWithBackend(backend, network, rng, options);
}

NtupleTrainingStats TrainTdlForward(NtupleNetwork& network, const TdlForwardTrainingOptions& options) {
    TdlRandom rng(options.seed);
    return TrainTdlForward(network, rng, options);
}

NtupleTrainingStats EvaluateTdlBest(const NtupleNetwork& network, TdlRandom& rng, std::size_t games,
                                    std::size_t maxMovesPerGame) {
    const NtupleFixed6View view = network.Fixed6SingleStageView(LearningMode::TD);
    if (Tdl8x6Kernel::Supports(view)) {
        Tdl8x6EvalBackend backend(network);
        return EvaluateTdlBestWithBackend(backend, rng, games, maxMovesPerGame);
    }

    NetworkEvalBackend backend(network);
    return EvaluateTdlBestWithBackend(backend, rng, games, maxMovesPerGame);
}

NtupleTrainingStats EvaluateTdlBest(const NtupleNetwork& network, std::uint32_t seed, std::size_t games,
                                    std::size_t maxMovesPerGame) {
    TdlRandom rng(seed);
    return EvaluateTdlBest(network, rng, games, maxMovesPerGame);
}

}  // namespace game2048::ai
