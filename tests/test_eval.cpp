#include "../src/search/evaluator.h"
#include "../src/search/expectimax.h"
#include "../src/training/benchmark.h"
#include "../src/value/ntuple.h"
#include "../src/tdl/api.h"
#include "../src/tdl/backend_fixed6.h"
#include "../src/tdl/backend_network.h"
#include "../src/tdl/backend_tdl8x6.h"
#include "../src/tdl/tdl8x6_kernel.h"
#include "../src/tdl/types.h"
#include "../src/value/ntuple_kernel.h"
#include "../src/search/transposition_table.h"
#include "../src/core/board.h"
#include "../src/core/board_fast.h"
#include "../src/shared/stats.h"
#include "test_framework.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using game2048::Board;
using game2048::FastBoard;
using game2048::ai::Evaluator;
using game2048::ai::DefaultNtuplePatterns;
using game2048::ai::DowngradeTilesForSearch;
using game2048::ai::EndgameMoveSafetyPenalty;
using game2048::ai::ExpectimaxAgent;
using game2048::ai::PaperTileDowngradeRoot;
using game2048::ai::LearningMode;
using game2048::ai::NodeType;
using game2048::ai::NtupleAgent;
using game2048::ai::NtupleNetwork;
using game2048::ai::NtuplePattern;
using game2048::ai::NtuplePreset;
using game2048::ai::NtupleTraceStep;
using game2048::ai::NtupleTrainingOptions;
using game2048::ai::NtupleTrainingRates;
using game2048::ai::NtupleTrainer;
using game2048::ai::NtupleUpdateOrder;
using game2048::ai::PatternsForPreset;
using game2048::ai::PatternSetForPreset;
using game2048::ai::SearchConfig;
using game2048::ai::SelectionMetric;
using game2048::ai::SelectChanceCellsForSearch;
using game2048::ai::TrainingSelectionValue;
using game2048::ai::AgentKind;
using game2048::ai::BenchmarkOptions;
using game2048::ai::BenchmarkRunner;
using game2048::BenchmarkGameStats;
using game2048::PackedBoard;
using game2048::SummarizeBenchmark;

Board MakeBoard(const std::array<std::array<int, 4>, 4>& rows) {
    return Board::FromRows(rows);
}

}  // namespace

TEST_CASE(Evaluator_Prefers_Empty_And_Orderly_Boards) {
    Evaluator evaluator;
    const FastBoard good = FastBoard::FromReference(
        MakeBoard({{{1024, 256, 64, 16}, {512, 128, 32, 8}, {4, 2, 0, 0}, {0, 0, 0, 0}}}));
    const FastBoard bad = FastBoard::FromReference(
        MakeBoard({{{2, 1024, 4, 16}, {8, 0, 64, 2}, {256, 32, 0, 8}, {4, 512, 128, 16}}}));

    EXPECT_TRUE(evaluator.Evaluate(good) > evaluator.Evaluate(bad));
}

TEST_CASE(BenchmarkRunner_ParallelMatchesSerialForFixedSeeds) {
    SearchConfig search;
    search.maxDepth = 1;
    search.timeBudgetMs = 0;
    search.iterativeDeepening = false;
    search.maxChanceBranchesPerValue = 2;

    BenchmarkOptions serial;
    serial.games = 4;
    serial.seed = 12345;
    serial.agent = AgentKind::Expectimax;
    serial.search = search;
    serial.evalThreads = 1;

    BenchmarkOptions parallel = serial;
    parallel.evalThreads = 2;

    BenchmarkRunner runner;
    const auto serialResults = runner.Run(serial);
    const auto parallelResults = runner.Run(parallel);

    EXPECT_EQ(parallelResults.size(), serialResults.size());
    for (std::size_t index = 0; index < serialResults.size(); ++index) {
        EXPECT_EQ(parallelResults[index].seed, serialResults[index].seed);
        EXPECT_EQ(parallelResults[index].score, serialResults[index].score);
        EXPECT_EQ(parallelResults[index].maxTile, serialResults[index].maxTile);
        EXPECT_EQ(parallelResults[index].moves, serialResults[index].moves);
    }
}

TEST_CASE(NtupleNetwork_Starts_At_Zero_And_Updates_Toward_Target) {
    NtupleNetwork network({NtuplePattern{{0, 1, 2, 3}}});
    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{16, 8, 4, 2}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}));

    EXPECT_EQ(network.Patterns().size(), static_cast<std::size_t>(1));
    EXPECT_EQ(network.WeightCount(), static_cast<std::size_t>(1u << 16U));
    EXPECT_NEAR(network.Evaluate(board), 0.0, 1e-6);

    const auto stats = network.UpdateToward(board, 100.0, 0.5);

    EXPECT_NEAR(stats.before, 0.0, 1e-6);
    EXPECT_NEAR(stats.error, 100.0, 1e-6);
    EXPECT_TRUE(stats.after > stats.before);
    EXPECT_TRUE(network.Evaluate(board) < 100.0);
}

TEST_CASE(NtupleDefaultPatterns_Include_Board_Symmetries) {
    const auto patterns = DefaultNtuplePatterns();
    auto contains = [&](const std::array<int, 4>& cells) {
        return std::find_if(patterns.begin(), patterns.end(), [&](const NtuplePattern& pattern) {
                   return pattern.cells.size() == cells.size() &&
                          std::equal(pattern.cells.begin(), pattern.cells.end(), cells.begin());
               }) != patterns.end();
    };

    EXPECT_TRUE(contains({0, 1, 2, 3}));
    EXPECT_TRUE(contains({3, 2, 1, 0}));
    EXPECT_TRUE(contains({0, 4, 8, 12}));
    EXPECT_TRUE(contains({12, 8, 4, 0}));
    EXPECT_TRUE(patterns.size() > 12);
}

TEST_CASE(NtuplePresetParser_Uses_Formal_Tdl_Names) {
    const auto patterns = PatternsForPreset(NtuplePreset::Tdl8x6KMatsuzaki);

    EXPECT_EQ(patterns.size(), std::size_t {8});
    for (const auto& pattern : patterns) {
        EXPECT_EQ(pattern.cells.size(), std::size_t {6});
    }

    const auto set = PatternSetForPreset(NtuplePreset::Tdl4x6Khyeh);
    EXPECT_EQ(set.basePatterns.size(), std::size_t {4});
    EXPECT_TRUE(set.useD4);
    EXPECT_TRUE(set.useFixedPath);
}

TEST_CASE(NtuplePreset_FixedPath_Matches_GenericPath) {
    const auto fixedSet = PatternSetForPreset(NtuplePreset::Tdl4x6Khyeh);
    auto genericSet = fixedSet;
    genericSet.useFixedPath = false;
    NtupleNetwork fixed(fixedSet);
    NtupleNetwork generic(genericSet);
    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{128, 64, 32, 16}, {8, 4, 2, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}));

    fixed.UpdateToward(board, 100.0, 0.5);
    generic.UpdateToward(board, 100.0, 0.5);

    EXPECT_NEAR(fixed.Evaluate(board), generic.Evaluate(board), 1e-9);
}

TEST_CASE(NtuplePreset_Tdl8x6_FeatureKeys_Match_TDL2048_Order) {
    NtupleNetwork network(PatternSetForPreset(NtuplePreset::Tdl8x6KMatsuzaki));
    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{2, 4, 8, 16}, {32, 64, 128, 256}, {512, 1024, 2048, 4096}, {0, 0, 0, 0}}}));

    const auto keys = network.FeatureKeysForBoard(board);

    EXPECT_EQ(keys.size(), std::size_t {64});
    EXPECT_EQ(keys[0], std::size_t {0x765321});
    EXPECT_EQ(keys[8], std::size_t {(1u << 24U) + 0xA98765});
    EXPECT_EQ(keys[16], std::size_t {(2u << 24U) + 0x654321});
    EXPECT_EQ(keys[24], std::size_t {(3u << 24U) + 0xA76543});
    EXPECT_EQ(keys[32], std::size_t {(4u << 24U) + 0xBA6321});
    EXPECT_EQ(keys[40], std::size_t {(5u << 24U) + 0x987654});
    EXPECT_EQ(keys[48], std::size_t {(6u << 24U) + 0x876542});
    EXPECT_EQ(keys[56], std::size_t {(7u << 24U) + 0xBA9521});
}

