#include "search/expectimax.h"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>
#include <vector>

#include "shared/profiler.h"

namespace game2048::ai {

namespace {

struct CandidateMove {
    Direction direction = Direction::Left;
    FastMoveResult move {};
    double orderingScore = 0.0;
};

struct CandidateBuffer {
    std::array<CandidateMove, 4> items {};
    std::size_t count = 0;
};

struct ChanceCellBuffer {
    std::array<int, kCellCount> cells {};
    std::size_t count = 0;
    int originalEmptyCount = 0;
};

struct RankedCell {
    int index = 0;
    double value = 0.0;
    double impact = 0.0;
};

constexpr std::array<Direction, 4> kDirections {
    Direction::Up, Direction::Left, Direction::Right, Direction::Down
};

FastMoveResult ApplyMove(const FastBoard& board, Direction direction) {
    switch (direction) {
        case Direction::Left:
            return board.MoveLeft();
        case Direction::Right:
            return board.MoveRight();
        case Direction::Up:
            return board.MoveUp();
        case Direction::Down:
            return board.MoveDown();
    }
    return {};
}

double CellImpact(const FastBoard& board, int index) {
    const int row = index / kBoardSize;
    const int col = index % kBoardSize;
    double impact = 0.0;
    const std::array<int, 4> offsets {-kBoardSize, kBoardSize, -1, 1};
    for (int offset : offsets) {
        const int neighbor = index + offset;
        if ((offset == -1 && col == 0) || (offset == 1 && col == kBoardSize - 1) ||
            neighbor < 0 || neighbor >= kCellCount) {
            continue;
        }
        impact += static_cast<double>(board.GetRank(neighbor));
    }
    if (row == 0 || row == kBoardSize - 1 || col == 0 || col == kBoardSize - 1) {
        impact += 1.0;
    }
    return impact;
}

bool RankedCellComesBefore(const RankedCell& lhs, const RankedCell& rhs) {
    if (lhs.value == rhs.value) {
        return lhs.impact > rhs.impact;
    }
    return lhs.value < rhs.value;
}

void InsertRankedCell(std::array<RankedCell, kCellCount>& cells, std::size_t& count,
                      std::size_t limit, const RankedCell& cell) {
    std::size_t insertAt = count;
    while (insertAt > 0 && RankedCellComesBefore(cell, cells[insertAt - 1])) {
        if (insertAt < limit) {
            cells[insertAt] = cells[insertAt - 1];
        }
        --insertAt;
    }
    if (insertAt < limit) {
        cells[insertAt] = cell;
    }
    count = std::min(limit, count + 1);
}

void InsertCandidate(CandidateBuffer& candidates, const CandidateMove& candidate) {
    std::size_t insertAt = candidates.count;
    while (insertAt > 0 && candidate.orderingScore > candidates.items[insertAt - 1].orderingScore) {
        candidates.items[insertAt] = candidates.items[insertAt - 1];
        --insertAt;
    }
    candidates.items[insertAt] = candidate;
    ++candidates.count;
}

template <typename EvalFn>
CandidateBuffer OrderedMoveCandidates(const FastBoard& board, const EvalFn& evaluateAfterMove) {
    CandidateBuffer candidates;
    for (Direction direction : kDirections) {
        const auto move = ApplyMove(board, direction);
        if (!move.changed) {
            continue;
        }
        InsertCandidate(candidates, {direction, move,
                                     static_cast<double>(move.scoreDelta) +
                                         evaluateAfterMove(FastBoard(move.board))});
    }
    return candidates;
}

bool IsEndgameBoard(const FastBoard& board, const SearchConfig& config) {
    return config.adaptiveEndgameSearch && board.MaxRank() >= config.endgameMinRank;
}

SearchConfig EffectiveConfigForBoard(const FastBoard& board, const SearchConfig& config) {
    SearchConfig effective = config;
    if (IsEndgameBoard(board, config) && config.endgameMaxChanceBranchesPerValue > 0) {
        effective.maxChanceBranchesPerValue = std::min(config.maxChanceBranchesPerValue,
                                                       config.endgameMaxChanceBranchesPerValue);
    }
    return effective;
}

ChanceCellBuffer SelectChanceCellsBuffer(const FastBoard& board, const Evaluator& evaluator,
                                         const SearchConfig& config) {
    ChanceCellBuffer result;
    result.count = board.CollectEmptyIndices(result.cells);
    result.originalEmptyCount = static_cast<int>(result.count);
    if (!config.approximateChanceNodes || config.maxChanceBranchesPerValue <= 0 ||
        static_cast<int>(result.count) <= config.maxChanceBranchesPerValue) {
        return result;
    }

    auto spawnValue = [&board, &evaluator](int index) {
        double expectedValue = 0.0;
        for (const auto& [rank, probability] : std::array<std::pair<int, double>, 2>{{
                 {1, 0.9}, {2, 0.1}
             }}) {
            FastBoard spawned = board;
            spawned.SetRank(index, rank);
            expectedValue += probability * evaluator.Evaluate(spawned);
        }
        return expectedValue;
    };

    std::array<RankedCell, kCellCount> ranked {};
    std::size_t rankedCount = 0;
    const std::size_t limit = static_cast<std::size_t>(config.maxChanceBranchesPerValue);
    for (std::size_t offset = 0; offset < result.count; ++offset) {
        const int index = result.cells[offset];
        InsertRankedCell(ranked, rankedCount, limit, {index, spawnValue(index), CellImpact(board, index)});
    }

    for (std::size_t offset = 0; offset < rankedCount; ++offset) {
        result.cells[offset] = ranked[offset].index;
    }
    result.count = rankedCount;
    return result;
}

double RootRolloutScore(FastBoard board, const Evaluator& evaluator, const SearchConfig& config) {
    double score = 0.0;
    double decay = 1.0;
    for (int step = 0; step < config.rootRolloutDepth && board.CanMove(); ++step) {
        CandidateMove best;
        best.orderingScore = -std::numeric_limits<double>::infinity();
        const CandidateBuffer candidates = OrderedMoveCandidates(board, [&evaluator](const FastBoard& moved) {
            return evaluator.Evaluate(moved);
        });
        for (std::size_t index = 0; index < candidates.count; ++index) {
            if (candidates.items[index].orderingScore > best.orderingScore) {
                best = candidates.items[index];
            }
        }

        if (best.orderingScore == -std::numeric_limits<double>::infinity()) {
            break;
        }

        board = FastBoard(best.move.board);
        score += decay * (static_cast<double>(best.move.scoreDelta) + evaluator.Evaluate(board));

        SearchConfig spawnConfig = config;
        spawnConfig.approximateChanceNodes = true;
        spawnConfig.maxChanceBranchesPerValue = 1;
        spawnConfig.useEndgameMoveSafety = false;
        spawnConfig.adaptiveEndgameSearch = false;
        spawnConfig.useTileDowngrading = false;
        spawnConfig.useRootTileDowngrading = false;
        const auto cells = SelectChanceCellsForSearch(board, evaluator, spawnConfig);
        if (cells.empty()) {
            break;
        }

        FastBoard worst = board;
        double worstValue = std::numeric_limits<double>::infinity();
        for (int index : cells) {
            for (int rank : std::array<int, 2>{{1, 2}}) {
                FastBoard spawned = board;
                spawned.SetRank(index, rank);
                const double value = evaluator.Evaluate(spawned);
                if (value < worstValue) {
                    worstValue = value;
                    worst = spawned;
                }
            }
        }
        board = worst;
        decay *= 0.6;
    }
    return score;
}

}  // namespace

std::uint64_t ComposeSearchKey(const FastBoard& board, int depth, NodeType nodeType, bool canonicalizeD4) {
    std::uint64_t key = canonicalizeD4 ? board.CanonicalKey() : board.Key();
    key ^= 0x9E3779B97F4A7C15ULL + (key << 6U) + (key >> 2U);
    key ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(depth) * 0xBF58476D1CE4E5B9ULL);
    key ^= nodeType == NodeType::Chance ? 0x94D049BB133111EBULL : 0x369DEA0F31A53F85ULL;
    return key;
}

