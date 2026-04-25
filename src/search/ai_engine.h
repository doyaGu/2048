#pragma once

#include "core/board_fast.h"
#include "search/expectimax.h"
#include "search/greedy.h"
#include "value/ntuple.h"

namespace game2048::ai {

enum class AgentKind {
    Human,
    Greedy,
    Expectimax,
    Ntuple
};

class AIEngine {
public:
    AIEngine();

    void SetAgent(AgentKind kind);
    AgentKind GetAgent() const;

    MoveDecision Recommend(const FastBoard& board);

    GreedyAgent& Greedy();
    ExpectimaxAgent& Expectimax();
    NtupleAgent& Ntuple();
    const GreedyAgent& Greedy() const;
    const ExpectimaxAgent& Expectimax() const;
    const NtupleAgent& Ntuple() const;

private:
    AgentKind agent_ = AgentKind::Expectimax;
    GreedyAgent greedy_;
    ExpectimaxAgent expectimax_;
    NtupleAgent ntuple_;
};

}  // namespace game2048::ai
