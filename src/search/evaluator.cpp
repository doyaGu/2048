#include "search/evaluator.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace game2048::ai {

namespace {

constexpr std::array<std::array<int, kCellCount>, 4> kSnakePaths {{
    {0, 1, 2, 3, 7, 6, 5, 4, 8, 9, 10, 11, 15, 14, 13, 12},
    {3, 2, 1, 0, 4, 5, 6, 7, 11, 10, 9, 8, 12, 13, 14, 15},
    {15, 11, 7, 3, 2, 6, 10, 14, 13, 9, 5, 1, 0, 4, 8, 12},
    {12, 8, 4, 0, 1, 5, 9, 13, 14, 10, 6, 2, 3, 7, 11, 15}
}};

struct PatternTables {
    std::array<double, 1u << 16> rowScore {};
    bool initialized = false;
};

std::uint16_t ReversePatternRow(std::uint16_t row) {
    return static_cast<std::uint16_t>(((row & 0x000FU) << 12U) |
                                      ((row & 0x00F0U) << 4U) |
                                      ((row & 0x0F00U) >> 4U) |
                                      ((row & 0xF000U) >> 12U));
}

double ScorePatternLine(const std::array<int, kBoardSize>& ranks) {
    int empty = 0;
    int maxRank = 0;
    double merge = 0.0;
    double smoothness = 0.0;
    double monotoneLeft = 0.0;
    double monotoneRight = 0.0;

    for (int rank : ranks) {
        if (rank == 0) {
            ++empty;
        }
        maxRank = std::max(maxRank, rank);
    }

    for (std::size_t index = 0; index + 1 < ranks.size(); ++index) {
        const int current = ranks[index];
        const int next = ranks[index + 1];
        if (current != 0 && current == next) {
            merge += static_cast<double>(1 << current);
        }
        if (current != 0 && next != 0) {
            smoothness -= static_cast<double>(std::abs(current - next));
        }
        if (current < next) {
            monotoneLeft -= static_cast<double>(next - current);
        }
        if (current > next) {
            monotoneRight -= static_cast<double>(current - next);
        }
    }

    const bool maxOnEdge = maxRank > 0 && (ranks.front() == maxRank || ranks.back() == maxRank);
    return static_cast<double>(empty) * 260.0 +
           merge * 3.0 +
           std::max(monotoneLeft, monotoneRight) * 180.0 +
           smoothness * 45.0 +
           (maxOnEdge ? static_cast<double>(maxRank * maxRank) * 25.0 : 0.0);
}

PatternTables& GetPatternTables() {
    static PatternTables tables;
    if (tables.initialized) {
        return tables;
    }

    for (std::uint32_t row = 0; row < tables.rowScore.size(); ++row) {
        const std::uint16_t rowBits = static_cast<std::uint16_t>(row);
        const std::array<int, kBoardSize> ranks {{
            static_cast<int>(rowBits & 0xFU),
            static_cast<int>((rowBits >> 4U) & 0xFU),
            static_cast<int>((rowBits >> 8U) & 0xFU),
            static_cast<int>((rowBits >> 12U) & 0xFU)
        }};
        const std::uint16_t reversed = ReversePatternRow(rowBits);
        const std::array<int, kBoardSize> reversedRanks {{
            static_cast<int>(reversed & 0xFU),
            static_cast<int>((reversed >> 4U) & 0xFU),
            static_cast<int>((reversed >> 8U) & 0xFU),
            static_cast<int>((reversed >> 12U) & 0xFU)
        }};
        tables.rowScore[row] = std::max(ScorePatternLine(ranks), ScorePatternLine(reversedRanks));
    }
    tables.initialized = true;
    return tables;
}

std::uint16_t PatternRow(const FastBoard& board, int row) {
    std::uint16_t bits = 0;
    for (int col = 0; col < kBoardSize; ++col) {
        const auto shift = static_cast<unsigned>(col * 4);
        bits |= static_cast<std::uint16_t>(board.GetRank(row * kBoardSize + col) << shift);
    }
    return bits;
}

double ComputePatternTable(const FastBoard& board) {
    const auto& tables = GetPatternTables();
    const FastBoard transposed = board.Transpose();
    double score = 0.0;
    for (int row = 0; row < kBoardSize; ++row) {
        score += tables.rowScore[PatternRow(board, row)];
        score += tables.rowScore[PatternRow(transposed, row)] * 0.85;
    }
    return score / 64.0;
}

double LinePenalty(const std::array<int, kBoardSize>& line, bool decreasing) {
    double penalty = 0.0;
    for (std::size_t index = 0; index + 1 < line.size(); ++index) {
        const int current = line[index];
        const int next = line[index + 1];
        if (decreasing) {
            if (current < next) {
                penalty += static_cast<double>(next - current);
            }
        } else if (current > next) {
            penalty += static_cast<double>(current - next);
        }
    }
    return penalty;
}

double ComputeMonotonicity(const FastBoard& board) {
    double penalty = 0.0;
    for (int row = 0; row < kBoardSize; ++row) {
        std::array<int, kBoardSize> line {};
        for (std::size_t col = 0; col < line.size(); ++col) {
            line[col] = board.GetRank(row * kBoardSize + static_cast<int>(col));
        }
        penalty += std::min(LinePenalty(line, true), LinePenalty(line, false));
    }
    for (int col = 0; col < kBoardSize; ++col) {
        std::array<int, kBoardSize> line {};
        for (std::size_t row = 0; row < line.size(); ++row) {
            line[row] = board.GetRank(static_cast<int>(row) * kBoardSize + col);
        }
        penalty += std::min(LinePenalty(line, true), LinePenalty(line, false));
    }
    return -penalty;
}

double ComputeSmoothness(const FastBoard& board) {
    double penalty = 0.0;
    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            const int index = row * kBoardSize + col;
            const int rank = board.GetRank(index);
            if (rank == 0) {
                continue;
            }
            if (col + 1 < kBoardSize) {
                const int right = board.GetRank(index + 1);
                if (right != 0) {
                    penalty += std::abs(rank - right);
                }
            }
            if (row + 1 < kBoardSize) {
                const int down = board.GetRank(index + kBoardSize);
                if (down != 0) {
                    penalty += std::abs(rank - down);
                }
            }
        }
    }
    return -penalty;
}

