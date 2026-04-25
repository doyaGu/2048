#include "training/benchmark.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <future>
#include <memory>
#include <thread>
#include <utility>

namespace game2048::ai {

namespace {

void ConfigureBenchmarkEngine(AIEngine& engine, const BenchmarkOptions& options,
                              const std::shared_ptr<const NtupleNetwork>& network) {
    engine.SetAgent(options.agent);
    engine.Expectimax().SetConfig(options.search);
    if (network) {
        if (options.agent == AgentKind::Ntuple) {
            engine.Ntuple().SetNetworkShared(network);
        }
        if (options.agent == AgentKind::Expectimax) {
            engine.Expectimax().SetLeafNetworkShared(network);
            engine.Expectimax().SetLeafPriorWeight(options.ntuplePriorWeight);
        }
    }
    if (options.agent == AgentKind::Ntuple) {
        engine.Ntuple().SetPriorWeight(options.ntuplePriorWeight);
    }
}

BenchmarkGameStats RunBenchmarkGame(AIEngine& engine, const BenchmarkOptions& options, std::size_t gameIndex) {
    const std::uint64_t seed = options.useRandomSeedPerGame
        ? (options.seed + gameIndex * 9973ULL)
        : (options.seed + gameIndex);
    Random rng(seed);
    FastBoard board;
    rng.SpawnOnFastBoard(board);
    rng.SpawnOnFastBoard(board);

    BenchmarkGameStats stats;
    stats.seed = seed;

    while (board.CanMove()) {
        const auto decision = engine.Recommend(board);
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
        rng.SpawnOnFastBoard(board);
        ++stats.moves;
        stats.score += move.scoreDelta;
        stats.nodes += decision.stats.nodes;
        stats.cacheHits += decision.stats.cacheHits;
        stats.leafEvals += decision.stats.leafEvals;
        stats.thinkTimeMs += decision.stats.elapsedMs;
    }

    stats.maxTile = board.MaxTile();
    return stats;
}

}  // namespace

BenchmarkRunner::BenchmarkRunner(AIEngine engine)
    : engine_(std::move(engine)) {}

std::vector<BenchmarkGameStats> BenchmarkRunner::Run(const BenchmarkOptions& options) {
    std::shared_ptr<const NtupleNetwork> network = options.ntupleNetwork;
    if (options.ntupleWeightPath.has_value()) {
        network = std::make_shared<NtupleNetwork>(NtupleNetwork::Load(*options.ntupleWeightPath));
    }

    std::vector<BenchmarkGameStats> results(options.games);
    const std::size_t requestedThreads = options.evalThreads == 0 ? 1 : options.evalThreads;
    const std::size_t threadCount = std::max<std::size_t>(
        1, std::min({requestedThreads, options.games, static_cast<std::size_t>(std::thread::hardware_concurrency() == 0
            ? 1
            : std::thread::hardware_concurrency())}));

    auto runRange = [this, &options, network, &results](std::size_t begin, std::size_t end) {
        AIEngine engine = engine_;
        ConfigureBenchmarkEngine(engine, options, network);
        for (std::size_t gameIndex = begin; gameIndex < end; ++gameIndex) {
            results[gameIndex] = RunBenchmarkGame(engine, options, gameIndex);
        }
    };

    if (threadCount <= 1 || options.games <= 1) {
        runRange(0, options.games);
    } else {
        std::vector<std::future<void>> futures;
        futures.reserve(threadCount);
        const std::size_t chunk = (options.games + threadCount - 1) / threadCount;
        for (std::size_t thread = 0; thread < threadCount; ++thread) {
            const std::size_t begin = thread * chunk;
            const std::size_t end = std::min(options.games, begin + chunk);
            if (begin >= end) {
                continue;
            }
            futures.push_back(std::async(std::launch::async, runRange, begin, end));
        }
        for (auto& future : futures) {
            future.get();
        }
    }

    if (options.csvPath.has_value()) {
        WriteCsv(*options.csvPath, results);
    }
    return results;
}

void BenchmarkRunner::WriteCsv(const std::string& path, const std::vector<BenchmarkGameStats>& results) {
    std::ofstream out(path);
    out << "seed,score,max_tile,moves,nodes,cache_hits,leaf_evals,think_time_ms\n";
    for (const auto& result : results) {
        out << result.seed << ',' << result.score << ',' << result.maxTile << ','
            << result.moves << ',' << result.nodes << ',' << result.cacheHits << ','
            << result.leafEvals << ',' << result.thinkTimeMs << '\n';
    }
}

int BenchmarkRunner::SpawnRandomTile(FastBoard& board, Random& rng) {
    return rng.SpawnOnFastBoard(board);
}

}  // namespace game2048::ai