TEST_CASE(TdlRandom_InitAndSpawn_MatchReferenceSequence) {
    game2048::ai::TdlRandom rng(700000U);

    game2048::FastBoard board = rng.InitBoard();
    EXPECT_EQ(board.Bits(), 0x210000000ULL);

    const auto firstSpawn = rng.SpawnNext(board);
    EXPECT_TRUE(firstSpawn);
    EXPECT_EQ(board.Bits(), 0x1000210000000ULL);

    const auto secondSpawn = rng.SpawnNext(board);
    EXPECT_TRUE(secondSpawn);
    EXPECT_EQ(board.Bits(), 0x1000211000000ULL);
}

TEST_CASE(TdlBestMove_TieBreaksUpRightDownLeft) {
    using game2048::ai::NtupleNetwork;
    using game2048::ai::NtuplePreset;
    using game2048::ai::PatternSetForPreset;

    NtupleNetwork network(PatternSetForPreset(NtuplePreset::Tdl8x6KMatsuzaki));
    game2048::FastBoard board;
    board.SetRank(5, 1);

    const auto candidate = game2048::ai::ChooseTdlBestMove(board, network);

    EXPECT_TRUE(candidate.valid);
    EXPECT_EQ(candidate.direction, game2048::Direction::Up);
}

TEST_CASE(ExpectimaxLeafNetwork_UsesAfterstateRewards_InRecursiveMaxNodes) {
    NtupleNetwork network(PatternSetForPreset(NtuplePreset::CompactD4));
    SearchConfig config;
    config.maxDepth = 1;
    config.timeBudgetMs = 0;
    config.iterativeDeepening = false;
    config.approximateChanceNodes = false;
    config.maxChanceBranchesPerValue = 16;
    config.useTranspositionTable = false;

    ExpectimaxAgent agent;
    agent.SetConfig(config);
    agent.SetLeafNetwork(network);
    agent.SetLeafPriorWeight(0.0);

    FastBoard board;
    board.SetRank(2, 1);
    board.SetRank(4, 1);

    const auto decision = agent.ChooseMove(board);

    EXPECT_TRUE(decision.valid);
    EXPECT_EQ(decision.direction, game2048::Direction::Right);
}

TEST_CASE(ExpectimaxLeafNetwork_TieBreaks_With_TDL_Move_Order) {
    NtupleNetwork network(PatternSetForPreset(NtuplePreset::CompactD4));
    SearchConfig config;
    config.maxDepth = 0;
    config.timeBudgetMs = 0;
    config.iterativeDeepening = false;
    config.useTranspositionTable = false;

    ExpectimaxAgent agent;
    agent.SetConfig(config);
    agent.SetLeafNetwork(network);
    agent.SetLeafPriorWeight(0.0);

    FastBoard board;
    board.SetRank(0, 1);
    board.SetRank(3, 1);

    const auto decision = agent.ChooseMove(board);

    EXPECT_TRUE(decision.valid);
    EXPECT_EQ(decision.direction, game2048::Direction::Right);
}

TEST_CASE(TdlForwardTraining_UsesOptimisticValueScale) {
    using game2048::ai::NtupleNetwork;
    using game2048::ai::NtuplePreset;
    using game2048::ai::PatternSetForPreset;

    NtupleNetwork network(PatternSetForPreset(NtuplePreset::Tdl8x6KMatsuzaki), 320000.0F);
    game2048::FastBoard board(0x210000000ULL);

    EXPECT_NEAR(network.Evaluate(board), 320000.0, 1.0);
}

TEST_CASE(TdlTypes_HeaderExposesSharedTrainingTypes) {
    game2048::ai::TdlForwardTrainingOptions options;
    options.games = 3;
    options.seed = 700000U;

    game2048::ai::TdlRandom rng(options.seed);
    const game2048::FastBoard board = rng.InitBoard();
    game2048::ai::TdlCandidateMove move;
    move.move = board.MoveLeft();
    move.valid = move.move.changed;

    EXPECT_EQ(options.games, std::size_t {3});
    EXPECT_TRUE(board.Bits() != 0);
}

TEST_CASE(TdlForwardTraining_FastPathMatchesGenericForFixedSeed) {
    using game2048::ai::NtupleNetwork;
    using game2048::ai::NtuplePreset;
    using game2048::ai::PatternSetForPreset;
    using game2048::ai::TdlForwardTrainingOptions;
    using game2048::ai::TdlRandom;
    using game2048::ai::TrainTdlForward;

    NtupleNetwork generic(PatternSetForPreset(NtuplePreset::Tdl8x6KMatsuzaki), 320000.0F);
    NtupleNetwork fast(PatternSetForPreset(NtuplePreset::Tdl8x6KMatsuzaki), 320000.0F);
    TdlForwardTrainingOptions genericOptions;
    genericOptions.games = 8;
    genericOptions.alpha = 0.1;
    genericOptions.maxMovesPerGame = 32;
    TdlForwardTrainingOptions fastOptions = genericOptions;
    fastOptions.fastPath = true;

    TdlRandom genericRng(700000U);
    TdlRandom fastRng(700000U);
    const auto genericStats = TrainTdlForward(generic, genericRng, genericOptions);
    const auto fastStats = TrainTdlForward(fast, fastRng, fastOptions);

    EXPECT_EQ(fastStats.games, genericStats.games);
    EXPECT_EQ(fastStats.moves, genericStats.moves);
    EXPECT_EQ(fastStats.updates, genericStats.updates);
    EXPECT_EQ(fastStats.totalScore, genericStats.totalScore);
    EXPECT_EQ(fastStats.maxTile, genericStats.maxTile);
    EXPECT_NEAR(fastStats.meanAbsTdError, genericStats.meanAbsTdError, 1e-9);
    EXPECT_NEAR(fastStats.rmsTdError, genericStats.rmsTdError, 1e-9);

    const std::array<game2048::FastBoard, 3> probes {
        game2048::FastBoard(0x210000000ULL),
        game2048::FastBoard(0x1000211000000ULL),
        game2048::FastBoard(0x123456789ABCDEF0ULL),
    };
    for (const auto& board : probes) {
        EXPECT_NEAR(fast.Evaluate(board), generic.Evaluate(board), 1e-6);
    }
}

TEST_CASE(TdlBestMove_NonCanonicalFallbackMatchesNetworkEvaluatorHelper) {
    using game2048::ai::ChooseTdlBestMove;
    using game2048::ai::NtupleNetwork;
    using game2048::ai::NtuplePattern;
    using game2048::ai::tdl_backend_detail::ChooseBestWithNetworkEvaluator;

    NtupleNetwork network({NtuplePattern{{0, 1, 2, 3}}}, 1000.0F);
    const FastBoard board(0x1000211000000ULL);

    const auto publicMove = ChooseTdlBestMove(board, network);
    const auto helperMove = ChooseBestWithNetworkEvaluator(board, network);

    EXPECT_EQ(publicMove.valid, helperMove.valid);
    EXPECT_EQ(publicMove.direction, helperMove.direction);
    EXPECT_EQ(publicMove.move.board, helperMove.move.board);
    EXPECT_EQ(publicMove.move.scoreDelta, helperMove.move.scoreDelta);
    EXPECT_NEAR(publicMove.value, helperMove.value, 1e-9);
}

TEST_CASE(TdlEvaluateBest_NonFixed6FallbackSmoke) {
    using game2048::ai::EvaluateTdlBest;
    using game2048::ai::NtupleNetwork;
    using game2048::ai::NtuplePattern;

    NtupleNetwork network({NtuplePattern{{0, 1, 2, 3}}}, 1000.0F);
    const auto stats = EvaluateTdlBest(network, 700000U, 3, 16);

    EXPECT_EQ(stats.games, std::size_t {3});
    EXPECT_TRUE(stats.moves > 0);
    EXPECT_TRUE(stats.totalScore > 0);
}

