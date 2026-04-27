#include "experiment/runner.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#endif

#include "search/ai_engine.h"
#include "training/benchmark.h"
#include "training/selection.h"
#include "value/ntuple.h"
#include "value/tdl_compat.h"

namespace game2048::experiment {

namespace {

namespace fs = std::filesystem;

ai::SearchConfig ToSearchConfig(const SearchProfileConfig& profile) {
    ai::SearchConfig config;
    config.maxDepth = profile.depth;
    config.timeBudgetMs = profile.timeBudgetMs;
    if (profile.fixedPly) {
        config.iterativeDeepening = false;
        config.timeBudgetMs = 0;
    }
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
    if (profile.fixedPly) {
        config.iterativeDeepening = false;
        config.timeBudgetMs = 0;
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

std::uint64_t CurrentRssKb() {
#if defined(__APPLE__)
    rusage usage {};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return static_cast<std::uint64_t>(usage.ru_maxrss / 1024);
    }
#elif defined(__unix__)
    rusage usage {};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return static_cast<std::uint64_t>(usage.ru_maxrss);
    }
#endif
    return 0;
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
        << "fixed_ply = " << (profile.search.fixedPly ? "true" : "false") << "\n\n"
        << "eval_depth = " << profile.search.evalDepth << "\n"
        << "eval_time_budget_ms = " << profile.search.evalTimeBudgetMs << "\n"
        << "eval_games = " << profile.search.evalGames << "\n"
        << "eval_interval = " << profile.search.evalInterval << "\n"
        << "progress_interval_games = " << profile.search.progressIntervalGames << "\n"
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

BenchmarkSummary SummaryFromTdlStats(const ai::NtupleTrainingStats& stats) {
    BenchmarkSummary summary;
    summary.games = stats.games;
    summary.averageScore = stats.games == 0
        ? 0.0
        : static_cast<double>(stats.totalScore) / static_cast<double>(stats.games);
    summary.averageMoves = stats.games == 0
        ? 0.0
        : static_cast<double>(stats.moves) / static_cast<double>(stats.games);
    summary.averageMaxTile = static_cast<double>(stats.maxTile);
    return summary;
}

std::vector<BenchmarkGameStats> RunTdlBestBenchmarkGames(const ai::NtupleNetwork& network,
                                                         std::uint32_t seed,
                                                         std::size_t games) {
    std::vector<BenchmarkGameStats> results;
    results.reserve(games);
    ai::TdlRandom rng(seed);
    for (std::size_t gameIndex = 0; gameIndex < games; ++gameIndex) {
        FastBoard board = rng.InitBoard();
        BenchmarkGameStats stats;
        stats.seed = static_cast<std::uint64_t>(seed) + gameIndex;
        while (true) {
            const ai::TdlCandidateMove best = ai::ChooseTdlBestMove(board, network);
            if (!best.valid) {
                break;
            }
            stats.score += best.move.scoreDelta;
            ++stats.moves;
            board = FastBoard(best.move.board);
            rng.SpawnNext(board);
        }
        stats.maxTile = board.MaxTile();
        results.push_back(stats);
    }
    return results;
}

TrainingRunResult RunTdlForwardTrainingProfileInternal(const ExperimentProfile& profile,
                                                       const std::string& sourceProfilePath) {
    if (profile.eval.mode != "tdl-best") {
        throw std::runtime_error("tdl-forward-td train requires eval.mode = \"tdl-best\"");
    }
    const fs::path artifactDir(profile.artifacts.dir);
    fs::create_directories(artifactDir);
    if (!sourceProfilePath.empty()) {
        SaveProfileCopy(sourceProfilePath, (artifactDir / "config.toml").string());
    } else {
        WriteTextFile(artifactDir / "config.toml", ResolvedConfigToml(profile));
    }

    ai::NtupleNetwork network(ai::PatternSetForPreset(profile.value.tuplePreset),
                              static_cast<float>(profile.value.optimisticInit),
                              profile.value.multistage ? profile.value.stageBoundaries : std::vector<int> {});
    network.SetProfileMetadata("run=" + profile.run.name + ";trainer=tdl-forward-td");

    std::vector<std::size_t> checkpoints = profile.trainer.checkpoints;
    if (checkpoints.empty()) {
        checkpoints.push_back(profile.trainer.games);
    }
    std::sort(checkpoints.begin(), checkpoints.end());
    checkpoints.erase(std::unique(checkpoints.begin(), checkpoints.end()), checkpoints.end());

    std::ofstream progress(artifactDir / "train-progress.csv");
    if (!progress) {
        throw std::runtime_error("could not write train-progress.csv");
    }
    progress << "checkpoint,trained,games_chunk,moves_chunk,updates_chunk,avg_score_chunk,games_per_sec,moves_per_sec,rss_kb\n";
    std::ofstream metrics(artifactDir / "metrics.csv");
    if (!metrics) {
        throw std::runtime_error("could not write metrics.csv");
    }
    metrics << "checkpoint,current_avg,current_moves,max_tile\n";

    ai::TdlRandom trainRng(static_cast<std::uint32_t>(profile.run.seed));
    BenchmarkSummary finalSummary;
    std::size_t trained = 0;
    const auto runStarted = std::chrono::steady_clock::now();
    for (std::size_t checkpoint : checkpoints) {
        if (checkpoint > profile.trainer.games) {
            continue;
        }
        while (trained < checkpoint) {
            const std::size_t remaining = checkpoint - trained;
            const std::size_t chunk = profile.trainer.progressIntervalGames == 0
                ? remaining
                : std::min(profile.trainer.progressIntervalGames, remaining);
            ai::TdlForwardTrainingOptions training;
            training.games = chunk;
            training.seed = static_cast<std::uint32_t>(profile.run.seed);
            training.alpha = profile.trainer.alpha;
            training.learningMode = profile.trainer.learningMode;
            const auto started = std::chrono::steady_clock::now();
            const ai::NtupleTrainingStats chunkStats = ai::TrainTdlForward(network, trainRng, training);
            const double elapsedSec = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - started).count();
            trained += chunk;
            const double chunkAvg = chunkStats.games == 0
                ? 0.0
                : static_cast<double>(chunkStats.totalScore) / static_cast<double>(chunkStats.games);
            const double gamesPerSec = elapsedSec > 0.0 ? static_cast<double>(chunkStats.games) / elapsedSec : 0.0;
            const double movesPerSec = elapsedSec > 0.0 ? static_cast<double>(chunkStats.moves) / elapsedSec : 0.0;
            const std::uint64_t rssKb = CurrentRssKb();
            progress << checkpoint << ',' << trained << ',' << chunkStats.games << ','
                     << chunkStats.moves << ',' << chunkStats.updates << ',' << chunkAvg << ','
                     << gamesPerSec << ',' << movesPerSec << ',' << rssKb << '\n';
            progress.flush();
            const double runElapsedSec = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - runStarted).count();
            std::cout << "tdl_train_progress checkpoint=" << checkpoint
                      << " trained=" << trained
                      << " chunk_games=" << chunkStats.games
                      << " chunk_avg=" << chunkAvg
                      << " games_per_sec=" << gamesPerSec
                      << " moves_per_sec=" << movesPerSec
                      << " elapsed_seconds=" << runElapsedSec
                      << " rss_kb=" << rssKb << '\n';
        }

        if (profile.eval.games > 0) {
            ai::TdlRandom evalRng(static_cast<std::uint32_t>(profile.run.seed + checkpoint));
            const ai::NtupleTrainingStats evalStats = ai::EvaluateTdlBest(network, evalRng, profile.eval.games);
            finalSummary = SummaryFromTdlStats(evalStats);
            metrics << checkpoint << ',' << finalSummary.averageScore << ','
                    << evalStats.moves << ',' << evalStats.maxTile << '\n';
            metrics.flush();
            std::cout << "tdl_eval checkpoint=" << checkpoint
                      << " current_avg=" << finalSummary.averageScore
                      << " moves=" << evalStats.moves
                      << " max_tile=" << evalStats.maxTile << '\n';
        }
        network.Save((artifactDir / ("checkpoint-" + std::to_string(checkpoint) + ".weights")).string());
    }

