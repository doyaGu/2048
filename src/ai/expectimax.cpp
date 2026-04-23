#include "ai/expectimax.h"

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

}  // namespace

std::uint64_t ComposeSearchKey(const FastBoard& board, int depth, NodeType nodeType) {
    std::uint64_t key = board.Key();
    key ^= 0x9E3779B97F4A7C15ULL + (key << 6U) + (key >> 2U);
    key ^= static_cast<std::uint64_t>(static_cast<std::uint32_t>(depth) * 0xBF58476D1CE4E5B9ULL);
    key ^= nodeType == NodeType::Chance ? 0x94D049BB133111EBULL : 0x369DEA0F31A53F85ULL;
    return key;
}

std::vector<int> SelectChanceCellsForSearch(const FastBoard& board, const Evaluator& evaluator,
                                            const SearchConfig& config) {
    auto empties = board.EmptyIndices();
    if (!config.approximateChanceNodes || config.maxChanceBranchesPerValue <= 0 ||
        static_cast<int>(empties.size()) <= config.maxChanceBranchesPerValue) {
        return empties;
    }

    struct RankedCell {
        int index = 0;
        double value = 0.0;
    };

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

    std::vector<RankedCell> ranked;
    ranked.reserve(empties.size());
    for (int index : empties) {
        ranked.push_back({index, spawnValue(index)});
    }

    std::sort(ranked.begin(), ranked.end(), [&board](const RankedCell& lhs, const RankedCell& rhs) {
        if (lhs.value == rhs.value) {
            return CellImpact(board, lhs.index) > CellImpact(board, rhs.index);
        }
        return lhs.value < rhs.value;
    });

    for (std::size_t index = 0; index < empties.size(); ++index) {
        empties[index] = ranked[index].index;
    }
    empties.resize(static_cast<std::size_t>(config.maxChanceBranchesPerValue));
    return empties;
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

    if (!board.CanMove()) {
        bestOverall.stats = aggregateStats;
        return bestOverall;
    }

    const int depthStart = config_.iterativeDeepening ? 1 : config_.maxDepth;
    const int depthEnd = config_.maxDepth;
    tt_.NextGeneration();

    for (int depth = depthStart; depth <= depthEnd; ++depth) {
        const auto deadline = started + std::chrono::milliseconds(config_.timeBudgetMs);
        SearchStats iterationStats;
        MoveDecision iterationBest;
        iterationBest.value = -std::numeric_limits<double>::infinity();

        std::vector<CandidateMove> candidates;
        candidates.reserve(4);
        for (Direction direction : kDirections) {
            const auto move = ApplyMove(board, direction);
            if (!move.changed) {
                continue;
            }
            candidates.push_back({direction, move,
                                  static_cast<double>(move.scoreDelta) +
                                      evaluator_.Evaluate(FastBoard(move.board))});
        }

        std::sort(candidates.begin(), candidates.end(), [](const CandidateMove& lhs, const CandidateMove& rhs) {
            return lhs.orderingScore > rhs.orderingScore;
        });

        aborted_ = false;
        for (const auto& candidate : candidates) {
            if (DeadlineReached(deadline)) {
                aborted_ = true;
                break;
            }

            const double value = SearchChance(FastBoard(candidate.move.board), depth, deadline, iterationStats);
            if (aborted_) {
                break;
            }

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

double ExpectimaxAgent::SearchMax(const FastBoard& board, int depth, Clock::time_point deadline, SearchStats& stats) {
    ++stats.nodes;
    if (DeadlineReached(deadline)) {
        aborted_ = true;
        return evaluator_.Evaluate(board);
    }

    if (depth <= 0 || !board.CanMove()) {
        ++stats.terminalNodes;
        return evaluator_.Evaluate(board);
    }

    if (config_.useTranspositionTable) {
        if (const auto* hit = tt_.Find(ComposeSearchKey(board, depth, NodeType::Max), static_cast<std::uint16_t>(depth), NodeType::Max)) {
            ++stats.cacheHits;
            return hit->value;
        }
    }

    double best = -std::numeric_limits<double>::infinity();
    Direction bestMove = Direction::Left;
    std::vector<CandidateMove> candidates;
    candidates.reserve(4);
    for (Direction direction : kDirections) {
        const auto move = ApplyMove(board, direction);
        if (!move.changed) {
            continue;
        }
        candidates.push_back({direction, move,
                              static_cast<double>(move.scoreDelta) +
                                  evaluator_.Evaluate(FastBoard(move.board))});
    }

    std::sort(candidates.begin(), candidates.end(), [](const CandidateMove& lhs, const CandidateMove& rhs) {
        return lhs.orderingScore > rhs.orderingScore;
    });

    if (candidates.empty()) {
        ++stats.terminalNodes;
        return evaluator_.Evaluate(board);
    }

    for (const auto& candidate : candidates) {
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
        tt_.Store(ComposeSearchKey(board, depth, NodeType::Max), static_cast<std::uint16_t>(depth), NodeType::Max, best, bestMove);
    }
    return best;
}

double ExpectimaxAgent::SearchChance(const FastBoard& board, int depth, Clock::time_point deadline, SearchStats& stats) {
    ++stats.nodes;
    ++stats.chanceNodes;
    if (DeadlineReached(deadline)) {
        aborted_ = true;
        return evaluator_.Evaluate(board);
    }

    const auto empties = SelectChanceCellsForSearch(board, evaluator_, config_);
    if (depth <= 0 || empties.empty()) {
        ++stats.terminalNodes;
        return evaluator_.Evaluate(board);
    }

    if (config_.useTranspositionTable) {
        if (const auto* hit = tt_.Find(ComposeSearchKey(board, depth, NodeType::Chance), static_cast<std::uint16_t>(depth), NodeType::Chance)) {
            ++stats.cacheHits;
            return hit->value;
        }
    }

    const double cellProbability = 1.0 / static_cast<double>(empties.size());
    double expected = 0.0;
    for (int index : empties) {
        for (const auto& [rank, probability] : std::array<std::pair<int, double>, 2>{{
                 {1, 0.9}, {2, 0.1}
             }}) {
            FastBoard spawned = board;
            spawned.SetRank(index, rank);
            expected += cellProbability * probability * SearchMax(spawned, depth - 1, deadline, stats);
            if (aborted_) {
                return expected;
            }
        }
    }

    if (config_.useTranspositionTable) {
        tt_.Store(ComposeSearchKey(board, depth, NodeType::Chance), static_cast<std::uint16_t>(depth), NodeType::Chance, expected, Direction::Left);
    }
    return expected;
}

bool ExpectimaxAgent::DeadlineReached(Clock::time_point deadline) const {
    return config_.timeBudgetMs > 0 && Clock::now() >= deadline;
}

}  // namespace game2048::ai