std::vector<int> SelectChanceCellsForSearch(const FastBoard& board, const Evaluator& evaluator,
                                            const SearchConfig& config) {
    const ChanceCellBuffer selected = SelectChanceCellsBuffer(board, evaluator, config);
    std::vector<int> cells;
    cells.reserve(selected.count);
    for (std::size_t offset = 0; offset < selected.count; ++offset) {
        cells.push_back(selected.cells[offset]);
    }
    return cells;
}

double EndgameMoveSafetyPenalty(const FastBoard& before, const FastBoard& after, const SearchConfig& config) {
    if (!config.useEndgameMoveSafety || before.MaxRank() < config.endgameMinRank) {
        return 0.0;
    }

    constexpr std::array<int, 4> kCorners {0, 3, 12, 15};
    const int maxRank = before.MaxRank();
    for (int corner : kCorners) {
        if (before.GetRank(corner) == maxRank) {
            return after.GetRank(corner) == maxRank ? 0.0 : -config.endgameMoveSafetyPenalty;
        }
    }
    return 0.0;
}

FastBoard DowngradeTilesForSearch(const FastBoard& board, int steps, int floorRank) {
    FastBoard downgraded = board;
    for (int index = 0; index < kCellCount; ++index) {
        const int rank = downgraded.GetRank(index);
        if (rank > floorRank) {
            downgraded.SetRank(index, std::max(floorRank, rank - std::max(steps, 0)));
        }
    }
    return downgraded;
}