    const fs::path bestWeightsPath = artifactDir / "best.weights";
    network.Save(bestWeightsPath.string());
    TrainingRunResult result;
    result.artifactDir = artifactDir.string();
    result.bestWeightsPath = bestWeightsPath.string();
    result.tcPersisted = network.HasTcState();
    result.finalSummary = finalSummary;
    WriteTextFile(artifactDir / "manifest.json", FormatManifest(profile, result));
    std::cout << "best_weights=" << result.bestWeightsPath
              << " average_score=" << result.finalSummary.averageScore << '\n';
    return result;
}

}  // namespace

TrainingRunResult RunTrainingProfile(const ExperimentProfile& profile, const std::string& sourceProfilePath) {
    if (profile.trainer.mode == "tdl-forward-td") {
        return RunTdlForwardTrainingProfileInternal(profile, sourceProfilePath);
    }
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
               "train_games_per_sec,train_moves_per_sec,train_elapsed_seconds,rss_kb,"
               "mean_abs_td_error,rms_td_error,max_abs_td_error,"
               "stage0_updates,stage1_updates,stage_other_updates,tc_touched_entries,"
               "replay_starts,replay_captured,confirmed\n";

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
    const auto runStarted = std::chrono::steady_clock::now();

    std::size_t phaseIndex = 0;
    for (const auto& phase : profile.phases) {
        if (phase.enableMultistage) {
            network.EnableStages(stageBoundaries);
        }
        std::size_t trainedGames = 0;
        while (trainedGames < phase.games) {
            const std::size_t remainingGames = phase.games - trainedGames;
            std::size_t chunkGames = remainingGames;
            auto clampToNextBoundary = [&](std::size_t interval) {
                if (interval == 0) {
                    return;
                }
                std::size_t untilBoundary = interval - (trainedGames % interval);
                if (untilBoundary == 0) {
                    untilBoundary = interval;
                }
                chunkGames = std::min(chunkGames, untilBoundary);
            };
            clampToNextBoundary(profile.search.evalInterval);
            clampToNextBoundary(profile.search.progressIntervalGames);
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
            options.startRank = phase.startRank;
            options.replayStartRank = phase.replayStartRank;
            options.replayCaptureRank = phase.replayCaptureRank;
            options.updateOrder = phase.updateOrder;
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
            const double elapsedSeconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - runStarted).count();
            const std::uint64_t rssKb = CurrentRssKb();
            trainLog << "phase=" << phase.name
                     << " trained_games=" << trainedGames
                     << " chunk_games=" << trainStats.games
                     << " chunk_moves=" << trainStats.moves
                     << " train_seconds=" << trainElapsedSeconds
                     << " elapsed_seconds=" << elapsedSeconds
                     << " games_per_sec=" << gamesPerSecond
                     << " moves_per_sec=" << movesPerSecond
                     << " rss_kb=" << rssKb
                     << " mean_abs_td_error=" << trainStats.meanAbsTdError
                     << " rms_td_error=" << trainStats.rmsTdError
                     << " max_abs_td_error=" << trainStats.maxAbsTdError
                     << " stage0_updates=" << (trainStats.stageUpdates.empty() ? 0 : trainStats.stageUpdates[0])
                     << " stage1_updates=" << (trainStats.stageUpdates.size() > 1 ? trainStats.stageUpdates[1] : 0)
                     << " tc_touched_entries=" << trainStats.tcTouchedEntries
                     << " replay_starts=" << trainStats.replayStarts
                     << " replay_captured=" << trainStats.replayCaptured
                     << " max_tile=" << trainStats.maxTile << '\n';
            trainLog.flush();

            const bool atEvalBoundary = profile.search.evalInterval > 0 && trainedGames % profile.search.evalInterval == 0;
            const bool atPhaseEnd = trainedGames == phase.games;
            if (profile.search.evalGames > 0 && (atEvalBoundary || atPhaseEnd)) {
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
                        << elapsedSeconds << ',' << rssKb << ','
                        << trainStats.meanAbsTdError << ',' << trainStats.rmsTdError << ','
                        << trainStats.maxAbsTdError << ','
                        << stage0Updates << ',' << stage1Updates << ',' << stageOtherUpdates << ','
                        << trainStats.tcTouchedEntries << ','
                        << trainStats.replayStarts << ',' << trainStats.replayCaptured << ','
                        << (confirmed ? 1 : 0) << '\n';
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

    if (profile.eval.mode == "tdl-best") {
        const ai::NtupleNetwork network = ai::NtupleNetwork::Load(weightsPath);
        const std::size_t games = profile.eval.games == 0 ? profile.search.finalGames : profile.eval.games;
        const auto results = RunTdlBestBenchmarkGames(network, static_cast<std::uint32_t>(profile.run.seed), games);
        const double elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        const BenchmarkSummary summary = SummarizeBenchmark(results, elapsed);
        const std::string text = std::string("mode: tdl-best\n") + FormatBenchmarkSummary(summary);
        WriteTextFile(artifactDir / "bench.txt", text + "\n");
        std::cout << text << '\n';
        return summary;
    }

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

namespace {

std::string ShellQuote(const std::string& value) {
    std::string quoted = "'";
    for (char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted += ch;
        }
    }
    quoted += "'";
    return quoted;
}

double LastAverageFromText(const std::string& text) {
    double last = std::numeric_limits<double>::quiet_NaN();
    std::stringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.find("total:") == std::string::npos && line.find("summary") == std::string::npos) {
            continue;
        }
        const std::size_t avgPos = line.find("avg=");
        if (avgPos == std::string::npos) {
            continue;
        }
        std::string token = line.substr(avgPos + 4U);
        const std::size_t end = token.find_first_of(" \t,;");
        if (end != std::string::npos) {
            token = token.substr(0, end);
        }
        try {
            std::size_t parsed = 0;
            const double value = std::stod(token, &parsed);
            if (parsed == token.size()) {
                last = value;
            }
        } catch (const std::exception&) {
        }
    }
    return last;
}

std::string ReferenceCacheKey(const ExperimentProfile& profile, std::size_t checkpoint) {
    const auto patternSet = ai::PatternSetForPreset(profile.value.tuplePreset);
    std::ostringstream key;
    key << profile.run.seed << ','
        << patternSet.name << ','
        << profile.value.optimisticInit << ','
        << profile.trainer.alpha << ','
        << checkpoint << ','
        << profile.eval.games;
    return key.str();
}

std::optional<double> ReadReferenceCache(const fs::path& path, const ExperimentProfile& profile,
                                         std::size_t checkpoint) {
    std::ifstream input(path);
    if (!input) {
        return std::nullopt;
    }
    const std::string wanted = ReferenceCacheKey(profile, checkpoint);
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line.rfind("seed,", 0) == 0) {
            continue;
        }
        std::vector<std::string> cells;
        std::stringstream stream(line);
        std::string cell;
        while (std::getline(stream, cell, ',')) {
            cells.push_back(cell);
        }
        if (cells.size() < 7) {
            continue;
        }
        std::ostringstream actual;
        actual << cells[0] << ',' << cells[1] << ',' << cells[2] << ','
               << cells[3] << ',' << cells[4] << ',' << cells[5];
        if (actual.str() == wanted) {
            return std::stod(cells[6]);
        }
    }
    return std::nullopt;
}

