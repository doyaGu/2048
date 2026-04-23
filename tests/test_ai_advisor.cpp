#include <array>

#include "../src/app/ai_advisor.h"
#include "../src/core/board.h"
#include "test_framework.h"

namespace {

using game2048::AIAdvisor;
using game2048::Board;
using game2048::Direction;
using game2048::ai::AgentKind;
using game2048::ai::SearchConfig;

Board MakeBoard(const std::array<std::array<int, 4>, 4>& rows) {
    return Board::FromRows(rows);
}

Board SampleAdvisableBoard() {
    return MakeBoard({{{2, 2, 4, 8}, {16, 32, 64, 128}, {256, 0, 0, 0}, {0, 0, 0, 0}}});
}

}  // namespace

TEST_CASE(AIAdvisor_RequestMove_UpdatesCachedRecommendationAndSearchStats) {
    AIAdvisor advisor(AgentKind::Greedy);
    const Board board = SampleAdvisableBoard();

    const auto decision = advisor.RequestMove(board);

    EXPECT_TRUE(decision.valid);
    EXPECT_EQ(advisor.Recommendation().valid, decision.valid);
    EXPECT_EQ(advisor.Recommendation().direction, decision.direction);
    EXPECT_EQ(advisor.LastSearch().nodes, decision.stats.nodes);
    EXPECT_EQ(advisor.LastSearch().evaluation, decision.stats.evaluation);
}

TEST_CASE(AIAdvisor_ResettingAgentOrConfig_ClearsCachedRecommendation) {
    AIAdvisor advisor(AgentKind::Greedy);
    const Board board = SampleAdvisableBoard();
    advisor.RequestMove(board);

    EXPECT_TRUE(advisor.Recommendation().valid);

    advisor.SetAgent(AgentKind::Expectimax);
    EXPECT_EQ(advisor.GetAgent(), AgentKind::Expectimax);
    EXPECT_FALSE(advisor.Recommendation().valid);
    EXPECT_EQ(advisor.LastSearch().nodes, 0ULL);

    advisor.RequestMove(board);
    SearchConfig config {};
    config.maxDepth = 2;
    config.timeBudgetMs = 2;
    advisor.SetSearchConfig(config);

    EXPECT_FALSE(advisor.Recommendation().valid);
    EXPECT_EQ(advisor.LastSearch().maxDepthReached, 0);
}