#include "../src/experiment/profile.h"
#include "../src/experiment/runner.h"
#include "../src/training/selection.h"
#include "test_framework.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

TEST_CASE(TomlProfile_ParsesTrainingProfile) {
    const std::string text = R"(
[run]
name = "smoke"
seed = 700000

[selection]
metric = "tile-rate"
target_tile = 8192
confidence_z = 1.25
confirm_games = 7

[search]
agent = "expectimax"
depth = 3
time_budget_ms = 50
eval_games = 2
eval_interval = 1
final_games = 3
eval_threads = 4
eval_depth = 2
eval_time_budget_ms = 10

[search.downgrade]
enabled = true
mode = "root"
steps = 2
floor_rank = 13

[value]
preset = "compact-d4"
optimistic_init = 200000.0
multistage = true
stage_boundaries = [11, 12, 13]

[value.storage]
mode = "dense-stage"

[artifacts]
dir = "artifacts/profile-smoke"

[matrix]
seeds = [700000, 700010]
priors = [0.0, 0.02]

[[phases]]
name = "bootstrap"
games = 2
learning_mode = "optimistic-td"
alpha = 0.002
final_alpha = 0.0005

[[phases]]
name = "fine"
games = 1
learning_mode = "optimistic-tc"
alpha = 0.0005
final_alpha = 0.0001
prior_weight = 0.02
)";

    const auto profile = game2048::experiment::ParseExperimentProfileText(text);

    EXPECT_EQ(profile.run.name, std::string("smoke"));
    EXPECT_EQ(profile.run.seed, 700000ULL);
    EXPECT_EQ(profile.selection.metric, game2048::training::SelectionMetric::TileRate);
    EXPECT_EQ(profile.selection.targetTile, 8192);
    EXPECT_NEAR(profile.selection.confidenceZ, 1.25, 1e-9);
    EXPECT_EQ(profile.selection.confirmGames, std::size_t {7});
    EXPECT_EQ(profile.search.depth, 3);
    EXPECT_EQ(profile.search.timeBudgetMs, 50);
    EXPECT_EQ(profile.search.evalThreads, std::size_t {4});
    EXPECT_EQ(profile.search.evalDepth, 2);
    EXPECT_EQ(profile.search.evalTimeBudgetMs, 10);
    EXPECT_TRUE(profile.search.downgrade.enabled);
    EXPECT_EQ(profile.search.downgrade.mode, game2048::experiment::SearchProfileConfig::DowngradeMode::Root);
    EXPECT_EQ(profile.search.downgrade.steps, 2);
    EXPECT_EQ(profile.search.downgrade.floorRank, 13);
    EXPECT_EQ(profile.value.stageBoundaries.size(), std::size_t {3});
    EXPECT_EQ(profile.value.stageBoundaries[0], 11);
    EXPECT_EQ(profile.value.tuplePreset, game2048::ai::NtuplePreset::CompactD4);
    EXPECT_EQ(profile.value.storageMode, std::string("dense-stage"));
    EXPECT_EQ(profile.matrix.seeds.size(), std::size_t {2});
    EXPECT_EQ(profile.matrix.priors.size(), std::size_t {2});
    EXPECT_EQ(profile.phases.size(), std::size_t {2});
    EXPECT_EQ(profile.phases[1].name, std::string("fine"));
}

