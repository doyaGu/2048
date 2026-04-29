#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "core/board_fast.h"
#include "tdl/types.h"
#include "value/ntuple.h"

namespace game2048::ai {

namespace tdl_backend_detail {

void AccumulateUpdateStats(NtupleTrainingStats& stats, const FastBoard& board,
                           const NtupleUpdateStats& update, const NtupleNetwork& network);

void FinalizeUpdateStats(NtupleTrainingStats& stats, const NtupleNetwork& network);

}  // namespace tdl_backend_detail

template <typename Backend>
NtupleTrainingStats TrainTdlForwardWithBackend(Backend& backend, NtupleNetwork& network, TdlRandom& rng,
                                               const TdlForwardTrainingOptions& options) {
    NtupleTrainingStats stats;
    stats.games = options.games;
    stats.stageUpdates.assign(Backend::InitialStageUpdateCount(network), 0);

    for (std::size_t gameIndex = 0; gameIndex < options.games; ++gameIndex) {
        FastBoard board = rng.InitBoard();
        auto current = backend.ChooseBest(board);
        if (!Backend::Valid(current)) {
            stats.maxTile = std::max(stats.maxTile, board.MaxTile());
            continue;
        }

        FastBoard previousAfterstate(Backend::AfterstateBits(current));
        stats.totalScore += Backend::ScoreDelta(current);
        ++stats.moves;
        board = previousAfterstate;
        rng.SpawnNext(board);

        std::size_t movesThisGame = 1;
        while (options.maxMovesPerGame == 0 || movesThisGame < options.maxMovesPerGame) {
            auto next = backend.ChooseBest(board);
            if (!Backend::Valid(next)) {
                const NtupleUpdateStats update = backend.Update(current, 0.0, options.alpha);
                tdl_backend_detail::AccumulateUpdateStats(stats, previousAfterstate, update, network);
                break;
            }

            const FastBoard nextAfterstate(Backend::AfterstateBits(next));
            const NtupleUpdateStats update =
                backend.Update(current, Backend::TargetValue(next), options.alpha);
            tdl_backend_detail::AccumulateUpdateStats(stats, previousAfterstate, update, network);

            current = next;
            previousAfterstate = nextAfterstate;
            stats.totalScore += Backend::ScoreDelta(next);
            ++stats.moves;
            ++movesThisGame;
            board = previousAfterstate;
            rng.SpawnNext(board);
        }

        stats.maxTile = std::max(stats.maxTile, board.MaxTile());
    }

    tdl_backend_detail::FinalizeUpdateStats(stats, network);
    return stats;
}

template <typename Backend>
NtupleTrainingStats EvaluateTdlBestWithBackend(Backend& backend, TdlRandom& rng, std::size_t games,
                                               std::size_t maxMovesPerGame) {
    NtupleTrainingStats stats;
    stats.games = games;
    for (std::size_t gameIndex = 0; gameIndex < games; ++gameIndex) {
        FastBoard board = rng.InitBoard();
        std::size_t movesThisGame = 0;
        while (maxMovesPerGame == 0 || movesThisGame < maxMovesPerGame) {
            const auto best = backend.ChooseBest(board);
            if (!Backend::Valid(best)) {
                break;
            }
            stats.totalScore += Backend::ScoreDelta(best);
            ++stats.moves;
            ++movesThisGame;
            board = FastBoard(Backend::AfterstateBits(best));
            rng.SpawnNext(board);
        }
        stats.maxTile = std::max(stats.maxTile, board.MaxTile());
    }
    return stats;
}

}  // namespace game2048::ai
