#include "value/tdl_compat.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace game2048::ai {

namespace {

constexpr std::array<Direction, 4> kTdlDirections {
    Direction::Up,
    Direction::Right,
    Direction::Down,
    Direction::Left,
};

FastMoveResult ApplyTdlDirection(const FastBoard& board, Direction direction) {
    switch (direction) {
        case Direction::Up:
            return board.MoveUp();
        case Direction::Right:
            return board.MoveRight();
        case Direction::Down:
            return board.MoveDown();
        case Direction::Left:
            return board.MoveLeft();
    }
    return {};
}

void AccumulateUpdateStats(NtupleTrainingStats& stats, const FastBoard& board,
                           const NtupleUpdateStats& update, const NtupleNetwork& network) {
    const std::size_t stage = network.StageFor(board);
    if (stats.stageUpdates.size() <= stage) {
        stats.stageUpdates.resize(stage + 1U);
    }
    ++stats.stageUpdates[stage];
    ++stats.updates;
    const double absError = std::abs(update.error);
    stats.meanAbsTdError += absError;
    stats.rmsTdError += update.error * update.error;
    stats.maxAbsTdError = std::max(stats.maxAbsTdError, absError);
    stats.maxTile = std::max(stats.maxTile, board.MaxTile());
}

void FinalizeUpdateStats(NtupleTrainingStats& stats, const NtupleNetwork& network) {
    if (stats.updates > 0) {
        stats.meanAbsTdError /= static_cast<double>(stats.updates);
        stats.rmsTdError = std::sqrt(stats.rmsTdError / static_cast<double>(stats.updates));
    }
    stats.tcTouchedEntries = network.TouchedTcEntries();
}

}  // namespace

TdlRandom::TdlRandom(std::uint32_t seed)
    : engine_(seed) {}

std::uint32_t TdlRandom::NextU32() {
    return static_cast<std::uint32_t>(engine_());
}

FastBoard TdlRandom::InitBoard() {
    const std::uint32_t u = NextU32();
    const std::uint32_t k = u & 0xffffU;
    const std::uint32_t i = k % 16U;
    const std::uint32_t j = (i + 1U + ((k >> 4U) % 15U)) % 16U;
    const std::uint32_t r = (u >> 16U) % 100U;
    std::uint64_t raw = ((r >= 1U ? 1ULL : 2ULL) << (i << 2U)) |
                        ((r >= 19U ? 1ULL : 2ULL) << (j << 2U));
    return FastBoard(raw);
}

bool TdlRandom::SpawnNext(FastBoard& board) {
    std::array<int, kCellCount> empties {};
    const std::size_t emptyCount = board.CollectEmptyIndices(empties);
    if (emptyCount == 0) {
        return false;
    }

    const std::uint32_t u = NextU32();
    const std::size_t selected = static_cast<std::size_t>((u >> 16U) % emptyCount);
    const int rank = (u % 10U) != 0U ? 1 : 2;
    board.SetRank(empties[selected], rank);
    return true;
}

TdlCandidateMove ChooseTdlBestMove(const FastBoard& board, const NtupleNetwork& network) {
    TdlCandidateMove best;
    best.value = -std::numeric_limits<double>::infinity();
    for (Direction direction : kTdlDirections) {
        const FastMoveResult move = ApplyTdlDirection(board, direction);
        if (!move.changed) {
            continue;
        }
        const FastBoard afterstate(move.board);
        const double value = static_cast<double>(move.scoreDelta) + network.Evaluate(afterstate);
        if (!best.valid || value > best.value) {
            best.valid = true;
            best.direction = direction;
            best.move = move;
            best.value = value;
        }
    }
    return best;
}

NtupleTrainingStats TrainTdlForward(NtupleNetwork& network, TdlRandom& rng,
                                    const TdlForwardTrainingOptions& options) {
    NtupleTrainingStats stats;
    stats.games = options.games;
    stats.stageUpdates.assign(std::max<std::size_t>(1, network.StageCount()), 0);

    for (std::size_t gameIndex = 0; gameIndex < options.games; ++gameIndex) {
        FastBoard board = rng.InitBoard();
        TdlCandidateMove current = ChooseTdlBestMove(board, network);
        if (!current.valid) {
            stats.maxTile = std::max(stats.maxTile, board.MaxTile());
            continue;
        }

        FastBoard previousAfterstate(current.move.board);
        stats.totalScore += current.move.scoreDelta;
        ++stats.moves;
        board = previousAfterstate;
        rng.SpawnNext(board);

        std::size_t movesThisGame = 1;
        while (options.maxMovesPerGame == 0 || movesThisGame < options.maxMovesPerGame) {
            TdlCandidateMove next = ChooseTdlBestMove(board, network);
            if (!next.valid) {
                const NtupleUpdateStats update =
                    network.UpdateTowardFast(previousAfterstate, 0.0, options.alpha, options.learningMode);
                AccumulateUpdateStats(stats, previousAfterstate, update, network);
                break;
            }

            const FastBoard nextAfterstate(next.move.board);
            const double target = next.value;
            const NtupleUpdateStats update =
                network.UpdateTowardFast(previousAfterstate, target, options.alpha, options.learningMode);
            AccumulateUpdateStats(stats, previousAfterstate, update, network);

            previousAfterstate = nextAfterstate;
            stats.totalScore += next.move.scoreDelta;
            ++stats.moves;
            ++movesThisGame;
            board = previousAfterstate;
            rng.SpawnNext(board);
        }

        stats.maxTile = std::max(stats.maxTile, board.MaxTile());
    }

    FinalizeUpdateStats(stats, network);
    return stats;
}

NtupleTrainingStats TrainTdlForward(NtupleNetwork& network, const TdlForwardTrainingOptions& options) {
    TdlRandom rng(options.seed);
    return TrainTdlForward(network, rng, options);
}

NtupleTrainingStats EvaluateTdlBest(const NtupleNetwork& network, TdlRandom& rng, std::size_t games,
                                    std::size_t maxMovesPerGame) {
    NtupleTrainingStats stats;
    stats.games = games;
    for (std::size_t gameIndex = 0; gameIndex < games; ++gameIndex) {
        FastBoard board = rng.InitBoard();
        std::size_t movesThisGame = 0;
        while (maxMovesPerGame == 0 || movesThisGame < maxMovesPerGame) {
            const TdlCandidateMove best = ChooseTdlBestMove(board, network);
            if (!best.valid) {
                break;
            }
            stats.totalScore += best.move.scoreDelta;
            ++stats.moves;
            ++movesThisGame;
            board = FastBoard(best.move.board);
            rng.SpawnNext(board);
        }
        stats.maxTile = std::max(stats.maxTile, board.MaxTile());
    }
    return stats;
}

NtupleTrainingStats EvaluateTdlBest(const NtupleNetwork& network, std::uint32_t seed, std::size_t games,
                                    std::size_t maxMovesPerGame) {
    TdlRandom rng(seed);
    return EvaluateTdlBest(network, rng, games, maxMovesPerGame);
}

}  // namespace game2048::ai