double ComputeCornerScore(const FastBoard& board) {
    const int maxRank = board.MaxRank();
    if (maxRank == 0) {
        return 0.0;
    }

    constexpr std::array<int, 4> kCorners {0, 3, 12, 15};
    bool inCorner = false;
    int bestDistance = 8;
    for (int index = 0; index < kCellCount; ++index) {
        if (board.GetRank(index) != maxRank) {
            continue;
        }
        const int row = index / kBoardSize;
        const int col = index % kBoardSize;
        for (int corner : kCorners) {
            const int cornerRow = corner / kBoardSize;
            const int cornerCol = corner % kBoardSize;
            const int distance = std::abs(row - cornerRow) + std::abs(col - cornerCol);
            bestDistance = std::min(bestDistance, distance);
            if (distance == 0) {
                inCorner = true;
            }
        }
    }

    if (inCorner) {
        return static_cast<double>(maxRank);
    }
    return -static_cast<double>(bestDistance * maxRank) * 0.5;
}

double ComputeMergePotential(const FastBoard& board) {
    double merges = 0.0;
    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            const int index = row * kBoardSize + col;
            const int rank = board.GetRank(index);
            if (rank == 0) {
                continue;
            }
            if (col + 1 < kBoardSize && rank == board.GetRank(index + 1)) {
                merges += static_cast<double>(1 << rank);
            }
            if (row + 1 < kBoardSize && rank == board.GetRank(index + kBoardSize)) {
                merges += static_cast<double>(1 << rank);
            }
        }
    }
    return merges / 64.0;
}

double ComputeSnakePattern(const FastBoard& board) {
    double best = 0.0;
    for (const auto& table : kSnakeWeightTables) {
        double score = 0.0;
        for (std::size_t index = 0; index < table.size(); ++index) {
            score += static_cast<double>(board.GetValue(static_cast<int>(index))) * table[index];
        }
        best = std::max(best, score);
    }
    return best / 65536.0;
}

double ComputeTrapPenalty(const FastBoard& board) {
    const int maxRank = board.MaxRank();
    double penalty = 0.0;
    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            const int index = row * kBoardSize + col;
            const int rank = board.GetRank(index);
            if (rank == 0) {
                continue;
            }
            const bool border = row == 0 || row == kBoardSize - 1 || col == 0 || col == kBoardSize - 1;
            if (!border && rank >= std::max(2, maxRank - 2)) {
                penalty += static_cast<double>(rank * rank);
            }
        }
    }
    return -penalty / 8.0;
}

int CountValidMoves(const FastBoard& board) {
    int moves = 0;
    moves += board.MoveUp().changed ? 1 : 0;
    moves += board.MoveLeft().changed ? 1 : 0;
    moves += board.MoveRight().changed ? 1 : 0;
    moves += board.MoveDown().changed ? 1 : 0;
    return moves;
}

double ComputeMobility(const FastBoard& board) {
    return static_cast<double>(CountValidMoves(board));
}

