#include "app/ai_advisor.h"

#include <array>

namespace game2048 {

namespace {

constexpr std::array<ai::AgentKind, 2> kAgentCycle {{
    ai::AgentKind::Greedy,
    ai::AgentKind::Expectimax,
}};

}  // namespace

AIAdvisor::AIAdvisor(ai::AgentKind agent, const ai::SearchConfig& searchConfig) {
    engine_.SetAgent(agent);
    engine_.Expectimax().SetConfig(searchConfig);
}

void AIAdvisor::SetAgent(ai::AgentKind kind) {
    if (engine_.GetAgent() == kind) {
        return;
    }
    engine_.SetAgent(kind);
    ResetRecommendation();
}

ai::AgentKind AIAdvisor::GetAgent() const {
    return engine_.GetAgent();
}

ai::AgentKind AIAdvisor::CycleAgent() {
    const ai::AgentKind current = engine_.GetAgent();
    for (std::size_t index = 0; index < kAgentCycle.size(); ++index) {
        if (kAgentCycle[index] == current) {
            const ai::AgentKind next = kAgentCycle[(index + 1) % kAgentCycle.size()];
            SetAgent(next);
            return next;
        }
    }

    SetAgent(kAgentCycle.front());
    return engine_.GetAgent();
}

void AIAdvisor::SetSearchConfig(const ai::SearchConfig& searchConfig) {
    engine_.Expectimax().SetConfig(searchConfig);
    ResetRecommendation();
}

void AIAdvisor::Invalidate() {
    recommendationDirty_ = true;
}

void AIAdvisor::ResetRecommendation() {
    recommendation_ = {};
    lastSearch_ = {};
    recommendationDirty_ = true;
}

ai::MoveDecision AIAdvisor::RequestMove(const Board& board) {
    recommendation_ = engine_.Recommend(ToFastBoard(board));
    lastSearch_ = recommendation_.stats;
    recommendationDirty_ = false;
    return recommendation_;
}

const ai::MoveDecision& AIAdvisor::EnsureRecommendation(const Board& board) {
    if (recommendationDirty_) {
        RequestMove(board);
    }
    return recommendation_;
}

const ai::MoveDecision& AIAdvisor::Recommendation() const {
    return recommendation_;
}

const SearchStats& AIAdvisor::LastSearch() const {
    return lastSearch_;
}

ai::FeatureBreakdown AIAdvisor::Breakdown(const Board& board) const {
    return engine_.Expectimax().GetEvaluator().Breakdown(ToFastBoard(board));
}

FastBoard AIAdvisor::ToFastBoard(const Board& board) {
    return FastBoard::FromReference(board);
}

}  // namespace game2048