PackedBoard DowngradeTilesForSearch(const PackedBoard& board, int steps, int floorRank) {
    PackedBoard downgraded = board;
    for (int index = 0; index < kCellCount; ++index) {
        const int rank = downgraded.GetRank(index);
        if (rank > floorRank) {
            downgraded.SetRank(index, std::max(floorRank, rank - std::max(steps, 0)));
        }
    }
    return downgraded;
}

PaperTileDowngradeResult PaperTileDowngradeRoot(const FastBoard& board) {
    PaperTileDowngradeResult result;
    result.board = board;
    const int maxRank = board.MaxRank();
    if (maxRank < 15) {
        return result;
    }

    std::array<bool, 32> present {};
    for (int index = 0; index < kCellCount; ++index) {
        const int rank = board.GetRank(index);
        if (rank >= 0 && rank < static_cast<int>(present.size())) {
            present[static_cast<std::size_t>(rank)] = true;
        }
    }

    int thresholdRank = 0;
    for (int rank = maxRank - 1; rank >= 1; --rank) {
        if (!present[static_cast<std::size_t>(rank)]) {
            thresholdRank = rank;
            break;
        }
    }
    if (thresholdRank <= 0) {
        return result;
    }

    result.thresholdRank = thresholdRank;
    for (int index = 0; index < kCellCount; ++index) {
        const int rank = result.board.GetRank(index);
        if (rank > thresholdRank) {
            result.board.SetRank(index, rank - 1);
            result.changed = true;
        }
    }
    return result;
}

ExpectimaxAgent::ExpectimaxAgent()
    : evaluator_(Evaluator()), config_(), tt_(kDefaultTranspositionTableEntries) {}

ExpectimaxAgent::ExpectimaxAgent(Evaluator evaluator, SearchConfig config)
    : evaluator_(std::move(evaluator)), config_(config), tt_(kDefaultTranspositionTableEntries) {}

MoveDecision ExpectimaxAgent::ChooseMove(const FastBoard& board) {
    MoveDecision bestOverall;
    bestOverall.value = -std::numeric_limits<double>::infinity();

    const auto started = Clock::now();
    SearchStats aggregateStats;

    FastBoard searchRoot = board;
    if (config_.useRootTileDowngrading) {
        searchRoot = PaperTileDowngradeRoot(board).board;
    }

    if (!searchRoot.CanMove()) {
        bestOverall.stats = aggregateStats;
        return bestOverall;
    }

    const int depthStart = config_.iterativeDeepening ? 1 : config_.maxDepth;
    const int depthEnd = config_.maxDepth + (IsEndgameBoard(searchRoot, config_) ? config_.endgameDepthBonus : 0);
    tt_.NextGeneration();

    for (int depth = depthStart; depth <= depthEnd; ++depth) {
        const auto deadline = started + std::chrono::milliseconds(config_.timeBudgetMs);
        SearchStats iterationStats;
        MoveDecision iterationBest;
        iterationBest.value = -std::numeric_limits<double>::infinity();

        const CandidateBuffer candidates = OrderedMoveCandidates(searchRoot, [this](const FastBoard& moved) {
            return LeafEvaluate(moved);
        });

        aborted_ = false;
        for (std::size_t candidateIndex = 0; candidateIndex < candidates.count; ++candidateIndex) {
            if (DeadlineReached(deadline)) {
                aborted_ = true;
                break;
            }

            const CandidateMove& candidate = candidates.items[candidateIndex];
            double value = SearchChance(FastBoard(candidate.move.board), depth, deadline, iterationStats);
            if (aborted_) {
                break;
            }
            if (config_.useRootRollout && config_.rootRolloutDepth > 0 && config_.rootRolloutWeight > 0.0) {
                value += config_.rootRolloutWeight *
                         RootRolloutScore(FastBoard(candidate.move.board), evaluator_, config_);
            }
            value += EndgameMoveSafetyPenalty(searchRoot, FastBoard(candidate.move.board), config_);

            if (!iterationBest.valid || value > iterationBest.value) {
                iterationBest.valid = true;
                iterationBest.direction = candidate.direction;
                iterationBest.value = value;
            }
        }

        if (!aborted_ && iterationBest.valid) {
            bestOverall = iterationBest;
            aggregateStats = iterationStats;
            aggregateStats.maxDepthReached = depth;
        } else {
            break;
        }

        if (!config_.iterativeDeepening) {
            break;
        }
    }

    if (!bestOverall.valid) {
        GreedyAgent fallback(evaluator_);
        bestOverall = fallback.ChooseMove(board);
        aggregateStats = bestOverall.stats;
    }

    bestOverall.stats = aggregateStats;
    bestOverall.stats.elapsedMs = std::chrono::duration<double, std::milli>(Clock::now() - started).count();
    bestOverall.stats.evaluation = bestOverall.value;
    return bestOverall;
}

