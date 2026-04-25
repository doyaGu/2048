#pragma once

#include <cstddef>

#include "shared/stats.h"

namespace game2048::training {

enum class SelectionMetric {
    AverageScore,
    TileRate,
};

struct SelectionPolicy {
    SelectionMetric metric = SelectionMetric::AverageScore;
    int targetTile = 0;
    double confidenceZ = 0.0;
    std::size_t confirmGames = 0;
};

double SelectionValue(const BenchmarkSummary& summary, const SelectionPolicy& policy);
bool IsBetterSelection(const BenchmarkSummary& candidate, const BenchmarkSummary& current,
                       const SelectionPolicy& policy);
const char* SelectionMetricName(SelectionMetric metric);

}  // namespace game2048::training