TEST_CASE(TdlForwardTraining_Fixed6BackendRunsAgainstNetworkBackendForFixedSeed) {
    using game2048::ai::NtupleNetwork;
    using game2048::ai::NtuplePreset;
    using game2048::ai::PatternSetForPreset;
    using game2048::ai::TdlForwardTrainingOptions;
    using game2048::ai::TdlRandom;
    using game2048::ai::TrainTdlForward;

    const auto fixedSet = PatternSetForPreset(NtuplePreset::Tdl4x6Khyeh);
    auto networkSet = fixedSet;
    networkSet.useFixedPath = false;
    NtupleNetwork fixedBackend(fixedSet, 1000.0F);
    NtupleNetwork networkBackend(networkSet, 1000.0F);

    TdlForwardTrainingOptions options;
    options.games = 8;
    options.alpha = 0.1;
    options.maxMovesPerGame = 32;

    TdlRandom fixedRng(700000U);
    TdlRandom networkRng(700000U);
    const auto fixedStats = TrainTdlForward(fixedBackend, fixedRng, options);
    const auto networkStats = TrainTdlForward(networkBackend, networkRng, options);

    EXPECT_EQ(fixedStats.games, networkStats.games);
    EXPECT_TRUE(fixedStats.moves > 0);
    EXPECT_TRUE(networkStats.moves > 0);
    EXPECT_TRUE(fixedStats.updates > 0);
    EXPECT_TRUE(networkStats.updates > 0);
    EXPECT_TRUE(fixedStats.totalScore > 0);
    EXPECT_TRUE(networkStats.totalScore > 0);

    const std::array<game2048::FastBoard, 3> probes {
        game2048::FastBoard(0x210000000ULL),
        game2048::FastBoard(0x1000211000000ULL),
        game2048::FastBoard(0x123456789ABCDEF0ULL),
    };
    for (const auto& board : probes) {
        EXPECT_TRUE(std::isfinite(fixedBackend.Evaluate(board)));
        EXPECT_TRUE(std::isfinite(networkBackend.Evaluate(board)));
    }
}

TEST_CASE(TdlForwardTraining_FallsBackForNonFixed6OrNonTd) {
    using game2048::ai::NtupleNetwork;
    using game2048::ai::NtuplePattern;
    using game2048::ai::TdlForwardTrainingOptions;
    using game2048::ai::TdlRandom;
    using game2048::ai::TrainTdlForward;

    NtupleNetwork nonFixed({NtuplePattern{{0, 1, 2, 3}}});
    TdlForwardTrainingOptions tdOptions;
    tdOptions.games = 2;
    tdOptions.alpha = 0.1;
    tdOptions.maxMovesPerGame = 8;
    TdlRandom nonFixedRng(700000U);
    const auto nonFixedStats = TrainTdlForward(nonFixed, nonFixedRng, tdOptions);

    EXPECT_EQ(nonFixedStats.games, std::size_t {2});
    EXPECT_TRUE(nonFixedStats.moves > 0);

    NtupleNetwork tcNetwork(PatternSetForPreset(NtuplePreset::Tdl4x6Khyeh));
    TdlForwardTrainingOptions tcOptions = tdOptions;
    tcOptions.learningMode = LearningMode::TC;
    TdlRandom tcRng(700000U);
    const auto tcStats = TrainTdlForward(tcNetwork, tcRng, tcOptions);

    EXPECT_EQ(tcStats.games, std::size_t {2});
    EXPECT_TRUE(tcStats.moves > 0);
    EXPECT_TRUE(tcNetwork.TouchedTcEntries() > 0);
}

TEST_CASE(TdlFixed6Backend_UpdateMatchesNetworkFastUpdate) {
    using game2048::ai::Fixed6TdBackend;
    using game2048::ai::NtupleNetwork;
    using game2048::ai::NtuplePreset;
    using game2048::ai::PatternSetForPreset;

    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{4, 4, 4, 4}, {4, 4, 4, 4}, {4, 4, 4, 4}, {4, 4, 4, 4}}}));
    NtupleNetwork network(PatternSetForPreset(NtuplePreset::Tdl4x6Khyeh), 1000.0F);
    NtupleNetwork backendNetwork(PatternSetForPreset(NtuplePreset::Tdl4x6Khyeh), 1000.0F);

    const auto networkStats = network.UpdateTowardFast(board, 125.0, 0.10, LearningMode::TD, false);
    Fixed6TdBackend backend(backendNetwork);
    const auto backendStats = backend.Update(board.Bits(), 125.0, 0.10);

    EXPECT_NEAR(backendStats.before, networkStats.before, 1e-9);
    EXPECT_NEAR(backendStats.error, networkStats.error, 1e-9);
    EXPECT_NEAR(backendNetwork.Evaluate(board), network.Evaluate(board), 1e-9);
}

TEST_CASE(Tdl8x6Backend_UpdateUsesCachedMoveEstimate) {
    using game2048::ai::NtupleNetwork;
    using game2048::ai::NtuplePreset;
    using game2048::ai::PatternSetForPreset;
    using game2048::ai::Tdl8x6TdBackend;

    const FastBoard board(0x1000211000000ULL);
    NtupleNetwork backendNetwork(PatternSetForPreset(NtuplePreset::Tdl8x6KMatsuzaki), 320000.0F);
    NtupleNetwork kernelNetwork(PatternSetForPreset(NtuplePreset::Tdl8x6KMatsuzaki), 320000.0F);

    Tdl8x6TdBackend backend(backendNetwork);
    const auto move = backend.ChooseBest(board);
    const auto backendStats = backend.Update(move, move.value + 125.0, 0.10);

    game2048::ai::Tdl8x6Kernel kernel(kernelNetwork.MutableFixed6SingleStageView(LearningMode::TD));
    const auto kernelMove = kernel.ChooseBest(board);
    const auto kernelStats = kernel.UpdateKnownValue(kernelMove.board, kernelMove.estimate,
                                                     kernelMove.value + 125.0, 0.10);

    EXPECT_NEAR(backendStats.before, kernelStats.before, 1e-9);
    EXPECT_NEAR(backendStats.error, kernelStats.error, 1e-9);
    EXPECT_NEAR(backendNetwork.Evaluate(FastBoard(move.board)),
                kernelNetwork.Evaluate(FastBoard(kernelMove.board)), 1e-6);
}

TEST_CASE(Tdl8x6Kernel_EvaluateMatchesNetworkForCanonicalFixed8x6) {
    using game2048::ai::NtupleNetwork;
    using game2048::ai::NtuplePreset;
    using game2048::ai::PatternSetForPreset;
    using game2048::ai::Tdl8x6Kernel;

    NtupleNetwork network(PatternSetForPreset(NtuplePreset::Tdl8x6KMatsuzaki), 320000.0F);
    const FastBoard trained(0x1000211000000ULL);
    network.UpdateToward(trained, 125000.0, 0.10, LearningMode::TD);

    Tdl8x6Kernel kernel(network.MutableFixed6SingleStageView(LearningMode::TD));
    const std::array<FastBoard, 3> probes {
        FastBoard(0x210000000ULL),
        FastBoard(0x1000211000000ULL),
        FastBoard(0x123456789ABCDEF0ULL),
    };

    for (const auto& board : probes) {
        EXPECT_NEAR(kernel.Evaluate(board.Bits()), network.Evaluate(board), 1e-6);
    }
}

TEST_CASE(Tdl8x6Kernel_ReadOnlyViewSupportsCanonicalEvaluation) {
    using game2048::ai::NtupleNetwork;
    using game2048::ai::NtuplePreset;
    using game2048::ai::PatternSetForPreset;
    using game2048::ai::Tdl8x6Kernel;

    NtupleNetwork network(PatternSetForPreset(NtuplePreset::Tdl8x6KMatsuzaki), 320000.0F);
    const auto view = network.Fixed6SingleStageView(LearningMode::TD);
    EXPECT_TRUE(Tdl8x6Kernel::Supports(view));

    const Tdl8x6Kernel kernel(view);
    const FastBoard board(0x1000211000000ULL);
    EXPECT_NEAR(kernel.Evaluate(board.Bits()), network.Evaluate(board), 1e-6);
}

