#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../board_fast.h"
#include "../rng.h"
#include "../stats.h"
#include "ai_engine.h"

namespace game2048::ai {

struct BenchmarkOptions {
    std::size_t games = kDefaultBenchmarkGames;
    std::uint64_t seed = kDefaultSeed;
    bool useRandomSeedPerGame = false;
    AgentKind agent = AgentKind::Expectimax;
    SearchConfig search {};
    std::optional<std::string> csvPath;
};

class BenchmarkRunner {
public:
    explicit BenchmarkRunner(AIEngine engine = {});

    std::vector<BenchmarkGameStats> Run(const BenchmarkOptions& options);
    static void WriteCsv(const std::string& path, const std::vector<BenchmarkGameStats>& results);

private:
    static int SpawnRandomTile(FastBoard& board, Random& rng);

    AIEngine engine_;
};

}  // namespace game2048::ai
