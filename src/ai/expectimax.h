#pragma once

#include <chrono>
#include <vector>

#include "core/board_fast.h"
#include "shared/stats.h"
#include "ai/evaluator.h"
#include "ai/greedy.h"
#include "ai/transposition_table.h"

namespace game2048::ai {

std::uint64_t ComposeSearchKey(const FastBoard& board, int depth, NodeType nodeType);

struct SearchConfig {
    int maxDepth = kDefaultExpectimaxDepth;
    int timeBudgetMs = kDefaultExpectimaxTimeBudgetMs;
    bool iterativeDeepening = true;
    bool useTranspositionTable = true;
    bool approximateChanceNodes = true;
    int maxChanceBranchesPerValue = 6;
};

std::vector<int> SelectChanceCellsForSearch(const FastBoard& board, const Evaluator& evaluator,
                                            const SearchConfig& config);

class ExpectimaxAgent {
public:
    ExpectimaxAgent();
    ExpectimaxAgent(Evaluator evaluator, SearchConfig config = {});

    MoveDecision ChooseMove(const FastBoard& board);
    const SearchConfig& Config() const;
    void SetConfig(const SearchConfig& config);
    Evaluator& GetEvaluator();
    const Evaluator& GetEvaluator() const;

private:
    using Clock = std::chrono::steady_clock;

    double SearchMax(const FastBoard& board, int depth, Clock::time_point deadline, SearchStats& stats);
    double SearchChance(const FastBoard& board, int depth, Clock::time_point deadline, SearchStats& stats);
    bool DeadlineReached(Clock::time_point deadline) const;

    Evaluator evaluator_;
    SearchConfig config_;
    TranspositionTable tt_;
    bool aborted_ = false;
};

}  // namespace game2048::ai