TEST_CASE(TdlBestMove_UsesCanonicalKernelResult) {
    using game2048::ai::ChooseTdlBestMove;
    using game2048::ai::NtupleNetwork;
    using game2048::ai::NtuplePreset;
    using game2048::ai::PatternSetForPreset;
    using game2048::ai::Tdl8x6Kernel;

    NtupleNetwork network(PatternSetForPreset(NtuplePreset::Tdl8x6KMatsuzaki), 320000.0F);
    const FastBoard board(0x1000211000000ULL);
    const Tdl8x6Kernel kernel(network.Fixed6SingleStageView(LearningMode::TD));
    const auto kernelMove = kernel.ChooseBest(board);
    const auto publicMove = ChooseTdlBestMove(board, network);

    EXPECT_TRUE(publicMove.valid);
    EXPECT_EQ(publicMove.move.board, kernelMove.board);
    EXPECT_EQ(publicMove.move.scoreDelta, kernelMove.scoreDelta);
    EXPECT_NEAR(publicMove.value, kernelMove.value, 1e-6);
}

TEST_CASE(Tdl8x6Kernel_UpdateMatchesNetworkForCanonicalFixed8x6) {
    using game2048::ai::NtupleNetwork;
    using game2048::ai::NtuplePreset;
    using game2048::ai::PatternSetForPreset;
    using game2048::ai::Tdl8x6Kernel;

    const FastBoard board(0x1000211000000ULL);
    NtupleNetwork generic(PatternSetForPreset(NtuplePreset::Tdl8x6KMatsuzaki), 320000.0F);
    NtupleNetwork kernelNetwork(PatternSetForPreset(NtuplePreset::Tdl8x6KMatsuzaki), 320000.0F);

    const auto genericStats = generic.UpdateToward(board, 125000.0, 0.10, LearningMode::TD);
    Tdl8x6Kernel kernel(kernelNetwork.MutableFixed6SingleStageView(LearningMode::TD));
    const auto kernelStats = kernel.Update(board.Bits(), 125000.0, 0.10);

    EXPECT_NEAR(kernelStats.before, genericStats.before, 1e-6);
    EXPECT_NEAR(kernelStats.error, genericStats.error, 1e-6);
    EXPECT_NEAR(kernelNetwork.Evaluate(board), generic.Evaluate(board), 1e-6);
}

TEST_CASE(Tdl8x6Kernel_RejectsNonCanonicalOffsets) {
    using game2048::ai::NtupleNetwork;
    using game2048::ai::NtuplePreset;
    using game2048::ai::NtupleMutableFixed6View;
    using game2048::ai::PatternSetForPreset;
    using game2048::ai::Tdl8x6Kernel;

    NtupleNetwork network(PatternSetForPreset(NtuplePreset::Tdl8x6KMatsuzaki), 320000.0F);
    const auto view = network.MutableFixed6SingleStageView(LearningMode::TD);
    EXPECT_TRUE(Tdl8x6Kernel::Supports(view));
    std::array<std::size_t, 8> offsets {};
    for (std::size_t index = 0; index < offsets.size(); ++index) {
        offsets[index] = view.patternOffsets[index];
    }
    ++offsets[3];

    bool rejected = false;
    try {
        const NtupleMutableFixed6View badView {
            view.weights,
            offsets.data(),
            view.shifts,
            view.patternCount,
            view.valid,
        };
        EXPECT_FALSE(Tdl8x6Kernel::Supports(badView));
        Tdl8x6Kernel kernel(badView);
        (void)kernel;
    } catch (const std::invalid_argument&) {
        rejected = true;
    }

    EXPECT_TRUE(rejected);
}

TEST_CASE(NtupleNetwork_Multistage_Uses_Dense_StageCopies) {
    NtupleNetwork network({NtuplePattern{{0, 1, 2, 3}}}, 0.0F, {3});
    const std::size_t singleStageWeights = 1u << 16U;

    EXPECT_EQ(network.StageCount(), std::size_t {2});
    EXPECT_EQ(network.WeightCount(), singleStageWeights);

    network.PromoteStageFromPrevious(1);
    EXPECT_EQ(network.WeightCount(), singleStageWeights * 2U);

    const FastBoard high = FastBoard::FromReference(
        MakeBoard({{{8, 4, 2, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}));
    network.UpdateToward(high, 8.0, 0.1);
    EXPECT_EQ(network.WeightCount(), singleStageWeights * 2U);
}

TEST_CASE(NtupleNetwork_OptimisticInit_Applies_To_New_Weights) {
    NtupleNetwork network({NtuplePattern{{0, 1, 2, 3}}}, 7.0F);
    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{16, 8, 4, 2}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}));

    EXPECT_NEAR(network.Evaluate(board), 7.0, 1e-6);
}

TEST_CASE(NtupleNetwork_OptimisticInit_Is_Target_Value_Not_PerWeight) {
    NtupleNetwork network(PatternSetForPreset(NtuplePreset::Tdl8x6KMatsuzaki), 320000.0F);
    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{128, 64, 32, 16}, {8, 4, 2, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}));

    EXPECT_NEAR(network.Evaluate(board), 320000.0, 1e-3);
}

