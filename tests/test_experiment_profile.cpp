#include "../src/experiment/profile.h"
#include "../src/experiment/runner.h"
#include "../src/training/selection.h"
#include "../src/value/ntuple.h"
#include "../src/value/tdl_compat.h"
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
	progress_interval_games = 5
	final_games = 3
eval_threads = 4
eval_depth = 2
eval_time_budget_ms = 10
fixed_ply = true
approximate_chance_nodes = false
max_chance_branches_per_value = 9
preserve_chance_probability_mass = true
adaptive_endgame_search = true
endgame_min_rank = 13
endgame_depth_bonus = 2
endgame_max_chance_branches_per_value = 3
endgame_pessimism = 0.12
canonicalize_transposition_keys = true
root_rollout = true
root_rollout_depth = 8
root_rollout_weight = 0.05

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
update_order = "backward"
replay_start_rank = 14
replay_capture_rank = 14
enable_multistage = true
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
    EXPECT_EQ(profile.search.progressIntervalGames, std::size_t {5});
    EXPECT_EQ(profile.search.evalDepth, 2);
    EXPECT_EQ(profile.search.evalTimeBudgetMs, 10);
    EXPECT_TRUE(profile.search.fixedPly);
    EXPECT_FALSE(profile.search.approximateChanceNodes);
    EXPECT_EQ(profile.search.maxChanceBranchesPerValue, 9);
    EXPECT_TRUE(profile.search.preserveChanceProbabilityMass);
    EXPECT_TRUE(profile.search.adaptiveEndgameSearch);
    EXPECT_EQ(profile.search.endgameMinRank, 13);
    EXPECT_EQ(profile.search.endgameDepthBonus, 2);
    EXPECT_EQ(profile.search.endgameMaxChanceBranchesPerValue, 3);
    EXPECT_NEAR(profile.search.endgamePessimism, 0.12, 1e-9);
    EXPECT_TRUE(profile.search.canonicalizeTranspositionKeys);
    EXPECT_TRUE(profile.search.rootRollout);
    EXPECT_EQ(profile.search.rootRolloutDepth, 8);
    EXPECT_NEAR(profile.search.rootRolloutWeight, 0.05, 1e-9);
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
    EXPECT_EQ(profile.phases[1].updateOrder, game2048::ai::NtupleUpdateOrder::Backward);
    EXPECT_EQ(profile.phases[1].replayStartRank, 14);
    EXPECT_EQ(profile.phases[1].replayCaptureRank, 14);
    EXPECT_TRUE(profile.phases[1].enableMultistage);
}

TEST_CASE(SearchProfile_Builds_StrongSearchConfig) {
    game2048::experiment::SearchProfileConfig profile;
    profile.depth = 6;
    profile.timeBudgetMs = 250;
    profile.fixedPly = true;
    profile.approximateChanceNodes = false;
    profile.maxChanceBranchesPerValue = 9;
    profile.preserveChanceProbabilityMass = true;
    profile.adaptiveEndgameSearch = true;
    profile.endgameMinRank = 13;
    profile.endgameDepthBonus = 2;
    profile.endgameMaxChanceBranchesPerValue = 3;
    profile.endgamePessimism = 0.12;
    profile.canonicalizeTranspositionKeys = true;
    profile.rootRollout = true;
    profile.rootRolloutDepth = 8;
    profile.rootRolloutWeight = 0.05;
    profile.downgrade.enabled = true;
    profile.downgrade.mode = game2048::experiment::SearchProfileConfig::DowngradeMode::Root;
    profile.downgrade.steps = 1;
    profile.downgrade.floorRank = 13;

    const auto config = game2048::experiment::BuildSearchConfig(profile);

    EXPECT_EQ(config.maxDepth, 6);
    EXPECT_EQ(config.timeBudgetMs, 0);
    EXPECT_FALSE(config.iterativeDeepening);
    EXPECT_FALSE(config.approximateChanceNodes);
    EXPECT_EQ(config.maxChanceBranchesPerValue, 9);
    EXPECT_TRUE(config.preserveChanceProbabilityMass);
    EXPECT_TRUE(config.adaptiveEndgameSearch);
    EXPECT_EQ(config.endgameMinRank, 13);
    EXPECT_EQ(config.endgameDepthBonus, 2);
    EXPECT_EQ(config.endgameMaxChanceBranchesPerValue, 3);
    EXPECT_NEAR(config.endgamePessimism, 0.12, 1e-9);
    EXPECT_TRUE(config.canonicalizeTranspositionKeys);
    EXPECT_TRUE(config.useRootRollout);
    EXPECT_EQ(config.rootRolloutDepth, 8);
    EXPECT_NEAR(config.rootRolloutWeight, 0.05, 1e-9);
    EXPECT_FALSE(config.useTileDowngrading);
    EXPECT_TRUE(config.useRootTileDowngrading);
}

