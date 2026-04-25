#include "shared/stats.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace game2048 {

namespace {

double Percentile(const std::vector<std::uint32_t>& sortedScores, double p) {
    if (sortedScores.empty()) {
        return 0.0;
    }
    const double index = p * static_cast<double>(sortedScores.size() - 1);
    const auto lower = static_cast<std::size_t>(index);
    const auto upper = std::min(lower + 1, sortedScores.size() - 1);
    const double fraction = index - static_cast<double>(lower);
    return static_cast<double>(sortedScores[lower]) * (1.0 - fraction) +
           static_cast<double>(sortedScores[upper]) * fraction;
}

}  // namespace

BenchmarkSummary SummarizeBenchmark(const std::vector<BenchmarkGameStats>& games, double totalElapsedMs) {
    BenchmarkSummary summary;
    summary.games = games.size();
    summary.totalElapsedMs = totalElapsedMs;
    if (games.empty()) {
        return summary;
    }

    std::vector<std::uint32_t> scores;
    scores.reserve(games.size());

    std::uint64_t totalScore = 0;
    long double totalScoreSquared = 0.0L;
    std::uint64_t totalMaxTile = 0;
    std::uint64_t totalMoves = 0;
    double totalThinkTime = 0.0;
    std::uint64_t totalNodes = 0;
    std::uint64_t totalLeafEvals = 0;
    std::uint64_t totalCacheHits = 0;

    summary.bestScore = games.front().score;
    summary.worstScore = games.front().score;

    for (const auto& game : games) {
        scores.push_back(game.score);
        totalScore += game.score;
        totalScoreSquared += static_cast<long double>(game.score) * static_cast<long double>(game.score);
        totalMaxTile += static_cast<std::uint64_t>(game.maxTile);
        totalMoves += game.moves;
        totalThinkTime += game.thinkTimeMs;
        totalNodes += game.nodes;
        totalLeafEvals += game.leafEvals;
        totalCacheHits += game.cacheHits;
        summary.bestScore = std::max(summary.bestScore, game.score);
        summary.worstScore = std::min(summary.worstScore, game.score);
        summary.maxTileDistribution[game.maxTile] += 1;
    }

    std::sort(scores.begin(), scores.end());
    summary.averageScore = static_cast<double>(totalScore) / static_cast<double>(games.size());
    const long double mean = summary.averageScore;
    const long double variance = std::max<long double>(
        0.0L, totalScoreSquared / static_cast<long double>(games.size()) - mean * mean);
    summary.scoreStddev = static_cast<double>(std::sqrt(variance));
    summary.medianScore = Percentile(scores, 0.50);
    summary.p90Score = Percentile(scores, 0.90);
    summary.p99Score = Percentile(scores, 0.99);
    summary.averageMaxTile = static_cast<double>(totalMaxTile) / static_cast<double>(games.size());
    summary.averageMoves = static_cast<double>(totalMoves) / static_cast<double>(games.size());
    summary.averageThinkTimeMs = totalMoves == 0 ? 0.0 : totalThinkTime / static_cast<double>(totalMoves);
    summary.averageNodesPerMove = totalMoves == 0 ? 0.0 : static_cast<double>(totalNodes) / static_cast<double>(totalMoves);
    summary.averageLeafEvalsPerMove = totalMoves == 0 ? 0.0 : static_cast<double>(totalLeafEvals) / static_cast<double>(totalMoves);
    summary.transpositionHitRate = totalNodes == 0 ? 0.0 : static_cast<double>(totalCacheHits) / static_cast<double>(totalNodes);
    for (int target : {1024, 2048, 4096, 8192, 16384, 32768}) {
        std::size_t reached = 0;
        for (const auto& game : games) {
            if (game.maxTile >= target) {
                ++reached;
            }
        }
        summary.achievementRates[target] = static_cast<double>(reached) / static_cast<double>(games.size());
    }
    return summary;
}

std::string FormatBenchmarkSummary(const BenchmarkSummary& summary) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "games: " << summary.games << '\n';
    oss << "average score: " << summary.averageScore << '\n';
    oss << "score stddev: " << summary.scoreStddev << '\n';
    oss << "median score: " << summary.medianScore << '\n';
    oss << "p90 score: " << summary.p90Score << '\n';
    oss << "p99 score: " << summary.p99Score << '\n';
    oss << "best score: " << summary.bestScore << '\n';
    oss << "worst score: " << summary.worstScore << '\n';
    oss << "average max tile: " << summary.averageMaxTile << '\n';
    oss << "average moves: " << summary.averageMoves << '\n';
    oss << "average nodes per move: " << summary.averageNodesPerMove << '\n';
    oss << "average leaf evals per move: " << summary.averageLeafEvalsPerMove << '\n';
    oss << "transposition hit rate: " << (summary.transpositionHitRate * 100.0) << "%\n";
    oss << "average think time per move (ms): " << summary.averageThinkTimeMs << '\n';
    oss << "total elapsed (ms): " << summary.totalElapsedMs << '\n';
    oss << "max tile distribution:";
    for (const auto& [tile, count] : summary.maxTileDistribution) {
        oss << ' ' << tile << '=' << count;
    }
    oss << '\n' << "achievement rates:";
    for (const auto& [tile, rate] : summary.achievementRates) {
        oss << ' ' << tile << '=' << (rate * 100.0) << '%';
    }
    return oss.str();
}

}  // namespace game2048