const SearchConfig& ExpectimaxAgent::Config() const {
    return config_;
}

void ExpectimaxAgent::SetConfig(const SearchConfig& config) {
    config_ = config;
}

Evaluator& ExpectimaxAgent::GetEvaluator() {
    return evaluator_;
}

const Evaluator& ExpectimaxAgent::GetEvaluator() const {
    return evaluator_;
}

void ExpectimaxAgent::SetLeafNetwork(NtupleNetwork network) {
    leafNetwork_ = std::move(network);
    leafNetworkShared_.reset();
    useLeafNetwork_ = true;
}

void ExpectimaxAgent::SetLeafNetworkShared(std::shared_ptr<const NtupleNetwork> network) {
    if (!network) {
        ClearLeafNetwork();
        return;
    }
    leafNetworkShared_ = std::move(network);
    useLeafNetwork_ = true;
}

void ExpectimaxAgent::ClearLeafNetwork() {
    useLeafNetwork_ = false;
    leafNetworkShared_.reset();
}

void ExpectimaxAgent::SetLeafPriorWeight(double weight) {
    leafPriorWeight_ = weight;
}

double ExpectimaxAgent::LeafEvaluate(const FastBoard& board) const {
    if (!useLeafNetwork_) {
        return evaluator_.Evaluate(board);
    }
    const FastBoard leafBoard = config_.useTileDowngrading
        ? DowngradeTilesForSearch(board, config_.tileDowngradeSteps, config_.tileDowngradeFloorRank)
        : board;
    return LeafNetwork().Evaluate(leafBoard) + leafPriorWeight_ * evaluator_.Evaluate(leafBoard);
}

const NtupleNetwork& ExpectimaxAgent::LeafNetwork() const {
    return leafNetworkShared_ ? *leafNetworkShared_ : leafNetwork_;
}

bool ExpectimaxAgent::CanUseCanonicalTranspositionKeys() const {
    return config_.canonicalizeTranspositionKeys &&
           useLeafNetwork_ &&
           leafPriorWeight_ == 0.0 &&
           LeafNetwork().PatternSet().useD4;
}