TEST_CASE(NtupleStrongProfile_Loads_StrongSearchSettings) {
    const auto profile = game2048::experiment::LoadExperimentProfile("profiles/ntuple_strong.toml");

    EXPECT_EQ(profile.value.tuplePreset, game2048::ai::NtuplePreset::Tdl8x6KMatsuzaki);
    EXPECT_TRUE(profile.value.multistage);
    EXPECT_EQ(profile.value.stageBoundaries.size(), std::size_t {1});
    EXPECT_EQ(profile.value.stageBoundaries[0], 14);
    EXPECT_TRUE(profile.search.fixedPly);
    EXPECT_TRUE(profile.search.downgrade.enabled);
    EXPECT_EQ(profile.search.downgrade.mode,
              game2048::experiment::SearchProfileConfig::DowngradeMode::Root);
    EXPECT_TRUE(profile.search.preserveChanceProbabilityMass);
    EXPECT_TRUE(profile.search.adaptiveEndgameSearch);
    EXPECT_TRUE(profile.search.canonicalizeTranspositionKeys);
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

TEST_CASE(TomlProfile_ParsesTdlForwardParityProfile) {
    const std::string text = R"(
[run]
name = "tdl-parity-smoke"
seed = 700000

[value]
preset = "tdl-8x6-kmatsuzaki"
optimistic_init = 320000.0

[trainer]
mode = "tdl-forward-td"
games = 20000
progress_interval_games = 5000
alpha = 0.1
checkpoints = [20000]

[eval]
mode = "tdl-best"
games = 1000
reference_cache = "artifacts/tdl-reference-cache.csv"

[artifacts]
dir = "artifacts/tdl-parity-smoke"
)";

    const auto profile = game2048::experiment::ParseExperimentProfileText(text);

    EXPECT_EQ(profile.run.name, std::string("tdl-parity-smoke"));
    EXPECT_EQ(profile.value.tuplePreset, game2048::ai::NtuplePreset::Tdl8x6KMatsuzaki);
    EXPECT_EQ(profile.trainer.mode, std::string("tdl-forward-td"));
    EXPECT_EQ(profile.trainer.games, std::size_t {20000});
    EXPECT_EQ(profile.trainer.progressIntervalGames, std::size_t {5000});
    EXPECT_NEAR(profile.trainer.alpha, 0.1, 1e-9);
    EXPECT_EQ(profile.trainer.checkpoints.size(), std::size_t {1});
    EXPECT_EQ(profile.trainer.checkpoints[0], std::size_t {20000});
    EXPECT_EQ(profile.eval.mode, std::string("tdl-best"));
    EXPECT_EQ(profile.eval.games, std::size_t {1000});
    EXPECT_EQ(profile.eval.referenceCachePath, std::string("artifacts/tdl-reference-cache.csv"));
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
approximate_chance_nodes = false
max_chance_branches_per_value = 9
preserve_chance_probability_mass = true
adaptive_endgame_search = true
root_rollout = true

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
    std::ifstream config(dir / "config.toml");
    const std::string configText((std::istreambuf_iterator<char>(config)), std::istreambuf_iterator<char>());
    std::filesystem::remove_all(dir);

    EXPECT_TRUE(text.find("\"tc_persisted\": false") != std::string::npos);
    EXPECT_TRUE(metricsText.find("raw_selection_value") != std::string::npos);
    EXPECT_TRUE(metricsText.find("mean_abs_td_error") != std::string::npos);
    EXPECT_TRUE(metricsText.find("stage0_updates") != std::string::npos);
    EXPECT_TRUE(configText.find("approximate_chance_nodes = false") != std::string::npos);
    EXPECT_TRUE(configText.find("max_chance_branches_per_value = 9") != std::string::npos);
    EXPECT_TRUE(configText.find("preserve_chance_probability_mass = true") != std::string::npos);
    EXPECT_TRUE(configText.find("adaptive_endgame_search = true") != std::string::npos);
    EXPECT_TRUE(configText.find("root_rollout = true") != std::string::npos);
}

TEST_CASE(TrainingProfile_Writes_Progress_Before_EvalInterval) {
    const std::filesystem::path dir = "/tmp/game2048_progress_interval";
    std::filesystem::remove_all(dir);
    auto profile = game2048::experiment::ParseExperimentProfileText(R"(
[run]
name = "progress"
seed = 42

[search]
agent = "ntuple"
eval_agent = "ntuple"
eval_games = 0
eval_interval = 100
progress_interval_games = 2
final_games = 1

[value]
preset = "compact-d4"

[artifacts]
dir = "/tmp/game2048_progress_interval"

[[phases]]
name = "td"
games = 5
learning_mode = "td"
update_order = "backward"
alpha = 0.01
)");

    static_cast<void>(game2048::experiment::RunTrainingProfile(profile));
    std::ifstream trainLog(dir / "train.log");
    const std::string text((std::istreambuf_iterator<char>(trainLog)), std::istreambuf_iterator<char>());
    std::filesystem::remove_all(dir);

    EXPECT_TRUE(text.find("trained_games=2") != std::string::npos);
    EXPECT_TRUE(text.find("trained_games=4") != std::string::npos);
    EXPECT_TRUE(text.find("trained_games=5") != std::string::npos);
}

TEST_CASE(BenchmarkProfile_TdlBest_Uses_Tdl_Eval_Games_And_Environment) {
    const std::filesystem::path dir = "/tmp/game2048_tdl_best_bench";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    const std::filesystem::path weights = dir / "weights.nt5";
    game2048::ai::NtupleNetwork network(
        game2048::ai::PatternSetForPreset(game2048::ai::NtuplePreset::Tdl8x6KMatsuzaki));
    network.Save(weights.string());

    game2048::experiment::ExperimentProfile profile;
    profile.run.name = "tdl-best-bench";
    profile.run.seed = 700000;
    profile.eval.mode = "tdl-best";
    profile.eval.games = 3;
    profile.search.finalGames = 1;
    profile.artifacts.dir = dir.string();

    const auto summary = game2048::experiment::RunBenchmarkProfile(profile, weights.string());
    const auto expected = game2048::ai::EvaluateTdlBest(network, 700000U, 3);
    std::filesystem::remove_all(dir);

    EXPECT_EQ(summary.games, std::size_t {3});
    EXPECT_NEAR(summary.averageScore,
                static_cast<double>(expected.totalScore) / static_cast<double>(expected.games),
                1e-9);
    EXPECT_TRUE(summary.achievementRates.find(2048) != summary.achievementRates.end());
}

}  // namespace
