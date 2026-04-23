#include "../src/ai/evaluator.h"
#include "../src/ai/expectimax.h"
#include "../src/ai/transposition_table.h"
#include "../src/core/board.h"
#include "../src/core/board_fast.h"
#include "test_framework.h"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>
#include <vector>

namespace game2048::ai {
std::uint64_t ComposeSearchKey(const FastBoard& board, int depth, NodeType nodeType);
}

namespace {

using game2048::Board;
using game2048::FastBoard;
using game2048::ai::Evaluator;
using game2048::ai::ExpectimaxAgent;
using game2048::ai::NodeType;
using game2048::ai::SearchConfig;
using game2048::ai::SelectChanceCellsForSearch;

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

TEST_CASE(Evaluator_Breakdown_Sums_To_Total) {
    Evaluator evaluator;
    const FastBoard board = FastBoard::FromReference(
        MakeBoard({{{128, 64, 32, 16}, {8, 4, 2, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}}}));

    const auto breakdown = evaluator.Breakdown(board);
    const double recomputed = breakdown.emptyTiles + breakdown.monotonicity + breakdown.smoothness +
                              breakdown.cornerMax + breakdown.mergePotential + breakdown.snakePattern +
                              breakdown.trapPenalty + breakdown.mobility + breakdown.danger;

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

TEST_CASE(SearchKey_DoesNotAlias_Depth_Into_BoardBits) {
    const FastBoard boardA = FastBoard::FromReference(
        MakeBoard({{{2, 4, 8, 16}, {32, 64, 128, 256}, {512, 1024, 2, 4}, {8, 16, 32, 64}}}));
    const FastBoard boardB(boardA.Bits() ^ (1ULL << 56U));

    EXPECT_TRUE(boardA.Bits() != boardB.Bits());
    EXPECT_EQ((boardA.Bits() ^ (1ULL << 56U)), boardB.Bits());
    EXPECT_TRUE(game2048::ai::ComposeSearchKey(boardA, 1, NodeType::Max) !=
                game2048::ai::ComposeSearchKey(boardB, 0, NodeType::Max));
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

TEST_CASE(Expectimax_DefaultSearch_UsesBoundedChanceExpansion) {
    SearchConfig config;

    EXPECT_TRUE(config.approximateChanceNodes);
    EXPECT_TRUE(config.maxChanceBranchesPerValue > 0);
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
