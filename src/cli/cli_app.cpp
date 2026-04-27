#include "cli/cli_app.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "core/board.h"
#include "core/game.h"
#include "experiment/profile.h"
#include "experiment/runner.h"
#include "search/ai_engine.h"
#include "value/ntuple.h"
#include "value/ntuple_kernel.h"

namespace game2048 {

namespace {

int ParseBoardCell(const std::string& cellText, int row, int col) {
    std::size_t parsed = 0;
    try {
        const int value = std::stoi(cellText, &parsed);
        if (parsed != cellText.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return value;
    } catch (const std::exception&) {
        std::ostringstream message;
        message << "invalid --board cell at row " << (row + 1)
                << ", column " << (col + 1)
                << ": expected an integer";
        throw std::runtime_error(message.str());
    }
}

std::array<std::array<int, kBoardSize>, kBoardSize> ParseBoardRows(const std::string& rows) {
    std::array<std::array<int, kBoardSize>, kBoardSize> values {};
    std::stringstream rowStream(rows);
    std::string rowText;
    int row = 0;
    while (std::getline(rowStream, rowText, '/')) {
        if (row >= kBoardSize) {
            throw std::runtime_error("--board has too many rows");
        }
        std::stringstream cellStream(rowText);
        std::string cellText;
        int col = 0;
        while (std::getline(cellStream, cellText, ',')) {
            if (col >= kBoardSize) {
                throw std::runtime_error("--board row has too many cells");
            }
            values[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] =
                ParseBoardCell(cellText, row, col);
            ++col;
        }
        if (col != kBoardSize) {
            throw std::runtime_error("--board row must have four cells");
        }
        ++row;
    }
    if (row != kBoardSize) {
        throw std::runtime_error("--board must have four rows");
    }
    return values;
}

ai::SearchConfig SearchConfigForProfile(const experiment::ExperimentProfile& profile) {
    ai::SearchConfig config;
    config.maxDepth = profile.search.depth;
    config.timeBudgetMs = profile.search.timeBudgetMs;
    if (profile.search.fixedPly) {
        config.iterativeDeepening = false;
        config.timeBudgetMs = 0;
    }
    config.useTileDowngrading = profile.search.downgrade.enabled &&
        profile.search.downgrade.mode == experiment::SearchProfileConfig::DowngradeMode::Leaf;
    config.useRootTileDowngrading = profile.search.downgrade.enabled &&
        profile.search.downgrade.mode == experiment::SearchProfileConfig::DowngradeMode::Root;
    config.tileDowngradeSteps = profile.search.downgrade.steps;
    config.tileDowngradeFloorRank = profile.search.downgrade.floorRank;
    return config;
}

int RunInspect(const CliOptions& options) {
    Board board = options.boardRows.has_value()
        ? Board::FromRows(ParseBoardRows(*options.boardRows))
        : Game(options.seed).GetBoard();

    ai::AIEngine engine;
    engine.SetAgent(options.agent);
    engine.Expectimax().SetConfig(options.search);
    if (options.profilePath.has_value()) {
        const auto profile = experiment::LoadExperimentProfile(*options.profilePath);
        engine.SetAgent(profile.search.agent);
        engine.Expectimax().SetConfig(SearchConfigForProfile(profile));
    }
    if (options.weightPath.has_value()) {
        ai::NtupleNetwork network = ai::NtupleNetwork::Load(*options.weightPath);
        engine.Expectimax().SetLeafNetwork(network);
    }

    const auto decision = engine.Recommend(FastBoard::FromReference(board));
    const auto breakdown = engine.Expectimax().GetEvaluator().Breakdown(FastBoard::FromReference(board));
    std::cout << "valid=" << (decision.valid ? "true" : "false")
              << " direction=" << static_cast<int>(decision.direction)
              << " nodes=" << decision.stats.nodes
              << " cache_hits=" << decision.stats.cacheHits
              << " depth=" << decision.stats.maxDepthReached
              << " elapsed_ms=" << decision.stats.elapsedMs
              << " evaluation=" << decision.stats.evaluation
              << " breakdown_total=" << breakdown.total
              << '\n';
    return decision.valid ? 0 : 2;
}

int RunMicrobench(const CliOptions& options) {
    const auto profile = experiment::LoadExperimentProfile(*options.profilePath);
    const std::vector<int> stageBoundaries = profile.value.stageBoundaries.empty()
        ? ai::DefaultStageBoundaries()
        : profile.value.stageBoundaries;
    ai::NtupleNetwork network(ai::PatternSetForPreset(profile.value.tuplePreset),
                              static_cast<float>(profile.value.optimisticInit),
                              profile.value.multistage ? stageBoundaries : std::vector<int> {});
    ai::NtupleTrainingOptions training;
    training.games = options.microbenchGames;
    training.seed = profile.run.seed;
    training.tuplePreset = profile.value.tuplePreset;
    training.useMultistage = profile.value.multistage;
    training.stageBoundaries = stageBoundaries;
    if (!profile.phases.empty()) {
        training.alpha = profile.phases.front().alpha;
        training.finalAlpha = profile.phases.front().finalAlpha;
        training.explorationRate = profile.phases.front().epsilon;
        training.finalExplorationRate = profile.phases.front().finalEpsilon;
        training.priorWeight = profile.phases.front().priorWeight;
        training.learningMode = profile.phases.front().learningMode;
        training.updateOrder = profile.phases.front().updateOrder;
        training.replayStartRank = profile.phases.front().replayStartRank;
        training.replayCaptureRank = profile.phases.front().replayCaptureRank;
    }

    ai::NtupleTrainer trainer(network);
    const auto started = std::chrono::steady_clock::now();
    const auto stats = trainer.Train(training);
    const double elapsedMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started).count();
    const double elapsedSec = elapsedMs / 1000.0;
    std::cout << "kernel=" << ai::ntuple_kernel::ActiveKernelName()
              << " games=" << stats.games
              << " moves=" << stats.moves
              << " updates=" << stats.updates
              << " elapsed_ms=" << elapsedMs
              << " games_per_sec=" << (elapsedSec > 0.0 ? static_cast<double>(stats.games) / elapsedSec : 0.0)
              << " moves_per_sec=" << (elapsedSec > 0.0 ? static_cast<double>(stats.moves) / elapsedSec : 0.0)
              << " max_tile=" << stats.maxTile
              << '\n';
    return 0;
}

}  // namespace

int RunCliCommand(const CliOptions& options) {
    if (options.command == CliCommand::Train) {
        const auto profile = experiment::LoadExperimentProfile(*options.profilePath);
        static_cast<void>(experiment::RunTrainingProfile(profile, *options.profilePath));
        return 0;
    }
    if (options.command == CliCommand::Bench) {
        const auto profile = experiment::LoadExperimentProfile(*options.profilePath);
        static_cast<void>(experiment::RunBenchmarkProfile(profile, *options.weightPath));
        return 0;
    }
    if (options.command == CliCommand::Matrix) {
        const auto profile = experiment::LoadExperimentProfile(*options.profilePath);
        static_cast<void>(experiment::RunMatrixProfile(profile, options.maxJobs));
        return 0;
    }
    if (options.command == CliCommand::Inspect) {
        return RunInspect(options);
    }
    if (options.command == CliCommand::Microbench) {
        return RunMicrobench(options);
    }
    if (options.command == CliCommand::Parity) {
        const auto profile = experiment::LoadExperimentProfile(*options.profilePath);
        const auto result = experiment::RunParityProfile(profile, options.tdlBinPath.value_or(""));
        return result.passed ? 0 : 3;
    }
    std::cerr << "play is only available in the GUI executable\n";
    return 1;
}

int RunCli(int argc, char** argv) {
    try {
        const CliOptions options = ParseCliOptions(argc, argv);
        return RunCliCommand(options);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}

}  // namespace game2048
