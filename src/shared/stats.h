#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace game2048 {

struct SearchStats {
    std::uint64_t nodes = 0;
    std::uint64_t chanceNodes = 0;
    std::uint64_t terminalNodes = 0;
    std::uint64_t cacheHits = 0;
    int maxDepthReached = 0;
    double elapsedMs = 0.0;
    double evaluation = 0.0;
};

struct BenchmarkGameStats {
    std::uint32_t score = 0;
    int maxTile = 0;
    std::uint32_t moves = 0;
    std::uint64_t nodes = 0;
    double thinkTimeMs = 0.0;
    std::uint64_t seed = 0;
};

struct BenchmarkSummary {
    std::size_t games = 0;
    double averageScore = 0.0;
    double medianScore = 0.0;
    double p90Score = 0.0;
    double p99Score = 0.0;
    std::uint32_t bestScore = 0;
    std::uint32_t worstScore = 0;
    double averageMaxTile = 0.0;
    double averageMoves = 0.0;
    double averageNodesPerMove = 0.0;
    double averageThinkTimeMs = 0.0;
    double totalElapsedMs = 0.0;
    std::map<int, std::size_t> maxTileDistribution;
    std::map<int, double> achievementRates;
};

BenchmarkSummary SummarizeBenchmark(const std::vector<BenchmarkGameStats>& games, double totalElapsedMs);
std::string FormatBenchmarkSummary(const BenchmarkSummary& summary);

}  // namespace game2048