TEST_CASE(NtupleNetwork_Selects_Stage_By_MaxRank_And_Promotes_Previous_Stage) {
    NtupleNetwork network({NtuplePattern{{1, 2, 3, 4}}}, 0.0F, {3});
    const FastBoard low = FastBoard::FromReference(
        MakeBoard({{{0, 4, 2, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}));
    const FastBoard high = FastBoard::FromReference(
        MakeBoard({{{8, 4, 2, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}));

    EXPECT_EQ(network.StageFor(low), std::size_t {0});
    EXPECT_EQ(network.StageFor(high), std::size_t {1});

    network.UpdateToward(low, 10.0, 1.0);
    EXPECT_TRUE(network.Evaluate(low) > 0.0);
    network.PromoteStageFromPrevious(1);
    EXPECT_NEAR(network.Evaluate(high), network.Evaluate(low), 1e-6);
}

TEST_CASE(NtupleNetwork_PaperStageBoundary_SwitchesAt16384) {
    NtupleNetwork network(PatternSetForPreset(NtuplePreset::Tdl8x6KMatsuzaki), 0.0F, {14});
    const FastBoard eightK = FastBoard::FromReference(
        MakeBoard({{{8192, 1024, 128, 16}, {8, 4, 2, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}));
    const FastBoard sixteenK = FastBoard::FromReference(
        MakeBoard({{{16384, 1024, 128, 16}, {8, 4, 2, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}));

    EXPECT_EQ(network.StageFor(eightK), std::size_t {0});
    EXPECT_EQ(network.StageFor(sixteenK), std::size_t {1});
    network.UpdateToward(eightK, 64.0, 0.5);
    const double stage0FallbackValue = network.Evaluate(sixteenK);
    network.PromoteStageFromPrevious(1);
    EXPECT_NEAR(network.Evaluate(sixteenK), stage0FallbackValue, 1e-6);
}

TEST_CASE(NtupleNetwork_EnableStages_Preserves_WarmStarted_BaseWeights) {
    NtupleNetwork network({NtuplePattern{{1, 2, 3, 4}}});
    const FastBoard low = FastBoard::FromReference(
        MakeBoard({{{4, 2, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}));
    const FastBoard high = FastBoard::FromReference(
        MakeBoard({{{8, 2, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}));

    network.UpdateToward(low, 12.0, 0.5);
    const double before = network.Evaluate(low);
    network.EnableStages({3});

    EXPECT_EQ(network.StageCount(), std::size_t {2});
    EXPECT_NEAR(network.Evaluate(low), before, 1e-9);
    EXPECT_NEAR(network.Evaluate(high), before, 1e-9);
}

TEST_CASE(NtupleNetwork_SaveAndLoad_RoundTrips_Learned_Value) {
    const std::string path = "/tmp/game2048_ntuple_roundtrip.weights";
    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{32, 16, 8, 4}, {2, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}));
    NtupleNetwork network({NtuplePattern{{0, 1, 2, 3}}});
    network.UpdateToward(board, 64.0, 0.5);

    network.Save(path);
    const NtupleNetwork loaded = NtupleNetwork::Load(path);
    std::remove(path.c_str());

    EXPECT_EQ(loaded.Patterns().size(), network.Patterns().size());
    EXPECT_EQ(loaded.WeightCount(), network.WeightCount());
    EXPECT_NEAR(loaded.Evaluate(board), network.Evaluate(board), 1e-9);
}

TEST_CASE(NtupleNetwork_Save_Writes_SelfDescribingV5Chunks) {
    const std::string path = "/tmp/game2048_ntuple_v5_header.weights";
    NtupleNetwork network({NtuplePattern{{0, 1, 2, 3}}}, 0.0F, {3});
    network.SetProfileMetadata("preset=compact-d4");

    network.Save(path);
    std::ifstream in(path, std::ios::binary);
    std::array<char, 8> magic {};
    in.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    auto readU32 = [&] {
        std::uint32_t value = 0;
        in.read(reinterpret_cast<char*>(&value), sizeof(value));
        return value;
    };

    const std::uint32_t version = readU32();
    const std::uint32_t endian = readU32();
    const std::uint32_t headerBytes = readU32();
    const std::uint32_t flags = readU32();
    const std::uint32_t valueType = readU32();
    const std::uint32_t keyBits = readU32();
    std::array<char, 4> chunkId {};
    in.read(chunkId.data(), static_cast<std::streamsize>(chunkId.size()));
    std::remove(path.c_str());

    EXPECT_TRUE(std::equal(magic.begin(), magic.end(), std::array<char, 8> {'G', '2', '0', '4', '8', 'N', 'T', '5'}.begin()));
    EXPECT_EQ(version, std::uint32_t {5});
    EXPECT_EQ(endian, std::uint32_t {0x01020304U});
    EXPECT_EQ(headerBytes, std::uint32_t {32});
    EXPECT_EQ(flags, std::uint32_t {0});
    EXPECT_EQ(valueType, std::uint32_t {1});
    EXPECT_EQ(keyBits, std::uint32_t {4});
    EXPECT_TRUE(std::equal(chunkId.begin(), chunkId.end(), std::array<char, 4> {'M', 'E', 'T', 'A'}.begin()));
}

TEST_CASE(NtupleNetwork_Load_Rejects_LegacyV1Format) {
    const std::string path = "/tmp/game2048_ntuple_legacy_v1.weights";
    {
        std::ofstream out(path, std::ios::binary);
        const char magic[] = "G2048NT1";
        const std::uint64_t version = 1;
        const std::uint64_t patternCount = 1;
        const std::uint64_t entriesPerPattern = 1u << 16U;
        const std::array<std::int32_t, 4> cells {0, 1, 2, 3};
        const std::uint64_t weightCount = entriesPerPattern;
        const double weight = 0.0;
        out.write(magic, 8);
        out.write(reinterpret_cast<const char*>(&version), sizeof(version));
        out.write(reinterpret_cast<const char*>(&patternCount), sizeof(patternCount));
        out.write(reinterpret_cast<const char*>(&entriesPerPattern), sizeof(entriesPerPattern));
        out.write(reinterpret_cast<const char*>(cells.data()), static_cast<std::streamsize>(cells.size() * sizeof(std::int32_t)));
        out.write(reinterpret_cast<const char*>(&weightCount), sizeof(weightCount));
        for (std::uint64_t index = 0; index < weightCount; ++index) {
            out.write(reinterpret_cast<const char*>(&weight), sizeof(weight));
        }
    }

    bool threw = false;
    try {
        static_cast<void>(NtupleNetwork::Load(path));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    std::remove(path.c_str());

    EXPECT_TRUE(threw);
}

TEST_CASE(NtupleNetwork_Load_Rejects_TruncatedV5Payload) {
    const std::string source = "/tmp/game2048_ntuple_source_v5.weights";
    const std::string truncated = "/tmp/game2048_ntuple_truncated_v5.weights";
    NtupleNetwork network({NtuplePattern{{0, 1, 2, 3}}});
    network.Save(source);
    {
        std::ifstream in(source, std::ios::binary);
        std::ofstream out(truncated, std::ios::binary);
        std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        bytes.resize(bytes.size() - 5U);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }

    bool threw = false;
    try {
        static_cast<void>(NtupleNetwork::Load(truncated));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    std::remove(source.c_str());
    std::remove(truncated.c_str());

    EXPECT_TRUE(threw);
}

TEST_CASE(NtupleNetwork_SaveAndLoad_Preserves_Multistage_Overrides) {
    const std::string path = "/tmp/game2048_ntuple_multistage_roundtrip.weights";
    const FastBoard high = FastBoard::FromReference(
        MakeBoard({{{8, 4, 2, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}));
    NtupleNetwork network({NtuplePattern{{0, 1, 2, 3}}}, 1.0F, {3});

    network.UpdateToward(high, 20.0, 0.5);
    network.Save(path);
    const NtupleNetwork loaded = NtupleNetwork::Load(path);
    std::remove(path.c_str());

    EXPECT_EQ(loaded.StageCount(), network.StageCount());
    EXPECT_EQ(loaded.WeightCount(), network.WeightCount());
    EXPECT_NEAR(loaded.Evaluate(high), network.Evaluate(high), 1e-9);
}

TEST_CASE(NtupleNetwork_V5_Preserves_TC_Coherence_State) {
    const std::string path = "/tmp/game2048_ntuple_tc_roundtrip.weights";
    NtupleNetwork network({NtuplePattern{{0, 1, 2, 3}}});
    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{16, 8, 4, 2}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}));

    EXPECT_EQ(network.TouchedTcEntries(), std::size_t {0});
    network.UpdateToward(board, 100.0, 0.5, LearningMode::TC);
    const double afterFirst = network.Evaluate(board);
    network.Save(path);
    NtupleNetwork loaded = NtupleNetwork::Load(path);
    std::remove(path.c_str());

    EXPECT_NEAR(loaded.Evaluate(board), afterFirst, 1e-9);
    EXPECT_EQ(loaded.TouchedTcEntries(), std::size_t {1});
    loaded.UpdateToward(board, 100.0, 0.5, LearningMode::TC);
    EXPECT_EQ(network.TouchedTcEntries(), std::size_t {1});
    EXPECT_TRUE(loaded.Evaluate(board) > afterFirst);
}

TEST_CASE(NtupleNetwork_TD_Mode_Does_Not_Allocate_TC_State) {
    NtupleNetwork network({NtuplePattern{{0, 1, 2, 3}}});
    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{16, 8, 4, 2}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}));

    network.UpdateToward(board, 100.0, 0.5, LearningMode::TD);

    EXPECT_EQ(network.TouchedTcEntries(), std::size_t {0});
}

TEST_CASE(NtupleNetwork_FastUpdate_Matches_RegularUpdate) {
    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{128, 64, 32, 16}, {8, 4, 2, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}));
    NtupleNetwork regular(PatternSetForPreset(NtuplePreset::Tdl4x6Khyeh));
    NtupleNetwork fast(PatternSetForPreset(NtuplePreset::Tdl4x6Khyeh));

    const auto regularStats = regular.UpdateToward(board, 250.0, 0.25, LearningMode::TD);
    const auto fastStats = fast.UpdateTowardFast(board, 250.0, 0.25, LearningMode::TD);

    EXPECT_NEAR(fastStats.before, regularStats.before, 1e-9);
    EXPECT_NEAR(fastStats.after, fast.Evaluate(board), 1e-9);
    EXPECT_NEAR(fastStats.error, regularStats.error, 1e-9);
    EXPECT_NEAR(fast.Evaluate(board), regular.Evaluate(board), 1e-9);
}

TEST_CASE(NtupleNetwork_FastUpdate_Matches_RegularUpdate_With_DuplicateKeys) {
    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{4, 4, 4, 4}, {4, 4, 4, 4}, {4, 4, 4, 4}, {4, 4, 4, 4}}}));
    NtupleNetwork regular(PatternSetForPreset(NtuplePreset::Tdl4x6Khyeh));
    NtupleNetwork fast(PatternSetForPreset(NtuplePreset::Tdl4x6Khyeh));

    regular.UpdateToward(board, 125.0, 0.10, LearningMode::TD);
    const auto fastStats = fast.UpdateTowardFast(board, 125.0, 0.10, LearningMode::TD);

    EXPECT_NEAR(fast.Evaluate(board), regular.Evaluate(board), 1e-9);
    EXPECT_NEAR(fastStats.after, fast.Evaluate(board), 1e-9);
}

TEST_CASE(NtupleFixed6Kernel_DispatchMatchesScalar) {
    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{128, 64, 32, 16}, {8, 4, 2, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}));
    std::array<std::uint8_t, game2048::ai::ntuple_kernel::kFixed6Transforms *
                             game2048::ai::ntuple_kernel::kFixed6Cells> shifts {
        0, 16, 0, 16, 0, 12, 0, 12,
        4, 20, 4, 20, 16, 28, 20, 24,
        8, 24, 8, 24, 32, 44, 40, 36,
        12, 28, 16, 32, 48, 60, 60, 48,
        16, 32, 20, 36, 4, 8, 4, 8,
        20, 36, 24, 40, 20, 24, 24, 20,
    };
    std::array<std::size_t, 1> offsets {0};
    std::array<std::size_t, game2048::ai::ntuple_kernel::kFixed6Transforms> scalarKeys {};
    std::array<std::size_t, game2048::ai::ntuple_kernel::kFixed6Transforms> dispatchKeys {};

    const std::size_t scalarCount = game2048::ai::ntuple_kernel::CollectFixed6Keys(
        board.Bits(), offsets.data(), shifts.data(), offsets.size(), scalarKeys.data(),
        game2048::ai::ntuple_kernel::KernelKind::Scalar);
    const std::size_t dispatchCount = game2048::ai::ntuple_kernel::CollectFixed6Keys(
        board.Bits(), offsets.data(), shifts.data(), offsets.size(), dispatchKeys.data(),
        game2048::ai::ntuple_kernel::KernelKind::Auto);

    EXPECT_EQ(dispatchCount, scalarCount);
    for (std::size_t index = 0; index < scalarCount; ++index) {
        EXPECT_EQ(dispatchKeys[index], scalarKeys[index]);
    }

    std::vector<float> weights(1u << 24U, 0.0F);
    for (std::size_t index = 0; index < scalarCount; ++index) {
        weights[scalarKeys[index]] = static_cast<float>(index + 1U);
    }
    const double scalarValue = game2048::ai::ntuple_kernel::EvaluateFixed6(
        board.Bits(), weights.data(), offsets.data(), shifts.data(), offsets.size(),
        game2048::ai::ntuple_kernel::KernelKind::Scalar);
    const double dispatchValue = game2048::ai::ntuple_kernel::EvaluateFixed6(
        board.Bits(), weights.data(), offsets.data(), shifts.data(), offsets.size(),
        game2048::ai::ntuple_kernel::KernelKind::Auto);

    EXPECT_NEAR(dispatchValue, scalarValue, 1e-9);
}

