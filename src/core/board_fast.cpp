#include "core/board_fast.h"

#include <algorithm>
#include <array>
#include <sstream>

namespace game2048 {

namespace {

struct RowTables {
    std::array<std::uint16_t, 1u << 16> leftResult {};
    std::array<std::uint16_t, 1u << 16> rightResult {};
    std::array<std::uint32_t, 1u << 16> leftScore {};
    std::array<std::uint32_t, 1u << 16> rightScore {};
    bool initialized = false;
};

RowTables& Tables() {
    static RowTables tables;
    return tables;
}

int ValueToRank(int value) {
    int rank = 0;
    while (value > 1) {
        value >>= 1;
        ++rank;
    }
    return rank;
}

std::uint16_t ReverseRow(std::uint16_t row) {
    return static_cast<std::uint16_t>(((row & 0x000FU) << 12U) |
                                      ((row & 0x00F0U) << 4U) |
                                      ((row & 0x0F00U) >> 4U) |
                                      ((row & 0xF000U) >> 12U));
}

void EnsureTables() {
    auto& tables = Tables();
    if (tables.initialized) {
        return;
    }

    for (std::uint32_t row = 0; row < (1u << 16); ++row) {
        std::array<int, 4> ranks {
            static_cast<int>(row & 0xFU),
            static_cast<int>((row >> 4U) & 0xFU),
            static_cast<int>((row >> 8U) & 0xFU),
            static_cast<int>((row >> 12U) & 0xFU)
        };

        std::array<int, 4> compacted {};
        std::size_t count = 0;
        for (int rank : ranks) {
            if (rank != 0) {
                compacted[count] = rank;
                ++count;
            }
        }

        std::array<int, 4> output {};
        std::uint32_t score = 0;
        std::size_t write = 0;
        for (std::size_t index = 0; index < count;) {
            if (index + 1 < count && compacted[index] == compacted[index + 1]) {
                const int mergedRank = compacted[index] + 1;
                output[write] = mergedRank;
                ++write;
                score += static_cast<std::uint32_t>(1U << mergedRank);
                index += 2;
            } else {
                output[write] = compacted[index];
                ++write;
                ++index;
            }
        }

        std::uint16_t leftRow = 0;
        for (std::size_t index = 0; index < output.size(); ++index) {
            leftRow |= static_cast<std::uint16_t>(output[index] << (index * 4U));
        }

        tables.leftResult[row] = leftRow;
        tables.leftScore[row] = score;
    }

    for (std::uint32_t row = 0; row < (1u << 16); ++row) {
        const std::uint16_t reversed = ReverseRow(static_cast<std::uint16_t>(row));
        tables.rightResult[row] = ReverseRow(tables.leftResult[reversed]);
        tables.rightScore[row] = tables.leftScore[reversed];
    }

    tables.initialized = true;
}

std::uint16_t ExtractRow(std::uint64_t bits, int row) {
    return static_cast<std::uint16_t>((bits >> (row * 16)) & 0xFFFFULL);
}

std::uint64_t SetRow(std::uint64_t bits, int row, std::uint16_t value) {
    const std::uint64_t mask = 0xFFFFULL << (row * 16);
    bits &= ~mask;
    bits |= static_cast<std::uint64_t>(value) << (row * 16);
    return bits;
}

}  // namespace

FastBoard::FastBoard() = default;

FastBoard::FastBoard(std::uint64_t bits)
    : bits_(bits) {
    EnsureTables();
}

FastBoard FastBoard::FromReference(const Board& board) {
    EnsureTables();
    std::uint64_t bits = 0;
    std::size_t index = 0;
    for (int value : board.Cells()) {
        const int rank = value == 0 ? 0 : ValueToRank(value);
        bits |= static_cast<std::uint64_t>(rank & 0xF) << (index * 4U);
        ++index;
    }
    return FastBoard(bits);
}

std::uint64_t FastBoard::Bits() const {
    return bits_;
}

int FastBoard::GetRank(int index) const {
    return static_cast<int>((bits_ >> (index * 4)) & 0xFULL);
}

int FastBoard::GetValue(int index) const {
    const int rank = GetRank(index);
    return rank == 0 ? 0 : (1 << rank);
}

bool FastBoard::IsEmpty(int index) const {
    return GetRank(index) == 0;
}

void FastBoard::SetRank(int index, int rank) {
    const std::uint64_t shift = static_cast<std::uint64_t>(index * 4);
    bits_ &= ~(0xFULL << shift);
    bits_ |= (static_cast<std::uint64_t>(rank) & 0xFULL) << shift;
}

FastMoveResult FastBoard::MoveLeft() const {
    EnsureTables();
    const auto& tables = Tables();

    std::uint64_t next = bits_;
    std::uint32_t score = 0;
    for (int row = 0; row < kBoardSize; ++row) {
        const std::uint16_t current = ExtractRow(bits_, row);
        const std::uint16_t moved = tables.leftResult[current];
        next = SetRow(next, row, moved);
        score += tables.leftScore[current];
    }
    return {next, score, next != bits_};
}

FastMoveResult FastBoard::MoveRight() const {
    EnsureTables();
    const auto& tables = Tables();

    std::uint64_t next = bits_;
    std::uint32_t score = 0;
    for (int row = 0; row < kBoardSize; ++row) {
        const std::uint16_t current = ExtractRow(bits_, row);
        const std::uint16_t moved = tables.rightResult[current];
        next = SetRow(next, row, moved);
        score += tables.rightScore[current];
    }
    return {next, score, next != bits_};
}

FastMoveResult FastBoard::MoveUp() const {
    const FastBoard transposed = Transpose();
    const auto moved = transposed.MoveLeft();
    return {FastBoard(moved.board).Transpose().Bits(), moved.scoreDelta, moved.changed};
}

FastMoveResult FastBoard::MoveDown() const {
    const FastBoard transposed = Transpose();
    const auto moved = transposed.MoveRight();
    return {FastBoard(moved.board).Transpose().Bits(), moved.scoreDelta, moved.changed};
}

bool FastBoard::CanMove() const {
    if (CountEmpty() > 0) {
        return true;
    }
    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            const int index = row * kBoardSize + col;
            const int rank = GetRank(index);
            if (row + 1 < kBoardSize && rank == GetRank(index + kBoardSize)) {
                return true;
            }
            if (col + 1 < kBoardSize && rank == GetRank(index + 1)) {
                return true;
            }
        }
    }
    return false;
}

