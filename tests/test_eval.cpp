#include "../src/ai/evaluator.h"
#include "../src/ai/expectimax.h"
#include "../src/ai/transposition_table.h"
#include "../src/core/board.h"
#include "../src/core/board_fast.h"
#include "test_framework.h"

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
                              breakdown.trapPenalty;

    EXPECT_NEAR(breakdown.total, recomputed, 1e-6);
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
