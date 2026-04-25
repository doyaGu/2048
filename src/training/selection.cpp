#include "training/selection.h"

#include <algorithm>
#include <cmath>

namespace game2048::training {

double SelectionValue(const BenchmarkSummary& summary, const SelectionPolicy& policy) {
    const double games = static_cast<double>(summary.games);
    if (policy.metric == SelectionMetric::AverageScore) {
        if (policy.confidenceZ <= 0.0 || games <= 0.0) {
            return summary.averageScore;
        }
        return summary.averageScore - policy.confidenceZ * summary.scoreStddev / std::sqrt(games);
    }
    const auto it = summary.achievementRates.find(policy.targetTile);
    const double rate = it == summary.achievementRates.end() ? 0.0 : it->second;
    if (policy.confidenceZ <= 0.0 || games <= 0.0) {
        return rate;
    }
    return rate - policy.confidenceZ * std::sqrt(std::max(0.0, rate * (1.0 - rate)) / games);
}

bool IsBetterSelection(const BenchmarkSummary& candidate, const BenchmarkSummary& current,
                       const SelectionPolicy& policy) {
    const double candidateValue = SelectionValue(candidate, policy);
    const double currentValue = SelectionValue(current, policy);
    return candidateValue > currentValue ||
           (candidateValue == currentValue && candidate.averageScore > current.averageScore);
}

const char* SelectionMetricName(SelectionMetric metric) {
    switch (metric) {
        case SelectionMetric::AverageScore:
            return "average-score";
        case SelectionMetric::TileRate:
            return "tile-rate";
    }
    return "unknown";
}

}  // namespace game2048::training
