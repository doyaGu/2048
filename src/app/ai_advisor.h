#pragma once

#include "ai/ai_engine.h"
#include "core/board.h"
#include "core/board_fast.h"
#include "shared/stats.h"

namespace game2048 {

class AIAdvisor {
public:
    explicit AIAdvisor(ai::AgentKind agent = ai::AgentKind::Expectimax,
                       const ai::SearchConfig& searchConfig = {});

    void SetAgent(ai::AgentKind kind);
    ai::AgentKind GetAgent() const;
    ai::AgentKind CycleAgent();
    void SetSearchConfig(const ai::SearchConfig& searchConfig);

    void Invalidate();
    void ResetRecommendation();

    ai::MoveDecision RequestMove(const Board& board);
    const ai::MoveDecision& EnsureRecommendation(const Board& board);
    const ai::MoveDecision& Recommendation() const;
    const SearchStats& LastSearch() const;
    ai::FeatureBreakdown Breakdown(const Board& board) const;

private:
    static FastBoard ToFastBoard(const Board& board);

    ai::AIEngine engine_;
    ai::MoveDecision recommendation_ {};
    SearchStats lastSearch_ {};
    bool recommendationDirty_ = true;
};

}  // namespace game2048
