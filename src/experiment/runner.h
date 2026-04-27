#pragma once

#include <string>
#include <vector>

#include "experiment/profile.h"
#include "shared/stats.h"

namespace game2048::experiment {

struct TrainingRunResult {
    std::string artifactDir;
    std::string bestWeightsPath;
    BenchmarkSummary finalSummary {};
    bool tcPersisted = false;
};

struct MatrixRunResult {
    std::string summaryCsvPath;
    std::string bestWeightsPath;
    std::vector<TrainingRunResult> jobs;
};

struct ParityRunResult {
    std::string artifactDir;
    std::string parityCsvPath;
    bool passed = true;
};

TrainingRunResult RunTrainingProfile(const ExperimentProfile& profile, const std::string& sourceProfilePath = {});
BenchmarkSummary RunBenchmarkProfile(const ExperimentProfile& profile, const std::string& weightsPath);
MatrixRunResult RunMatrixProfile(const ExperimentProfile& profile, int maxJobs);
ParityRunResult RunParityProfile(const ExperimentProfile& profile, const std::string& tdlBinPath = {});

}  // namespace game2048::experiment