TEST_CASE(PackedBoard_Constructors_Do_Not_Initialize_MoveTables) {
    EXPECT_FALSE(game2048::PackedBoardMoveTablesInitialized());
    PackedBoard board;
    board.SetRank(0, 2);
    EXPECT_FALSE(game2048::PackedBoardMoveTablesInitialized());

    static_cast<void>(board.MoveLeft());
    EXPECT_TRUE(game2048::PackedBoardMoveTablesInitialized());
}

TEST_CASE(PackedBoard80_Stores_And_Moves_Ranks_Above_Fifteen) {
    PackedBoard board;
    board.SetRank(0, 16);
    board.SetRank(1, 16);
    board.SetRank(2, 2);

    const auto moved = board.MoveLeft();

    EXPECT_TRUE(moved.changed);
    EXPECT_EQ(PackedBoard(moved.board).GetRank(0), 17);
    EXPECT_EQ(PackedBoard(moved.board).GetRank(1), 2);
    EXPECT_EQ(moved.scoreDelta, std::uint32_t {131072});
}

TEST_CASE(PackedBoard80_Does_Not_Merge_Beyond_MaxRank) {
    PackedBoard board;
    board.SetRank(0, 31);
    board.SetRank(1, 31);

    const auto moved = board.MoveLeft();

    EXPECT_FALSE(moved.changed);
    EXPECT_EQ(moved.board.GetRank(0), 31);
    EXPECT_EQ(moved.board.GetRank(1), 31);
    EXPECT_EQ(moved.scoreDelta, std::uint32_t {0});
}

TEST_CASE(FastBoard_D4CanonicalKey_Matches_All_Symmetries) {
    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{2, 4, 8, 16}, {32, 64, 128, 256}, {512, 1024, 2, 4}, {8, 16, 32, 64}}}));
    const std::uint64_t canonical = board.CanonicalKey();

    for (int transform = 0; transform < 8; ++transform) {
        EXPECT_EQ(board.TransformD4(transform).CanonicalKey(), canonical);
    }
}

TEST_CASE(TileDowngrading_Uses_Step_Transform_Without_Mutating_Source) {
    PackedBoard wide;
    wide.SetRank(0, 18);
    wide.SetRank(1, 16);
    wide.SetRank(2, 10);

    const PackedBoard downgraded = game2048::ai::DowngradeTilesForSearch(wide, 2, 13);

    EXPECT_EQ(wide.GetRank(0), 18);
    EXPECT_EQ(wide.GetRank(1), 16);
    EXPECT_EQ(downgraded.GetRank(0), 16);
    EXPECT_EQ(downgraded.GetRank(1), 14);
    EXPECT_EQ(downgraded.GetRank(2), 10);
}

TEST_CASE(PaperTileDowngrading_IsRootOnlyLargestMissingHalving) {
    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{32768, 16384, 8192, 4096},
                    {2048, 1024, 512, 128},
                    {64, 32, 16, 8},
                    {4, 2, 0, 0}}}));

    const auto downgraded = PaperTileDowngradeRoot(board);

    EXPECT_TRUE(downgraded.changed);
    EXPECT_EQ(downgraded.thresholdRank, 8);
    EXPECT_EQ(board.GetRank(0), 15);
    EXPECT_EQ(downgraded.board.GetRank(0), 14);
    EXPECT_EQ(downgraded.board.GetRank(1), 13);
    EXPECT_EQ(downgraded.board.GetRank(2), 12);
    EXPECT_EQ(downgraded.board.GetRank(6), 8);
}

TEST_CASE(PaperTileDowngrading_DoesNotTriggerBelow32768) {
    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{16384, 8192, 4096, 2048},
                    {1024, 512, 128, 64},
                    {32, 16, 8, 4},
                    {2, 0, 0, 0}}}));

    const auto downgraded = PaperTileDowngradeRoot(board);

    EXPECT_FALSE(downgraded.changed);
    EXPECT_EQ(downgraded.board.Bits(), board.Bits());
}

TEST_CASE(NtupleTrainer_Runs_SelfPlay_And_Updates_Network) {
    NtupleNetwork network({NtuplePattern{{0, 1, 2, 3}}});
    NtupleTrainingOptions options;
    options.games = 1;
    options.seed = 7;
    options.maxMovesPerGame = 6;
    options.alpha = 0.05;

    NtupleTrainer trainer(network);
    const auto stats = trainer.Train(options);

    EXPECT_EQ(stats.games, std::size_t {1});
    EXPECT_TRUE(stats.moves > 0);
    EXPECT_TRUE(stats.updates > 0);
    EXPECT_TRUE(stats.maxTile >= 2);
    EXPECT_TRUE(stats.totalScore > 0);
    EXPECT_TRUE(stats.meanAbsTdError > 0.0);
    EXPECT_TRUE(stats.rmsTdError >= stats.meanAbsTdError);
    EXPECT_EQ(stats.stageUpdates.size(), std::size_t {1});
    EXPECT_EQ(stats.stageUpdates[0], stats.updates);
}

TEST_CASE(NtupleTrainer_Can_Disable_ExpectedSpawnTarget) {
    NtupleTrainingOptions options;

    EXPECT_FALSE(options.useExpectedSpawnTarget);

    options.useExpectedSpawnTarget = true;
    EXPECT_TRUE(options.useExpectedSpawnTarget);
}

TEST_CASE(NtupleTrainer_BackwardTrace_Uses_NextReward_And_TerminalZeroTarget) {
    NtupleNetwork network({NtuplePattern{{0}}});
    FastBoard first;
    first.SetRank(0, 1);
    FastBoard second;
    second.SetRank(0, 2);

    const auto stats = ApplyBackwardAfterstateTrace(network, {
        NtupleTraceStep {first, 8},
        NtupleTraceStep {second, 16},
    }, 1.0, LearningMode::TD);

    EXPECT_EQ(stats.updates, std::size_t {2});
    EXPECT_NEAR(network.Evaluate(second), 0.0, 1e-9);
    EXPECT_NEAR(network.Evaluate(first), 16.0, 1e-9);
}

