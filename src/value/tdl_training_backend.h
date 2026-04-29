#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "core/board_fast.h"
#include "value/ntuple.h"
#include "value/ntuple_kernel.h"
#include "value/tdl8x6_kernel.h"
#include "value/tdl_types.h"

namespace game2048::ai {

struct Fixed6TdMove {
    static constexpr double kInvalidValue = -std::numeric_limits<double>::infinity();

    std::uint64_t board = 0;
    double value = kInvalidValue;
    std::uint32_t scoreDelta = 0;

    bool Valid() const { return value != kInvalidValue; }
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
    static std::size_t InitialStageUpdateCount(const NtupleNetwork&) { return 1; }

private:
    NtupleMutableFixed6View view_ {};
};

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
    static std::size_t InitialStageUpdateCount(const NtupleNetwork&) { return 1; }

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
