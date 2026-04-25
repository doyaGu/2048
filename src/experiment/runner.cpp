#include "experiment/runner.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>

#include "search/ai_engine.h"
#include "training/benchmark.h"
#include "training/selection.h"
#include "value/ntuple.h"

namespace game2048::experiment {

namespace {

namespace fs = std::filesystem;

ai::SearchConfig ToSearchConfig(const SearchProfileConfig& profile) {
    ai::SearchConfig config;
    config.maxDepth = profile.depth;
    config.timeBudgetMs = profile.timeBudgetMs;
    config.useTileDowngrading = profile.downgrade.enabled &&
        profile.downgrade.mode == SearchProfileConfig::DowngradeMode::Leaf;
    config.useRootTileDowngrading = profile.downgrade.enabled &&
        profile.downgrade.mode == SearchProfileConfig::DowngradeMode::Root;
    config.tileDowngradeSteps = profile.downgrade.steps;
    config.tileDowngradeFloorRank = profile.downgrade.floorRank;
    return config;
}

ai::SearchConfig ToEvalSearchConfig(const SearchProfileConfig& profile) {
    ai::SearchConfig config = ToSearchConfig(profile);
    if (profile.evalDepth > 0) {
        config.maxDepth = profile.evalDepth;
    }
    if (profile.evalTimeBudgetMs > 0) {
        config.timeBudgetMs = profile.evalTimeBudgetMs;
    }
    return config;
}

void WriteTextFile(const fs::path& path, const std::string& text) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("could not write file: " + path.string());
    }
    output << text;
}

std::string FormatManifest(const ExperimentProfile& profile, const TrainingRunResult& result) {
    const auto patternSet = ai::PatternSetForPreset(profile.value.tuplePreset);
#if defined(NDEBUG)
    const char* buildType = "Release";
#else
    const char* buildType = "Debug";
#endif
    std::ostringstream out;
    out << "{\n"
        << "  \"run_name\": \"" << profile.run.name << "\",\n"
        << "  \"artifact_dir\": \"" << result.artifactDir << "\",\n"
        << "  \"best_weights\": \"" << result.bestWeightsPath << "\",\n"
        << "  \"weight_format_version\": 5,\n"
        << "  \"preset\": \"" << patternSet.name << "\",\n"
        << "  \"storage_mode\": \"" << profile.value.storageMode << "\",\n"
        << "  \"stage_count\": " << (profile.value.stageBoundaries.size() + 1U) << ",\n"
        << "  \"tc_persisted\": " << (result.tcPersisted ? "true" : "false") << ",\n"
        << "  \"build_type\": \"" << buildType << "\",\n"
        << "  \"average_score\": " << result.finalSummary.averageScore << ",\n"
        << "  \"games\": " << result.finalSummary.games << "\n"
        << "}\n";
    return out.str();
}

std::string ResolvedConfigToml(const ExperimentProfile& profile) {
    std::ostringstream out;
    out << "[run]\n"
        << "name = \"" << profile.run.name << "\"\n"
        << "seed = " << profile.run.seed << "\n\n"
        << "[selection]\n"
        << "metric = \"" << training::SelectionMetricName(profile.selection.metric) << "\"\n"
        << "target_tile = " << profile.selection.targetTile << "\n"
        << "confidence_z = " << profile.selection.confidenceZ << "\n"
        << "confirm_games = " << profile.selection.confirmGames << "\n\n"
        << "[search]\n"
        << "depth = " << profile.search.depth << "\n"
        << "time_budget_ms = " << profile.search.timeBudgetMs << "\n\n"
        << "eval_depth = " << profile.search.evalDepth << "\n"
        << "eval_time_budget_ms = " << profile.search.evalTimeBudgetMs << "\n"
        << "eval_games = " << profile.search.evalGames << "\n"
        << "eval_interval = " << profile.search.evalInterval << "\n"
        << "final_games = " << profile.search.finalGames << "\n"
        << "eval_threads = " << profile.search.evalThreads << "\n\n"
        << "[search.downgrade]\n"
        << "enabled = " << (profile.search.downgrade.enabled ? "true" : "false") << "\n"
        << "mode = \"" << (profile.search.downgrade.mode == SearchProfileConfig::DowngradeMode::Root ? "root" : "leaf") << "\"\n"
        << "steps = " << profile.search.downgrade.steps << "\n"
        << "floor_rank = " << profile.search.downgrade.floorRank << "\n\n"
        << "[value]\n"
        << "preset = \"" << ai::PatternSetForPreset(profile.value.tuplePreset).name << "\"\n"
        << "optimistic_init = " << profile.value.optimisticInit << "\n"
        << "multistage = " << (profile.value.multistage ? "true" : "false") << "\n\n"
        << "[value.storage]\n"
        << "mode = \"" << profile.value.storageMode << "\"\n\n"
        << "[artifacts]\n"
        << "dir = \"" << profile.artifacts.dir << "\"\n";
    return out.str();
}