void AppendReferenceCache(const fs::path& path, const ExperimentProfile& profile,
                          std::size_t checkpoint, double average) {
    if (!path.parent_path().empty()) {
        fs::create_directories(path.parent_path());
    }
    const bool writeHeader = !fs::exists(path) || fs::file_size(path) == 0;
    std::ofstream output(path, std::ios::app);
    if (!output) {
        throw std::runtime_error("could not write reference cache: " + path.string());
    }
    if (writeHeader) {
        output << "seed,preset,optimistic_init,alpha,checkpoint,eval_games,tdl_avg\n";
    }
    output << ReferenceCacheKey(profile, checkpoint) << ',' << average << '\n';
}

double RunTdlReferenceAverage(const std::string& tdlBinPath, const ExperimentProfile& profile,
                              std::size_t checkpoint, const fs::path& outputPath,
                              const fs::path& cachePath) {
    if (tdlBinPath.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (!cachePath.empty()) {
        if (const auto cached = ReadReferenceCache(cachePath, profile, checkpoint)) {
            return *cached;
        }
    }
    const auto patternSet = ai::PatternSetForPreset(profile.value.tuplePreset);
    const double initialEntry = profile.value.optimisticInit /
                                static_cast<double>(patternSet.basePatterns.size() * (patternSet.useD4 ? 8U : 1U));
    const std::size_t trainK = std::max<std::size_t>(1, checkpoint / 1000U);
    const std::size_t evalK = std::max<std::size_t>(1, profile.eval.games / 1000U);
    std::ostringstream command;
    command << ShellQuote(tdlBinPath)
            << " -n 8x6patt=" << initialEntry
            << " -a " << profile.trainer.alpha
            << " -s " << profile.run.seed
            << " -p 1"
            << " -t " << trainK
            << " -e " << evalK
            << " 2>&1";

    std::array<char, 4096> buffer {};
    std::string output;
    FILE* pipe = popen(command.str().c_str(), "r");
    if (pipe == nullptr) {
        throw std::runtime_error("failed to run TDL2048 reference binary");
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }
    static_cast<void>(pclose(pipe));
    WriteTextFile(outputPath, output);
    const double average = LastAverageFromText(output);
    if (!cachePath.empty() && !std::isnan(average)) {
        AppendReferenceCache(cachePath, profile, checkpoint, average);
    }
    return average;
}

}  // namespace

