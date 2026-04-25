#pragma once

#include "core/board_fast.h"
#include "core/config.h"

namespace game2048::ai {

struct FeatureBreakdown {
    double emptyTiles = 0.0;
    double monotonicity = 0.0;
    double smoothness = 0.0;
    double cornerMax = 0.0;
    double mergePotential = 0.0;
    double snakePattern = 0.0;
    double trapPenalty = 0.0;
    double mobility = 0.0;
    double danger = 0.0;
    double chainContinuity = 0.0;
    double endgameProgress = 0.0;
    double patternTable = 0.0;
    double total = 0.0;
};

struct EvaluatorConfig {
    EvaluatorWeights weights = kDefaultEvaluatorWeights;
    bool useEmptyTiles = true;
    bool useMonotonicity = true;
    bool useSmoothness = true;
    bool useCornerMax = true;
    bool useMergePotential = true;
    bool useSnakePattern = true;
    bool useTrapPenalty = true;
    bool useMobility = true;
    bool useDanger = true;
    bool useChainContinuity = false;
    bool useEndgameProgress = false;
    bool usePatternTable = false;
};

class Evaluator {
public:
    explicit Evaluator(EvaluatorConfig config = {});

    double Evaluate(const FastBoard& board) const;
    FeatureBreakdown Breakdown(const FastBoard& board) const;
    const EvaluatorConfig& Config() const;

private:
    EvaluatorConfig config_;
};

}  // namespace game2048::ai