TEST_CASE(NtupleTrainer_ReplayStart_Trains_HighStage_From_Real_Buffer) {
    NtupleNetwork network(PatternSetForPreset(NtuplePreset::Tdl8x6KMatsuzaki), 0.0F, {14});
    FastBoard replay;
    replay.SetRank(0, 14);
    replay.SetRank(1, 13);
    replay.SetRank(2, 12);

    NtupleTrainer trainer(network);
    trainer.AddReplayStart(replay);

    NtupleTrainingOptions options;
    options.games = 1;
    options.seed = 19;
    options.maxMovesPerGame = 4;
    options.alpha = 0.01;
    options.finalAlpha = 0.01;
    options.updateOrder = NtupleUpdateOrder::Backward;
    options.replayStartRank = 14;
    const auto stats = trainer.Train(options);

    EXPECT_TRUE(stats.maxTile >= 16384);
    EXPECT_TRUE(stats.stageUpdates.size() > 1);
    EXPECT_TRUE(stats.stageUpdates[1] > 0);
    EXPECT_EQ(stats.replayStarts, std::size_t {1});
}

TEST_CASE(NtupleTrainingRates_Linearly_Anneal_Per_Game) {
    NtupleTrainingOptions options;
    options.games = 5;
    options.alpha = 0.10;
    options.finalAlpha = 0.02;
    options.explorationRate = 0.50;
    options.finalExplorationRate = 0.10;

    const auto first = NtupleTrainingRates(options, 0);
    const auto middle = NtupleTrainingRates(options, 2);
    const auto last = NtupleTrainingRates(options, 4);

    EXPECT_NEAR(first.alpha, 0.10, 1e-9);
    EXPECT_NEAR(first.explorationRate, 0.50, 1e-9);
    EXPECT_NEAR(middle.alpha, 0.06, 1e-9);
    EXPECT_NEAR(middle.explorationRate, 0.30, 1e-9);
    EXPECT_NEAR(last.alpha, 0.02, 1e-9);
    EXPECT_NEAR(last.explorationRate, 0.10, 1e-9);
}

TEST_CASE(BenchmarkSummary_Reports_Endgame_AchievementRates) {
    BenchmarkGameStats game8192;
    game8192.score = 1;
    game8192.maxTile = 8192;
    BenchmarkGameStats game32768;
    game32768.score = 2;
    game32768.maxTile = 32768;
    const auto summary = SummarizeBenchmark({game8192, game32768}, 0.0);

    EXPECT_NEAR(summary.achievementRates.at(8192), 1.0, 1e-9);
    EXPECT_NEAR(summary.achievementRates.at(16384), 0.5, 1e-9);
    EXPECT_NEAR(summary.achievementRates.at(32768), 0.5, 1e-9);
}

TEST_CASE(TrainingSelectionValue_Supports_AverageScore_And_TileRate) {
    BenchmarkGameStats small;
    small.score = 100;
    small.maxTile = 4096;
    BenchmarkGameStats large;
    large.score = 300;
    large.maxTile = 16384;
    const auto summary = SummarizeBenchmark({small, large}, 0.0);

    EXPECT_NEAR(TrainingSelectionValue(summary, SelectionMetric::AverageScore, 0), 200.0, 1e-9);
    EXPECT_NEAR(TrainingSelectionValue(summary, SelectionMetric::TileRate, 8192), 0.5, 1e-9);
}

TEST_CASE(NtupleAgent_Chooses_Move_With_Highest_Learned_Value) {
    NtupleNetwork network({NtuplePattern{{0, 1, 2, 3}}});
    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{0, 0, 2, 4}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}));
    const FastBoard afterLeft(board.MoveLeft().board);
    network.UpdateToward(afterLeft, 100.0, 1.0);

    NtupleAgent agent(network);
    const auto decision = agent.ChooseMove(board);

    EXPECT_TRUE(decision.valid);
    EXPECT_EQ(decision.direction, game2048::Direction::Left);
    EXPECT_TRUE(decision.value > 0.0);
}

TEST_CASE(NtupleAgent_Uses_Heuristic_Prior_When_Weights_Are_Empty) {
    NtupleNetwork network({NtuplePattern{{0, 1, 2, 3}}});
    NtupleAgent agent(network);
    agent.SetPriorWeight(0.25);
    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{1024, 512, 256, 128}, {64, 32, 16, 8}, {4, 2, 0, 0}, {0, 0, 0, 0}}}));

    Evaluator evaluator;
    double bestHeuristic = -std::numeric_limits<double>::infinity();
    game2048::Direction bestDirection = game2048::Direction::Left;
    for (game2048::Direction direction : std::array<game2048::Direction, 4> {
             game2048::Direction::Left,
             game2048::Direction::Up,
             game2048::Direction::Right,
             game2048::Direction::Down,
         }) {
        game2048::FastMoveResult move;
        switch (direction) {
            case game2048::Direction::Left:
                move = board.MoveLeft();
                break;
            case game2048::Direction::Right:
                move = board.MoveRight();
                break;
            case game2048::Direction::Up:
                move = board.MoveUp();
                break;
            case game2048::Direction::Down:
                move = board.MoveDown();
                break;
        }
        if (!move.changed) {
            continue;
        }
        const double heuristic = evaluator.Evaluate(FastBoard(move.board));
        if (heuristic > bestHeuristic) {
            bestHeuristic = heuristic;
            bestDirection = direction;
        }
    }

    const auto decision = agent.ChooseMove(board);

    EXPECT_TRUE(decision.valid);
    EXPECT_EQ(decision.direction, bestDirection);
}

TEST_CASE(Evaluator_Breakdown_Sums_To_Total) {
    Evaluator evaluator;
    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{128, 64, 32, 16}, {8, 4, 2, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}));

    const auto breakdown = evaluator.Breakdown(board);
    const double recomputed = breakdown.emptyTiles + breakdown.monotonicity + breakdown.smoothness +
                              breakdown.cornerMax + breakdown.mergePotential + breakdown.snakePattern +
                              breakdown.trapPenalty + breakdown.mobility + breakdown.danger +
                              breakdown.chainContinuity + breakdown.endgameProgress +
                              breakdown.patternTable;

    EXPECT_NEAR(breakdown.total, recomputed, 1e-6);
}

TEST_CASE(Evaluator_Rewards_Mobility_And_Penalizes_Danger) {
    Evaluator evaluator;
    const FastBoard flexible = FastBoard::FromReference(
        MakeBoard({{{4, 2, 4, 2}, {2, 4, 2, 4}, {4, 2, 0, 2}, {2, 4, 2, 4}}}));
    const FastBoard cramped = FastBoard::FromReference(
        MakeBoard({{{4, 2, 4, 2}, {2, 4, 2, 4}, {4, 2, 4, 2}, {2, 4, 2, 0}}}));

    const auto flexibleBreakdown = evaluator.Breakdown(flexible);
    const auto crampedBreakdown = evaluator.Breakdown(cramped);

    EXPECT_TRUE(flexibleBreakdown.mobility > crampedBreakdown.mobility);
    EXPECT_TRUE(flexibleBreakdown.danger > crampedBreakdown.danger);
    EXPECT_TRUE(evaluator.Evaluate(flexible) > evaluator.Evaluate(cramped));
}

TEST_CASE(Evaluator_PatternTable_Rewards_Local_Order_And_Merges) {
    game2048::ai::EvaluatorConfig config;
    config.usePatternTable = true;
    Evaluator evaluator(config);
    const FastBoard orderly = FastBoard::FromReference(
        MakeBoard({{{512, 256, 128, 64}, {32, 16, 8, 4}, {4, 4, 2, 2}, {0, 0, 0, 0}}}));
    const FastBoard noisy = FastBoard::FromReference(
        MakeBoard({{{512, 2, 128, 4}, {16, 256, 8, 64}, {4, 2, 4, 2}, {0, 0, 0, 0}}}));

    const auto orderlyBreakdown = evaluator.Breakdown(orderly);
    const auto noisyBreakdown = evaluator.Breakdown(noisy);

    EXPECT_TRUE(orderlyBreakdown.patternTable > noisyBreakdown.patternTable);
    EXPECT_TRUE(evaluator.Evaluate(orderly) > evaluator.Evaluate(noisy));
}