ParityRunResult RunParityProfile(const ExperimentProfile& profile, const std::string& tdlBinPath) {
    if (profile.trainer.mode != "tdl-forward-td") {
        throw std::runtime_error("parity requires trainer.mode = \"tdl-forward-td\"");
    }
    if (profile.eval.mode != "tdl-best") {
        throw std::runtime_error("parity requires eval.mode = \"tdl-best\"");
    }

    const fs::path artifactDir(profile.artifacts.dir);
    fs::create_directories(artifactDir);
    ai::NtupleNetwork network(ai::PatternSetForPreset(profile.value.tuplePreset),
                              static_cast<float>(profile.value.optimisticInit),
                              profile.value.multistage ? profile.value.stageBoundaries : std::vector<int> {});

    std::vector<std::size_t> checkpoints = profile.trainer.checkpoints;
    if (checkpoints.empty()) {
        checkpoints.push_back(profile.trainer.games);
    }
    std::sort(checkpoints.begin(), checkpoints.end());
    checkpoints.erase(std::unique(checkpoints.begin(), checkpoints.end()), checkpoints.end());

    ParityRunResult result;
    result.artifactDir = artifactDir.string();
    result.parityCsvPath = (artifactDir / "parity.csv").string();

    std::ofstream csv(result.parityCsvPath);
    if (!csv) {
        throw std::runtime_error("could not write parity.csv");
    }
    csv << "checkpoint,current_avg,current_moves,tdl_avg,relative_gap,status\n";
    const fs::path referenceCachePath = profile.eval.referenceCachePath.empty()
        ? artifactDir / "tdl-reference-cache.csv"
        : fs::path(profile.eval.referenceCachePath);
    const fs::path progressPath = artifactDir / "parity-progress.csv";
    std::ofstream progress(progressPath);
    if (!progress) {
        throw std::runtime_error("could not write parity-progress.csv");
    }
    progress << "checkpoint,trained,games_chunk,moves_chunk,updates_chunk,avg_score_chunk,games_per_sec,moves_per_sec\n";

    ai::TdlRandom trainRng(static_cast<std::uint32_t>(profile.run.seed));
    std::size_t trained = 0;
    for (std::size_t checkpoint : checkpoints) {
        if (checkpoint > profile.trainer.games) {
            continue;
        }
        ai::NtupleTrainingStats trainStats;
        const auto checkpointStarted = std::chrono::steady_clock::now();
        while (trained < checkpoint) {
            const std::size_t remaining = checkpoint - trained;
            const std::size_t chunk = profile.trainer.progressIntervalGames == 0
                ? remaining
                : std::min(profile.trainer.progressIntervalGames, remaining);
            ai::TdlForwardTrainingOptions training;
            training.games = chunk;
            training.seed = static_cast<std::uint32_t>(profile.run.seed);
            training.alpha = profile.trainer.alpha;
            training.learningMode = profile.trainer.learningMode;
            const auto started = std::chrono::steady_clock::now();
            const ai::NtupleTrainingStats chunkStats = ai::TrainTdlForward(network, trainRng, training);
            const double chunkElapsedSec = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - started).count();
            trained += chunk;
            trainStats.games += chunkStats.games;
            trainStats.moves += chunkStats.moves;
            trainStats.updates += chunkStats.updates;
            trainStats.totalScore += chunkStats.totalScore;
            trainStats.maxTile = std::max(trainStats.maxTile, chunkStats.maxTile);
            const double chunkAvg = chunkStats.games == 0
                ? 0.0
                : static_cast<double>(chunkStats.totalScore) / static_cast<double>(chunkStats.games);
            const double gamesPerSec = chunkElapsedSec > 0.0
                ? static_cast<double>(chunkStats.games) / chunkElapsedSec
                : 0.0;
            const double movesPerSec = chunkElapsedSec > 0.0
                ? static_cast<double>(chunkStats.moves) / chunkElapsedSec
                : 0.0;
            progress << checkpoint << ','
                     << trained << ','
                     << chunkStats.games << ','
                     << chunkStats.moves << ','
                     << chunkStats.updates << ','
                     << chunkAvg << ','
                     << gamesPerSec << ','
                     << movesPerSec << '\n';
            progress.flush();
            std::cout << "parity_progress checkpoint=" << checkpoint
                      << " trained=" << trained
                      << " chunk_games=" << chunkStats.games
                      << " chunk_avg=" << chunkAvg
                      << " games_per_sec=" << gamesPerSec
                      << " moves_per_sec=" << movesPerSec
                      << '\n';
        }
        const double elapsedSec = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - checkpointStarted).count();

        ai::TdlRandom evalRng(static_cast<std::uint32_t>(profile.run.seed + checkpoint));
        const ai::NtupleTrainingStats evalStats = ai::EvaluateTdlBest(network, evalRng, profile.eval.games);
        const double currentAvg = evalStats.games == 0
            ? 0.0
            : static_cast<double>(evalStats.totalScore) / static_cast<double>(evalStats.games);
        const double tdlAvg = RunTdlReferenceAverage(tdlBinPath, profile, checkpoint,
                                                     artifactDir / ("tdl-" + std::to_string(checkpoint) + ".log"),
                                                     referenceCachePath);
        double gap = 0.0;
        std::string status = "current-only";
        if (!std::isnan(tdlAvg) && tdlAvg > 0.0) {
            gap = std::abs(currentAvg - tdlAvg) / tdlAvg;
            status = gap <= profile.eval.parityTolerance ? "pass" : "fail";
            if (status == "fail") {
                result.passed = false;
            }
        }
        csv << checkpoint << ','
            << currentAvg << ','
            << evalStats.moves << ','
            << tdlAvg << ','
            << gap << ','
            << status << '\n';
        csv.flush();

        std::cout << "checkpoint=" << checkpoint
                  << " trained_chunk_games=" << trainStats.games
                  << " current_avg=" << currentAvg
                  << " tdl_avg=" << tdlAvg
                  << " gap=" << gap
                  << " status=" << status
                  << " train_games_per_sec=" << (elapsedSec > 0.0 ? static_cast<double>(trainStats.games) / elapsedSec : 0.0)
                  << '\n';
        network.Save((artifactDir / ("checkpoint-" + std::to_string(checkpoint) + ".weights")).string());
        if (!result.passed) {
            break;
        }
    }
    network.Save((artifactDir / "current.weights").string());
    std::cout << "parity_csv=" << result.parityCsvPath << '\n';
    return result;
}

}  // namespace game2048::experiment