int FastBoard::CountEmpty() const {
    int count = 0;
    for (int index = 0; index < kCellCount; ++index) {
        count += GetRank(index) == 0 ? 1 : 0;
    }
    return count;
}

int FastBoard::MaxRank() const {
    int maxRank = 0;
    for (int index = 0; index < kCellCount; ++index) {
        maxRank = std::max(maxRank, GetRank(index));
    }
    return maxRank;
}

int FastBoard::MaxTile() const {
    const int rank = MaxRank();
    return rank == 0 ? 0 : (1 << rank);
}

FastBoard FastBoard::Transpose() const {
    FastBoard out;
    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            out.SetRank(col * kBoardSize + row, GetRank(row * kBoardSize + col));
        }
    }
    return out;
}

FastBoard FastBoard::Mirror() const {
    FastBoard out;
    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            out.SetRank(row * kBoardSize + (kBoardSize - 1 - col), GetRank(row * kBoardSize + col));
        }
    }
    return out;
}

std::uint64_t FastBoard::Key() const {
    return bits_;
}

Board FastBoard::ToReference() const {
    Board board;
    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            const int index = row * kBoardSize + col;
            board.Set(row, col, GetValue(index));
        }
    }
    return board;
}

std::vector<int> FastBoard::EmptyIndices() const {
    std::vector<int> indices;
    indices.reserve(kCellCount);
    for (int index = 0; index < kCellCount; ++index) {
        if (GetRank(index) == 0) {
            indices.push_back(index);
        }
    }
    return indices;
}

bool FastBoard::operator==(const FastBoard& other) const {
    return bits_ == other.bits_;
}

bool FastBoard::operator!=(const FastBoard& other) const {
    return !(*this == other);
}

std::string FastBoard::ToString() const {
    return ToReference().ToString();
}

}  // namespace game2048
