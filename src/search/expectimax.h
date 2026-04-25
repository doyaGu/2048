#pragma once

#include <chrono>
#include <array>
#include <memory>
#include <vector>

#include "core/board_fast.h"
#include "shared/stats.h"
#include "search/evaluator.h"
#include "search/greedy.h"
#include "value/ntuple.h"
#include "search/transposition_table.h"

namespace game2048::ai {

std::uint64_t ComposeSearchKey(const FastBoard& board, int depth, NodeType nodeType,
                               bool canonicalizeD4 = false);

struct SearchConfig {
    int maxDepth = kDefaultExpectimaxDepth;
    int timeBudgetMs = kDefaultExpectimaxTimeBudgetMs;
    bool iterativeDeepening = true;
    bool useTranspositionTable = true;
    bool approximateChanceNodes = true;
    int maxChanceBranchesPerValue = 6;
    bool preserveChanceProbabilityMass = false;
    bool useEndgameMoveSafety = false;
    double endgameMoveSafetyPenalty = 50000.0;
    bool adaptiveEndgameSearch = false;
    int endgameMinRank = 12;
    int endgameDepthBonus = 1;
    int endgameMaxChanceBranchesPerValue = 4;
    double endgamePessimism = 0.08;
    bool useRootRollout = false;
    int rootRolloutDepth = 6;
    double rootRolloutWeight = 0.03;
    bool useTileDowngrading = false;
    bool useRootTileDowngrading = false;
    int tileDowngradeSteps = 1;
    int tileDowngradeFloorRank = 13;
    std::array<int, 17> chanceDepthLimitByEmpty {};
    bool canonicalizeTranspositionKeys = false;
};

std::vector<int> SelectChanceCellsForSearch(const FastBoard& board, const Evaluator& evaluator,
                                            const SearchConfig& config);
double EndgameMoveSafetyPenalty(const FastBoard& before, const FastBoard& after, const SearchConfig& config);
FastBoard DowngradeTilesForSearch(const FastBoard& board, int steps, int floorRank);
PackedBoard DowngradeTilesForSearch(const PackedBoard& board, int steps, int floorRank);
struct PaperTileDowngradeResult {
    FastBoard board {};
    bool changed = false;
    int thresholdRank = 0;
};
PaperTileDowngradeResult PaperTileDowngradeRoot(const FastBoard& board);

class ExpectimaxAgent {
public:
    ExpectimaxAgent();
    ExpectimaxAgent(Evaluator evaluator, SearchConfig config = {});

    MoveDecision ChooseMove(const FastBoard& board);
    const SearchConfig& Config() const;
    void SetConfig(const SearchConfig& config);
    Evaluator& GetEvaluator();
    const Evaluator& GetEvaluator() const;
    void SetLeafNetwork(NtupleNetwork network);
    void SetLeafNetworkShared(std::shared_ptr<const NtupleNetwork> network);
    void ClearLeafNetwork();
    void SetLeafPriorWeight(double weight);

private:
    using Clock = std::chrono::steady_clock;

    double LeafEvaluate(const FastBoard& board) const;
    const NtupleNetwork& LeafNetwork() const;
    bool CanUseCanonicalTranspositionKeys() const;
    double SearchMax(const FastBoard& board, int depth, Clock::time_point deadline, SearchStats& stats);
    double SearchChance(const FastBoard& board, int depth, Clock::time_point deadline, SearchStats& stats);
    bool DeadlineReached(Clock::time_point deadline) const;

    Evaluator evaluator_;
    NtupleNetwork leafNetwork_;
    std::shared_ptr<const NtupleNetwork> leafNetworkShared_;
    SearchConfig config_;
    TranspositionTable tt_;
    bool useLeafNetwork_ = false;
    double leafPriorWeight_ = 1.0;
    bool aborted_ = false;
};

}  // namespace game2048::ai
