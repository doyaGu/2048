#pragma once

#include "core/board_fast.h"
#include "ai/expectimax.h"
#include "ai/greedy.h"

namespace game2048::ai {

enum class AgentKind {
    Human,
    Greedy,
    Expectimax
};

class AIEngine {
public:
    AIEngine();

    void SetAgent(AgentKind kind);
    AgentKind GetAgent() const;

    MoveDecision Recommend(const FastBoard& board);

    GreedyAgent& Greedy();
    ExpectimaxAgent& Expectimax();
    const GreedyAgent& Greedy() const;
    const ExpectimaxAgent& Expectimax() const;

private:
    AgentKind agent_ = AgentKind::Expectimax;
    GreedyAgent greedy_;
    ExpectimaxAgent expectimax_;
};

}  // namespace game2048::ai