TEST_CASE(TomlProfile_Rejects_LegacyTdl6Preset) {
    bool threw = false;
    try {
        static_cast<void>(game2048::experiment::ParseExperimentProfileText(R"(
[run]
name = "legacy"

[value]
preset = "tdl6"

[[phases]]
name = "one"
games = 1
)"));
    } catch (const std::runtime_error&) {
        threw = true;
    }

    EXPECT_TRUE(threw);
}

TEST_CASE(TomlProfile_RejectsMissingPhase) {
    bool threw = false;
    try {
        static_cast<void>(game2048::experiment::ParseExperimentProfileText("[run]\nname = \"bad\"\n"));
    } catch (const std::runtime_error&) {
        threw = true;
    }

    EXPECT_TRUE(threw);
}

TEST_CASE(MatrixExpansion_UsesConfiguredSeedsAndPriors) {
    auto profile = game2048::experiment::ParseExperimentProfileText(R"(
[run]
name = "matrix"
seed = 1

[artifacts]
dir = "artifacts/matrix"

[matrix]
seeds = [11, 22]
priors = [0.0, 0.05]

[[phases]]
name = "one"
games = 1
)");

    const auto jobs = game2048::experiment::ExpandMatrixJobs(profile);

    EXPECT_EQ(jobs.size(), std::size_t {4});
    EXPECT_EQ(jobs[0].run.seed, 11ULL);
    EXPECT_EQ(jobs[0].run.name, std::string("matrix-seed11-prior0"));
    EXPECT_NEAR(jobs[3].phases[0].priorWeight, 0.05, 1e-9);
}

TEST_CASE(SelectionPolicy_SelectsTileRateThenAverageScore) {
    game2048::BenchmarkSummary first;
    first.averageScore = 1000.0;
    first.achievementRates[8192] = 0.25;
    game2048::BenchmarkSummary second;
    second.averageScore = 900.0;
    second.achievementRates[8192] = 0.50;

    const game2048::training::SelectionPolicy policy {
        game2048::training::SelectionMetric::TileRate,
        8192
    };

    EXPECT_TRUE(game2048::training::IsBetterSelection(second, first, policy));
    EXPECT_FALSE(game2048::training::IsBetterSelection(first, second, policy));
}

TEST_CASE(SelectionPolicy_ConfidenceBoundPenalizesNoisyAverageScore) {
    game2048::BenchmarkSummary steady;
    steady.games = 100;
    steady.averageScore = 900.0;
    steady.scoreStddev = 100.0;
    game2048::BenchmarkSummary noisy;
    noisy.games = 100;
    noisy.averageScore = 910.0;
    noisy.scoreStddev = 300.0;

    const game2048::training::SelectionPolicy policy {
        game2048::training::SelectionMetric::AverageScore,
        0,
        1.0
    };

    EXPECT_TRUE(game2048::training::IsBetterSelection(steady, noisy, policy));
    EXPECT_NEAR(game2048::training::SelectionValue(steady, policy), 890.0, 1e-9);
}

TEST_CASE(TrainingManifest_Reports_When_TC_State_Is_Not_Persisted) {
    const std::filesystem::path dir = "/tmp/game2048_manifest_td_only";
    std::filesystem::remove_all(dir);
    auto profile = game2048::experiment::ParseExperimentProfileText(R"(
[run]
name = "manifest-td"
seed = 42

[search]
agent = "ntuple"
eval_agent = "ntuple"
final_games = 1

[value]
preset = "compact-d4"

[artifacts]
dir = "/tmp/game2048_manifest_td_only"

[[phases]]
name = "td"
games = 1
learning_mode = "td"
alpha = 0.01
)");

    static_cast<void>(game2048::experiment::RunTrainingProfile(profile));
    std::ifstream metrics(dir / "metrics.csv");
    const std::string metricsText((std::istreambuf_iterator<char>(metrics)), std::istreambuf_iterator<char>());
    std::ifstream manifest(dir / "manifest.json");
    const std::string text((std::istreambuf_iterator<char>(manifest)), std::istreambuf_iterator<char>());
    std::filesystem::remove_all(dir);

    EXPECT_TRUE(text.find("\"tc_persisted\": false") != std::string::npos);
    EXPECT_TRUE(metricsText.find("raw_selection_value") != std::string::npos);
    EXPECT_TRUE(metricsText.find("mean_abs_td_error") != std::string::npos);
    EXPECT_TRUE(metricsText.find("stage0_updates") != std::string::npos);
}

}  // namespace