double ExpectimaxAgent::SearchMax(const FastBoard& board, int depth, Clock::time_point deadline, SearchStats& stats) {
    ++stats.nodes;
    if (DeadlineReached(deadline)) {
        aborted_ = true;
        return LeafEvaluate(board);
    }

    if (depth <= 0 || !board.CanMove()) {
        ++stats.terminalNodes;
        ++stats.leafEvals;
        return LeafEvaluate(board);
    }

    if (config_.useTranspositionTable) {
        const bool canonicalize = CanUseCanonicalTranspositionKeys();
        if (const auto* hit = tt_.Find(ComposeSearchKey(board, depth, NodeType::Max, canonicalize),
                                      static_cast<std::uint16_t>(depth), NodeType::Max)) {
            ++stats.cacheHits;
            return hit->value;
        }
    }

    double best = -std::numeric_limits<double>::infinity();
    Direction bestMove = Direction::Left;
    const CandidateBuffer candidates = OrderedMoveCandidates(board, [this](const FastBoard& moved) {
        return LeafEvaluate(moved);
    });

    if (candidates.count == 0) {
        ++stats.terminalNodes;
        ++stats.leafEvals;
        return LeafEvaluate(board);
    }

    for (std::size_t candidateIndex = 0; candidateIndex < candidates.count; ++candidateIndex) {
        const CandidateMove& candidate = candidates.items[candidateIndex];
        const double value = SearchChance(FastBoard(candidate.move.board), depth, deadline, stats);
        if (aborted_) {
            return best;
        }
        if (value > best) {
            best = value;
            bestMove = candidate.direction;
        }
    }

    if (config_.useTranspositionTable) {
        tt_.Store(ComposeSearchKey(board, depth, NodeType::Max, CanUseCanonicalTranspositionKeys()),
                  static_cast<std::uint16_t>(depth), NodeType::Max, best, bestMove);
    }
    return best;
}

double ExpectimaxAgent::SearchChance(const FastBoard& board, int depth, Clock::time_point deadline, SearchStats& stats) {
    ++stats.nodes;
    ++stats.chanceNodes;
    if (DeadlineReached(deadline)) {
        aborted_ = true;
        return LeafEvaluate(board);
    }

    const SearchConfig effectiveConfig = EffectiveConfigForBoard(board, config_);
    const ChanceCellBuffer empties = SelectChanceCellsBuffer(board, evaluator_, effectiveConfig);
    if (empties.count > 0 && static_cast<std::size_t>(empties.originalEmptyCount) < config_.chanceDepthLimitByEmpty.size()) {
        const int capped = config_.chanceDepthLimitByEmpty[static_cast<std::size_t>(empties.originalEmptyCount)];
        if (capped > 0) {
            depth = std::min(depth, capped);
        }
    }
    if (depth <= 0 || empties.count == 0) {
        ++stats.terminalNodes;
        ++stats.leafEvals;
        return LeafEvaluate(board);
    }

    if (config_.useTranspositionTable) {
        const bool canonicalize = CanUseCanonicalTranspositionKeys();
        if (const auto* hit = tt_.Find(ComposeSearchKey(board, depth, NodeType::Chance, canonicalize),
                                      static_cast<std::uint16_t>(depth), NodeType::Chance)) {
            ++stats.cacheHits;
            return hit->value;
        }
    }

    const double cellProbability = 1.0 / static_cast<double>(
        config_.preserveChanceProbabilityMass ? empties.originalEmptyCount : static_cast<int>(empties.count));
    double expected = 0.0;
    double worst = std::numeric_limits<double>::infinity();
    for (std::size_t offset = 0; offset < empties.count; ++offset) {
        const int index = empties.cells[offset];
        for (const auto& [rank, probability] : std::array<std::pair<int, double>, 2>{{
                 {1, 0.9}, {2, 0.1}
             }}) {
            FastBoard spawned = board;
            spawned.SetRank(index, rank);
            const double value = SearchMax(spawned, depth - 1, deadline, stats);
            expected += cellProbability * probability * value;
            worst = std::min(worst, value);
            if (aborted_) {
                return expected;
            }
        }
    }

    if (config_.preserveChanceProbabilityMass && empties.originalEmptyCount > static_cast<int>(empties.count)) {
        const double missingCellProbability =
            static_cast<double>(empties.originalEmptyCount - static_cast<int>(empties.count)) /
            static_cast<double>(empties.originalEmptyCount);
        expected += missingCellProbability * LeafEvaluate(board);
    }

    if (IsEndgameBoard(board, config_) && worst < expected) {
        expected -= config_.endgamePessimism * (expected - worst);
    }

    if (config_.useTranspositionTable) {
        tt_.Store(ComposeSearchKey(board, depth, NodeType::Chance, CanUseCanonicalTranspositionKeys()),
                  static_cast<std::uint16_t>(depth), NodeType::Chance, expected, Direction::Left);
    }
    return expected;
}

bool ExpectimaxAgent::DeadlineReached(Clock::time_point deadline) const {
    return config_.timeBudgetMs > 0 && Clock::now() >= deadline;
}

}  // namespace game2048::ai