BenchmarkSummary EvaluateNetwork(const ExperimentProfile& profile, const ai::NtupleNetwork& network,
                                 std::uint64_t seed, double priorWeight, std::size_t gamesOverride = 0) {
    ai::BenchmarkRunner runner;
    const std::shared_ptr<const ai::NtupleNetwork> networkView(&network, [](const ai::NtupleNetwork*) {});
    const auto results = runner.Run({
        gamesOverride == 0 ? profile.search.evalGames : gamesOverride,
        seed,
        false,
        profile.search.evalAgent,
        ToEvalSearchConfig(profile.search),
        std::nullopt,
        networkView,
        priorWeight,
        profile.search.evalThreads,
        std::nullopt
    });
    return SummarizeBenchmark(results, 0.0);
}

}  // namespace

TrainingRunResult RunTrainingProfile(const ExperimentProfile& profile, const std::string& sourceProfilePath) {
    const fs::path artifactDir(profile.artifacts.dir);
    fs::create_directories(artifactDir);
    if (!sourceProfilePath.empty()) {
        SaveProfileCopy(sourceProfilePath, (artifactDir / "config.toml").string());
    } else {
        WriteTextFile(artifactDir / "config.toml", ResolvedConfigToml(profile));
    }

    std::ofstream metrics(artifactDir / "metrics.csv");
    if (!metrics) {
        throw std::runtime_error("could not write metrics.csv");
    }
    metrics << "phase,trained_games,selection_value,average_score,average_max_tile,"
               "raw_selection_value,score_stddev,rate_1024,rate_2048,rate_4096,rate_8192,rate_16384,rate_32768,"
               "average_nodes_per_move,average_leaf_evals_per_move,transposition_hit_rate,"
               "train_games_per_sec,train_moves_per_sec,mean_abs_td_error,rms_td_error,max_abs_td_error,"
               "stage0_updates,stage1_updates,stage_other_updates,tc_touched_entries,confirmed\n";

    std::ofstream trainLog(artifactDir / "train.log");
    if (!trainLog) {
        throw std::runtime_error("could not write train.log");
    }

    const std::vector<int> stageBoundaries = profile.value.stageBoundaries.empty()
        ? ai::DefaultStageBoundaries()
        : profile.value.stageBoundaries;
    ai::NtupleNetwork network(ai::PatternSetForPreset(profile.value.tuplePreset),
                              static_cast<float>(profile.value.optimisticInit),
                              profile.value.multistage ? stageBoundaries : std::vector<int> {});
    network.SetProfileMetadata("run=" + profile.run.name + ";storage=" + profile.value.storageMode);
    ai::NtupleTrainer trainer(network);
    BenchmarkSummary bestSummary;
    bestSummary.averageScore = -std::numeric_limits<double>::infinity();
    const fs::path bestWeightsPath = artifactDir / "best.weights";
    bool bestSaved = false;
    bool bestTcPersisted = false;
    const training::SelectionPolicy selection = profile.selection;
    training::SelectionPolicy rawSelection = selection;
    rawSelection.confidenceZ = 0.0;

    std::size_t phaseIndex = 0;
    for (const auto& phase : profile.phases) {
        std::size_t trainedGames = 0;
        while (trainedGames < phase.games) {
            const std::size_t chunkGames = profile.search.evalInterval == 0
                ? phase.games - trainedGames
                : std::min(profile.search.evalInterval, phase.games - trainedGames);
            ai::NtupleTrainingOptions options;
            options.games = chunkGames;
            options.seed = profile.run.seed + static_cast<std::uint64_t>(phaseIndex * 2000000U + trainedGames);
            options.alpha = phase.alpha;
            options.finalAlpha = phase.finalAlpha;
            options.explorationRate = phase.epsilon;
            options.finalExplorationRate = phase.finalEpsilon;
            options.priorWeight = phase.priorWeight;
            options.learningMode = phase.learningMode;
            options.tuplePreset = profile.value.tuplePreset;
            options.useMultistage = profile.value.multistage;
            options.stageBoundaries = stageBoundaries;
            const auto trainStarted = std::chrono::steady_clock::now();
            const ai::NtupleTrainingStats trainStats = trainer.Train(options);
            const double trainElapsedSeconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - trainStarted).count();
            trainedGames += chunkGames;
            const double gamesPerSecond = trainElapsedSeconds > 0.0
                ? static_cast<double>(trainStats.games) / trainElapsedSeconds
                : 0.0;
            const double movesPerSecond = trainElapsedSeconds > 0.0
                ? static_cast<double>(trainStats.moves) / trainElapsedSeconds
                : 0.0;
            trainLog << "phase=" << phase.name
                     << " trained_games=" << trainedGames
                     << " chunk_games=" << trainStats.games
                     << " chunk_moves=" << trainStats.moves
                     << " train_seconds=" << trainElapsedSeconds
                     << " games_per_sec=" << gamesPerSecond
                     << " moves_per_sec=" << movesPerSecond
                     << " mean_abs_td_error=" << trainStats.meanAbsTdError
                     << " rms_td_error=" << trainStats.rmsTdError
                     << " max_abs_td_error=" << trainStats.maxAbsTdError
                     << " stage0_updates=" << (trainStats.stageUpdates.empty() ? 0 : trainStats.stageUpdates[0])
                     << " stage1_updates=" << (trainStats.stageUpdates.size() > 1 ? trainStats.stageUpdates[1] : 0)
                     << " tc_touched_entries=" << trainStats.tcTouchedEntries
                     << " max_tile=" << trainStats.maxTile << '\n';
            trainLog.flush();

            if (profile.search.evalGames > 0) {
                const BenchmarkSummary summary = EvaluateNetwork(profile, network,
                    profile.run.seed + 1000003ULL + static_cast<std::uint64_t>(phaseIndex * 10000U + trainedGames),
                    phase.priorWeight);
                BenchmarkSummary selectionSummary = summary;
                bool confirmed = false;
                if (training::IsBetterSelection(summary, bestSummary, selection) && selection.confirmGames > 0) {
                    selectionSummary = EvaluateNetwork(profile, network,
                        profile.run.seed + 9000003ULL +
                            static_cast<std::uint64_t>(phaseIndex * 10000U + trainedGames),
                        phase.priorWeight,
                        selection.confirmGames);
                    confirmed = true;
                }
                const double value = training::SelectionValue(selectionSummary, selection);
                const double rawValue = training::SelectionValue(selectionSummary, rawSelection);
                const auto rate = [&](int tile) {
                    const auto found = selectionSummary.achievementRates.find(tile);
                    return found == selectionSummary.achievementRates.end() ? 0.0 : found->second;
                };
                const std::size_t stage0Updates = trainStats.stageUpdates.empty() ? 0 : trainStats.stageUpdates[0];
                const std::size_t stage1Updates = trainStats.stageUpdates.size() > 1 ? trainStats.stageUpdates[1] : 0;
                std::size_t stageOtherUpdates = 0;
                for (std::size_t index = 2; index < trainStats.stageUpdates.size(); ++index) {
                    stageOtherUpdates += trainStats.stageUpdates[index];
                }
                metrics << phase.name << ',' << trainedGames << ',' << value << ','
                        << selectionSummary.averageScore << ',' << selectionSummary.averageMaxTile << ','
                        << rawValue << ',' << selectionSummary.scoreStddev << ','
                        << rate(1024) << ',' << rate(2048) << ',' << rate(4096) << ','
                        << rate(8192) << ',' << rate(16384) << ',' << rate(32768) << ','
                        << selectionSummary.averageNodesPerMove << ',' << selectionSummary.averageLeafEvalsPerMove << ','
                        << selectionSummary.transpositionHitRate << ','
                        << gamesPerSecond << ',' << movesPerSecond << ','
                        << trainStats.meanAbsTdError << ',' << trainStats.rmsTdError << ','
                        << trainStats.maxAbsTdError << ','
                        << stage0Updates << ',' << stage1Updates << ',' << stageOtherUpdates << ','
                        << trainStats.tcTouchedEntries << ',' << (confirmed ? 1 : 0) << '\n';
                metrics.flush();
                trainLog << "phase=" << phase.name
                         << " trained_games=" << trainedGames
                         << " selection_value=" << value
                         << " raw_selection_value=" << rawValue
                         << " average_score=" << selectionSummary.averageScore
                         << " score_stddev=" << selectionSummary.scoreStddev
                         << " confirmed=" << (confirmed ? 1 : 0) << '\n';
                if (training::IsBetterSelection(selectionSummary, bestSummary, selection)) {
                    bestSummary = selectionSummary;
                    network.Save(bestWeightsPath.string());
                    bestSaved = true;
                    bestTcPersisted = network.HasTcState();
                }
            }
        }
        const fs::path phasePath = artifactDir / ("phase-" + phase.name + ".weights");
        network.Save(phasePath.string());
        ++phaseIndex;
    }

    if (!bestSaved) {
        network.Save(bestWeightsPath.string());
        bestTcPersisted = network.HasTcState();
    }

    TrainingRunResult result;
    result.artifactDir = artifactDir.string();
    result.bestWeightsPath = bestWeightsPath.string();
    result.tcPersisted = bestTcPersisted;
    result.finalSummary = RunBenchmarkProfile(profile, result.bestWeightsPath);
    WriteTextFile(artifactDir / "manifest.json", FormatManifest(profile, result));
    std::cout << "best_weights=" << result.bestWeightsPath
              << " average_score=" << result.finalSummary.averageScore << '\n';
    return result;
}

