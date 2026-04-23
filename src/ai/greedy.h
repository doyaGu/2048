#pragma once

#include "../board_fast.h"
#include "../stats.h"
#include "evaluator.h"

namespace game2048::ai {

struct MoveDecision {
    Direction direction = Direction::Left;
    bool valid = false;
    double value = 0.0;
    SearchStats stats {};
};

class GreedyAgent {
public:
    GreedyAgent();
    explicit GreedyAgent(Evaluator evaluator);
    MoveDecision ChooseMove(const FastBoard& board) const;

private:
    Evaluator evaluator_;
};

}  // namespace game2048::ai
