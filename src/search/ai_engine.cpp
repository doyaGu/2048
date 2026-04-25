#include "search/ai_engine.h"

namespace game2048::ai {

AIEngine::AIEngine() = default;

void AIEngine::SetAgent(AgentKind kind) {
    agent_ = kind;
}

AgentKind AIEngine::GetAgent() const {
    return agent_;
}

MoveDecision AIEngine::Recommend(const FastBoard& board) {
    switch (agent_) {
        case AgentKind::Human:
        case AgentKind::Greedy:
            return greedy_.ChooseMove(board);
        case AgentKind::Expectimax:
            return expectimax_.ChooseMove(board);
        case AgentKind::Ntuple:
            return ntuple_.ChooseMove(board);
    }
    return {};
}

GreedyAgent& AIEngine::Greedy() {
    return greedy_;
}

ExpectimaxAgent& AIEngine::Expectimax() {
    return expectimax_;
}

NtupleAgent& AIEngine::Ntuple() {
    return ntuple_;
}

const GreedyAgent& AIEngine::Greedy() const {
    return greedy_;
}

const ExpectimaxAgent& AIEngine::Expectimax() const {
    return expectimax_;
}

const NtupleAgent& AIEngine::Ntuple() const {
    return ntuple_;
}

}  // namespace game2048::ai
