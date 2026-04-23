#include "ai/evaluator.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace game2048::ai {

namespace {

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

    breakdown.total = breakdown.emptyTiles + breakdown.monotonicity + breakdown.smoothness +
                      breakdown.cornerMax + breakdown.mergePotential + breakdown.snakePattern +
                      breakdown.trapPenalty;
    return breakdown;
}

const EvaluatorConfig& Evaluator::Config() const {
    return config_;
}

}  // namespace game2048::ai