TEST_CASE(Evaluator_Rewards_Continuous_Endgame_Snake_Chain) {
    game2048::ai::EvaluatorConfig config;
    config.useChainContinuity = true;
    Evaluator evaluator(config);
    const FastBoard continuous = FastBoard::FromReference(
        MakeBoard({{{4096, 2048, 1024, 512}, {32, 64, 128, 256}, {16, 8, 4, 2}, {0, 0, 0, 0}}}));
    const FastBoard broken = FastBoard::FromReference(
        MakeBoard({{{4096, 2048, 32, 512}, {1024, 64, 128, 256}, {16, 8, 4, 2}, {0, 0, 0, 0}}}));

    const auto continuousBreakdown = evaluator.Breakdown(continuous);
    const auto brokenBreakdown = evaluator.Breakdown(broken);

    EXPECT_TRUE(continuousBreakdown.chainContinuity > brokenBreakdown.chainContinuity);
    EXPECT_TRUE(evaluator.Evaluate(continuous) > evaluator.Evaluate(broken));
}

TEST_CASE(Evaluator_Rewards_Endgame_Progress_Toward_Second_4096) {
    game2048::ai::EvaluatorConfig config;
    config.useEndgameProgress = true;
    Evaluator evaluator(config);
    const FastBoard ready = FastBoard::FromReference(
        MakeBoard({{{4096, 2048, 2048, 512}, {32, 64, 128, 256}, {16, 8, 4, 2}, {0, 0, 0, 0}}}));
    const FastBoard scattered = FastBoard::FromReference(
        MakeBoard({{{4096, 512, 32, 2048}, {1024, 64, 128, 256}, {16, 8, 4, 2}, {2048, 0, 0, 0}}}));

    const auto readyBreakdown = evaluator.Breakdown(ready);
    const auto scatteredBreakdown = evaluator.Breakdown(scattered);

    EXPECT_TRUE(readyBreakdown.endgameProgress > scatteredBreakdown.endgameProgress);
    EXPECT_TRUE(evaluator.Evaluate(ready) > evaluator.Evaluate(scattered));
}

TEST_CASE(SearchKey_DoesNotAlias_Depth_Into_BoardBits) {
    const FastBoard boardA = FastBoard::FromReference(
        MakeBoard({{{2, 4, 8, 16}, {32, 64, 128, 256}, {512, 1024, 2, 4}, {8, 16, 32, 64}}}));
    const FastBoard boardB(boardA.Bits() ^ (1ULL << 56U));

    EXPECT_TRUE(boardA.Bits() != boardB.Bits());
    EXPECT_EQ((boardA.Bits() ^ (1ULL << 56U)), boardB.Bits());
    EXPECT_TRUE(game2048::ai::ComposeSearchKey(boardA, 1, NodeType::Max) !=
                game2048::ai::ComposeSearchKey(boardB, 0, NodeType::Max));
}

TEST_CASE(SearchKey_Uses_Raw_Board_By_Default_For_NonInvariant_Evaluators) {
    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{2, 4, 8, 16}, {32, 64, 128, 256}, {512, 1024, 2, 4}, {8, 16, 32, 64}}}));
    const FastBoard mirrored = board.Mirror();

    EXPECT_EQ(board.CanonicalKey(), mirrored.CanonicalKey());
    EXPECT_TRUE(game2048::ai::ComposeSearchKey(board, 2, NodeType::Max) !=
                game2048::ai::ComposeSearchKey(mirrored, 2, NodeType::Max));
}

TEST_CASE(Expectimax_Fallback_Preserves_NonZero_Stats) {
    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 2, 0, 0}, {0, 0, 0, 2}}}));
    ExpectimaxAgent agent(Evaluator(), SearchConfig{8, 1, false, true, false, 0});

    const auto decision = agent.ChooseMove(board);

    EXPECT_TRUE(decision.valid);
    EXPECT_TRUE(decision.stats.nodes > 0);
    EXPECT_TRUE(decision.stats.elapsedMs >= 0.0);
}

TEST_CASE(Expectimax_Can_Use_Ntuple_Leaf_Evaluator) {
    NtupleNetwork network({NtuplePattern{{0, 1, 2, 3}}});
    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{0, 0, 2, 4}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}));
    const FastBoard afterLeft(board.MoveLeft().board);
    network.UpdateToward(afterLeft, 1000.0, 1.0);

    SearchConfig config;
    config.maxDepth = 0;
    config.timeBudgetMs = 0;
    config.iterativeDeepening = false;
    config.useTranspositionTable = false;

    ExpectimaxAgent agent(Evaluator(), config);
    agent.SetLeafNetwork(network);
    agent.SetLeafPriorWeight(0.0);
    const auto decision = agent.ChooseMove(board);

    EXPECT_TRUE(decision.valid);
    EXPECT_EQ(decision.direction, game2048::Direction::Left);
    EXPECT_TRUE(decision.value > 0.0);
}

TEST_CASE(Expectimax_DefaultSearch_UsesBoundedChanceExpansion) {
    SearchConfig config;

    EXPECT_TRUE(config.approximateChanceNodes);
    EXPECT_TRUE(config.maxChanceBranchesPerValue > 0);
    EXPECT_TRUE(config.endgameMoveSafetyPenalty > 0.0);
    EXPECT_TRUE(config.endgameDepthBonus > 0);
    EXPECT_TRUE(config.endgamePessimism > 0.0);
    EXPECT_TRUE(config.rootRolloutDepth > 0);
    EXPECT_TRUE(config.rootRolloutWeight > 0.0);
}

TEST_CASE(Expectimax_EndgameMoveSafety_Penalizes_Moving_Locked_Corner_Max) {
    SearchConfig config;
    config.useEndgameMoveSafety = true;
    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{4096, 1024, 512, 256}, {2048, 128, 64, 32}, {16, 8, 4, 2}, {0, 0, 0, 0}}}));
    const FastBoard safe(board.MoveLeft().board);
    const FastBoard unsafe(board.MoveDown().board);

    EXPECT_NEAR(EndgameMoveSafetyPenalty(board, safe, config), 0.0, 1e-6);
    EXPECT_TRUE(EndgameMoveSafetyPenalty(board, unsafe, config) < 0.0);
}

TEST_CASE(Expectimax_TileDowngrading_Caps_HighRanks_Without_Mutating_Input) {
    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{32768, 16384, 8192, 4096}, {2048, 1024, 512, 256}, {128, 64, 32, 16}, {8, 4, 2, 0}}}));

    const FastBoard downgraded = DowngradeTilesForSearch(board, 2, 13);

    EXPECT_EQ(board.GetRank(0), 15);
    EXPECT_EQ(board.GetRank(1), 14);
    EXPECT_EQ(downgraded.GetRank(0), 13);
    EXPECT_EQ(downgraded.GetRank(1), 13);
    EXPECT_EQ(downgraded.GetRank(2), 13);
    EXPECT_EQ(downgraded.GetRank(3), 12);
}

TEST_CASE(Expectimax_ApproximateChanceSelection_IncludesWorstEvaluatedSpawnCell) {
    const Evaluator evaluator;
    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{1024, 512, 256, 128}, {64, 32, 16, 8}, {4, 2, 0, 0}, {0, 0, 0, 0}}}));
    SearchConfig config;
    config.approximateChanceNodes = true;
    config.maxChanceBranchesPerValue = 2;

    const auto selected = SelectChanceCellsForSearch(board, evaluator, config);
    double worstValue = std::numeric_limits<double>::infinity();
    int worstIndex = -1;
    for (int index : board.EmptyIndices()) {
        double expectedValue = 0.0;
        for (const auto& rankAndProbability : std::array<std::pair<int, double>, 2>{{{1, 0.9}, {2, 0.1}}}) {
            FastBoard spawned = board;
            spawned.SetRank(index, rankAndProbability.first);
            expectedValue += rankAndProbability.second * evaluator.Evaluate(spawned);
        }
        if (expectedValue < worstValue) {
            worstValue = expectedValue;
            worstIndex = index;
        }
    }

    EXPECT_TRUE(std::find(selected.begin(), selected.end(), worstIndex) != selected.end());
}