double ComputeDanger(const FastBoard& board) {
    const int empty = board.CountEmpty();
    const int validMoves = CountValidMoves(board);
    double penalty = 0.0;

    if (empty < 4) {
        const int deficit = 4 - empty;
        penalty += static_cast<double>(deficit * deficit);
    }
    if (validMoves < 3) {
        const int deficit = 3 - validMoves;
        penalty += static_cast<double>(deficit * deficit);
    }
    if (!board.CanMove()) {
        penalty += 16.0;
    }

    return -penalty;
}

double ComputeChainContinuity(const FastBoard& board) {
    const int maxRank = board.MaxRank();
    if (maxRank < 12) {
        return 0.0;
    }

    double best = 0.0;
    for (const auto& path : kSnakePaths) {
        if (board.GetRank(path[0]) != maxRank) {
            continue;
        }

        double score = static_cast<double>(maxRank * 2);
        int previousRank = board.GetRank(path[0]);
        for (std::size_t offset = 1; offset < path.size(); ++offset) {
            const int rank = board.GetRank(path[offset]);
            if (rank == 0) {
                break;
            }

            const int gap = previousRank - rank;
            if (gap < 0 || gap > 2) {
                break;
            }
            score += static_cast<double>(rank);
            previousRank = rank;
        }
        best = std::max(best, score);
    }
    return best;
}

double ComputeEndgameProgress(const FastBoard& board) {
    const int maxRank = board.MaxRank();
    if (maxRank < 12) {
        return 0.0;
    }

    double best = 0.0;
    for (const auto& path : kSnakePaths) {
        if (board.GetRank(path[0]) != maxRank) {
            continue;
        }

        double score = 0.0;
        for (std::size_t offset = 1; offset < path.size(); ++offset) {
            const int rank = board.GetRank(path[offset]);
            if (rank < maxRank - 4) {
                continue;
            }

            const double proximity = 1.0 / static_cast<double>(offset);
            const double rankWeight = static_cast<double>((rank - (maxRank - 5)) * (rank - (maxRank - 5)));
            score += rankWeight * proximity;
            if (rank == maxRank - 1) {
                score += 8.0 * proximity;
            }
        }
        best = std::max(best, score);
    }
    return best;
}

}  // namespace

Evaluator::Evaluator(EvaluatorConfig config)
    : config_(std::move(config)) {}

double Evaluator::Evaluate(const FastBoard& board) const {
    return Breakdown(board).total;
}

FeatureBreakdown Evaluator::Breakdown(const FastBoard& board) const {
    FeatureBreakdown breakdown;

    if (config_.useEmptyTiles) {
        breakdown.emptyTiles = config_.weights.emptyTiles * static_cast<double>(board.CountEmpty());
    }
    if (config_.useMonotonicity) {
        breakdown.monotonicity = config_.weights.monotonicity * ComputeMonotonicity(board);
    }
    if (config_.useSmoothness) {
        breakdown.smoothness = config_.weights.smoothness * ComputeSmoothness(board);
    }
    if (config_.useCornerMax) {
        breakdown.cornerMax = config_.weights.cornerMax * ComputeCornerScore(board);
    }
    if (config_.useMergePotential) {
        breakdown.mergePotential = config_.weights.mergePotential * ComputeMergePotential(board);
    }
    if (config_.useSnakePattern) {
        breakdown.snakePattern = config_.weights.snakePattern * ComputeSnakePattern(board);
    }
    if (config_.useTrapPenalty) {
        breakdown.trapPenalty = config_.weights.trapPenalty * ComputeTrapPenalty(board);
    }
    if (config_.useMobility) {
        breakdown.mobility = config_.weights.mobility * ComputeMobility(board);
    }
    if (config_.useDanger) {
        breakdown.danger = config_.weights.danger * ComputeDanger(board);
    }
    if (config_.useChainContinuity) {
        breakdown.chainContinuity = config_.weights.chainContinuity * ComputeChainContinuity(board);
    }
    if (config_.useEndgameProgress) {
        breakdown.endgameProgress = config_.weights.endgameProgress * ComputeEndgameProgress(board);
    }
    if (config_.usePatternTable) {
        breakdown.patternTable = config_.weights.patternTable * ComputePatternTable(board);
    }

    breakdown.total = breakdown.emptyTiles + breakdown.monotonicity + breakdown.smoothness +
                      breakdown.cornerMax + breakdown.mergePotential + breakdown.snakePattern +
                      breakdown.trapPenalty + breakdown.mobility + breakdown.danger +
                      breakdown.chainContinuity + breakdown.endgameProgress + breakdown.patternTable;
    return breakdown;
}

const EvaluatorConfig& Evaluator::Config() const {
    return config_;
}

}  // namespace game2048::ai
