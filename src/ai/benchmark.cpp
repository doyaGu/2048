#include "ai/benchmark.h"

#include <chrono>
#include <fstream>

namespace game2048::ai {

BenchmarkRunner::BenchmarkRunner(AIEngine engine)
    : engine_(std::move(engine)) {}

std::vector<BenchmarkGameStats> BenchmarkRunner::Run(const BenchmarkOptions& options) {
    std::vector<BenchmarkGameStats> results;
    results.reserve(options.games);

    engine_.SetAgent(options.agent);
    engine_.Expectimax().SetConfig(options.search);

    for (std::size_t gameIndex = 0; gameIndex < options.games; ++gameIndex) {
        const std::uint64_t seed = options.useRandomSeedPerGame ? (options.seed + gameIndex * 9973ULL) : (options.seed + gameIndex);
        Random rng(seed);
        FastBoard board;
        SpawnRandomTile(board, rng);
        SpawnRandomTile(board, rng);

        BenchmarkGameStats stats;
        stats.seed = seed;

        while (board.CanMove()) {
            const auto decision = engine_.Recommend(board);
            if (!decision.valid) {
                break;
            }

            FastMoveResult move;
            switch (decision.direction) {
                case Direction::Left:
                    move = board.MoveLeft();
                    break;
                case Direction::Right:
                    move = board.MoveRight();
                    break;
                case Direction::Up:
                    move = board.MoveUp();
                    break;
                case Direction::Down:
                    move = board.MoveDown();
                    break;
            }

            if (!move.changed) {
                break;
            }

            board = FastBoard(move.board);
            SpawnRandomTile(board, rng);
            ++stats.moves;
            stats.score += move.scoreDelta;
            stats.nodes += decision.stats.nodes;
            stats.thinkTimeMs += decision.stats.elapsedMs;
        }

        stats.maxTile = board.MaxTile();
        results.push_back(stats);
    }

    if (options.csvPath.has_value()) {
        WriteCsv(*options.csvPath, results);
    }
    return results;
}

void BenchmarkRunner::WriteCsv(const std::string& path, const std::vector<BenchmarkGameStats>& results) {
    std::ofstream out(path);
    out << "seed,score,max_tile,moves,nodes,think_time_ms\n";
    for (const auto& result : results) {
        out << result.seed << ',' << result.score << ',' << result.maxTile << ','
            << result.moves << ',' << result.nodes << ',' << result.thinkTimeMs << '\n';
    }
}

int BenchmarkRunner::SpawnRandomTile(FastBoard& board, Random& rng) {
    return rng.SpawnOnFastBoard(board);
}

}  // namespace game2048::ai