BenchmarkSummary RunBenchmarkProfile(const ExperimentProfile& profile, const std::string& weightsPath) {
    const fs::path artifactDir(profile.artifacts.dir);
    fs::create_directories(artifactDir);
    const auto started = std::chrono::steady_clock::now();
    ai::BenchmarkRunner runner;
    const auto results = runner.Run({
        profile.search.finalGames,
        profile.run.seed,
        false,
        profile.search.agent,
        ToSearchConfig(profile.search),
        weightsPath,
        nullptr,
        profile.phases.empty() ? 0.0 : profile.phases.back().priorWeight,
        profile.search.evalThreads,
        std::nullopt
    });
    const double elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    const BenchmarkSummary summary = SummarizeBenchmark(results, elapsed);
    const std::string text = FormatBenchmarkSummary(summary);
    WriteTextFile(artifactDir / "bench.txt", text + "\n");
    std::cout << text << '\n';
    return summary;
}

MatrixRunResult RunMatrixProfile(const ExperimentProfile& profile, int maxJobs) {
    const fs::path runDir(profile.artifacts.dir);
    fs::create_directories(runDir);
    std::vector<ExperimentProfile> jobs = ExpandMatrixJobs(profile);
    std::vector<TrainingRunResult> results;
    std::vector<std::future<TrainingRunResult>> running;
    std::size_t next = 0;
    while (next < jobs.size() || !running.empty()) {
        while (next < jobs.size() && static_cast<int>(running.size()) < maxJobs) {
            ExperimentProfile job = jobs[next++];
            running.push_back(std::async(std::launch::async, [job]() {
                return RunTrainingProfile(job);
            }));
        }
        results.push_back(running.front().get());
        running.erase(running.begin());
    }

    const training::SelectionPolicy selection = profile.selection;
    std::size_t bestIndex = 0;
    for (std::size_t index = 1; index < results.size(); ++index) {
        if (training::IsBetterSelection(results[index].finalSummary, results[bestIndex].finalSummary, selection)) {
            bestIndex = index;
        }
    }

    MatrixRunResult matrix;
    matrix.summaryCsvPath = (runDir / "summary.csv").string();
    matrix.bestWeightsPath = (runDir / "best" / "best.weights").string();
    matrix.jobs = results;

    std::ofstream summary(matrix.summaryCsvPath);
    if (!summary) {
        throw std::runtime_error("could not write matrix summary");
    }
    summary << "artifact_dir,selection_value,average_score,best_weights\n";
    for (const auto& result : results) {
        summary << result.artifactDir << ','
                << training::SelectionValue(result.finalSummary, selection) << ','
                << result.finalSummary.averageScore << ','
                << result.bestWeightsPath << '\n';
    }

    fs::create_directories(runDir / "best");
    fs::copy_file(results[bestIndex].bestWeightsPath, matrix.bestWeightsPath,
                  fs::copy_options::overwrite_existing);
    fs::copy_file(fs::path(results[bestIndex].artifactDir) / "bench.txt", runDir / "best" / "bench.txt",
                  fs::copy_options::overwrite_existing);

    std::cout << "summary=" << matrix.summaryCsvPath << '\n'
              << "best_weights=" << matrix.bestWeightsPath << '\n';
    return matrix;
}

}  // namespace game2048::experiment
