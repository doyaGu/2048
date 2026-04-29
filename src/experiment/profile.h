#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "search/ai_engine.h"
#include "training/selection.h"
#include "value/ntuple.h"

namespace game2048::experiment {

struct RunConfig {
    std::string name = "ntuple";
    std::uint64_t seed = kDefaultSeed;
};

struct SearchProfileConfig {
    enum class DowngradeMode {
        Leaf,
        Root,
    };

    struct DowngradeConfig {
        bool enabled = false;
        DowngradeMode mode = DowngradeMode::Leaf;
        int steps = 1;
        int floorRank = 13;
    };

    ai::AgentKind agent = ai::AgentKind::Expectimax;
    ai::AgentKind evalAgent = ai::AgentKind::Expectimax;
    int depth = kDefaultExpectimaxDepth;
    int timeBudgetMs = kDefaultExpectimaxTimeBudgetMs;
    int evalDepth = 0;
    int evalTimeBudgetMs = 0;
    bool fixedPly = false;
    bool approximateChanceNodes = ai::SearchConfig {}.approximateChanceNodes;
    int maxChanceBranchesPerValue = ai::SearchConfig {}.maxChanceBranchesPerValue;
    bool preserveChanceProbabilityMass = ai::SearchConfig {}.preserveChanceProbabilityMass;
    bool adaptiveEndgameSearch = ai::SearchConfig {}.adaptiveEndgameSearch;
    int endgameMinRank = ai::SearchConfig {}.endgameMinRank;
    int endgameDepthBonus = ai::SearchConfig {}.endgameDepthBonus;
    int endgameMaxChanceBranchesPerValue = ai::SearchConfig {}.endgameMaxChanceBranchesPerValue;
    double endgamePessimism = ai::SearchConfig {}.endgamePessimism;
    bool canonicalizeTranspositionKeys = ai::SearchConfig {}.canonicalizeTranspositionKeys;
    bool rootRollout = ai::SearchConfig {}.useRootRollout;
    int rootRolloutDepth = ai::SearchConfig {}.rootRolloutDepth;
    double rootRolloutWeight = ai::SearchConfig {}.rootRolloutWeight;
    DowngradeConfig downgrade {};
    std::size_t evalGames = 0;
    std::size_t evalInterval = 0;
    std::size_t progressIntervalGames = 0;
    std::size_t finalGames = kDefaultBenchmarkGames;
    std::size_t evalThreads = 1;
};

struct ValueProfileConfig {
    ai::NtuplePreset tuplePreset = ai::NtuplePreset::CompactD4;
    std::string storageMode = "dense-stage";
    double optimisticInit = 0.0;
    bool multistage = false;
    std::vector<int> stageBoundaries {};
};

struct TrainingPhaseConfig {
    std::string name = "phase";
    std::size_t games = 0;
    ai::LearningMode learningMode = ai::LearningMode::TD;
    double alpha = 0.01;
    double finalAlpha = 0.01;
    double epsilon = 0.0;
    double finalEpsilon = 0.0;
    double priorWeight = 0.0;
    int startRank = 0;
    int replayStartRank = 0;
    int replayCaptureRank = 0;
    ai::NtupleUpdateOrder updateOrder = ai::NtupleUpdateOrder::Online;
    bool enableMultistage = false;
};

struct MatrixConfig {
    std::vector<std::uint64_t> seeds {};
    std::vector<double> priors {};
};

struct ArtifactConfig {
    std::string dir = "artifacts/experiment";
};

struct TrainerProfileConfig {
    std::string mode {};
    std::size_t games = 0;
    std::size_t progressIntervalGames = 0;
    double alpha = 0.1;
    ai::LearningMode learningMode = ai::LearningMode::TD;
    std::vector<std::size_t> checkpoints {};
    bool fastPath = false;
};

struct EvalProfileConfig {
    std::string mode {};
    std::size_t games = 0;
    double parityTolerance = 0.15;
    std::string referenceCachePath {};
};

struct ExperimentProfile {
    RunConfig run {};
    training::SelectionPolicy selection {};
    SearchProfileConfig search {};
    ValueProfileConfig value {};
    TrainerProfileConfig trainer {};
    EvalProfileConfig eval {};
    MatrixConfig matrix {};
    ArtifactConfig artifacts {};
    std::vector<TrainingPhaseConfig> phases {};
};

ExperimentProfile ParseExperimentProfileText(const std::string& text);
ExperimentProfile LoadExperimentProfile(const std::string& path);
void SaveProfileCopy(const std::string& sourcePath, const std::string& destinationPath);
std::vector<ExperimentProfile> ExpandMatrixJobs(const ExperimentProfile& profile);

}  // namespace game2048::experiment
