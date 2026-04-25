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

struct Row20Tables {
    std::array<std::uint32_t, 1u << 20> leftResult {};
    std::array<std::uint32_t, 1u << 20> rightResult {};
    std::array<std::uint32_t, 1u << 20> leftScore {};
    std::array<std::uint32_t, 1u << 20> rightScore {};
    bool initialized = false;
};

RowTables& Tables() {
    static RowTables tables;
    return tables;
}

Row20Tables& Tables20() {
    static Row20Tables tables;
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

std::uint32_t ReverseRow20(std::uint32_t row) {
    return ((row & 0x0001FU) << 15U) |
           ((row & 0x003E0U) << 5U) |
           ((row & 0x07C00U) >> 5U) |
           ((row & 0xF8000U) >> 15U);
}

std::array<int, 4> UnpackRow20(std::uint32_t row) {
    return {
        static_cast<int>(row & 0x1FU),
        static_cast<int>((row >> 5U) & 0x1FU),
        static_cast<int>((row >> 10U) & 0x1FU),
        static_cast<int>((row >> 15U) & 0x1FU)
    };
}

std::uint32_t PackRow20(const std::array<int, 4>& ranks) {
    std::uint32_t row = 0;
    for (std::size_t index = 0; index < ranks.size(); ++index) {
        row |= static_cast<std::uint32_t>(ranks[index] & 0x1F) << (index * 5U);
    }
    return row;
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

void EnsureTables20() {
    auto& tables = Tables20();
    if (tables.initialized) {
        return;
    }

    for (std::uint32_t row = 0; row < (1u << 20); ++row) {
        const std::array<int, 4> ranks = UnpackRow20(row);
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
            if (index + 1 < count && compacted[index] == compacted[index + 1] && compacted[index] < 31) {
                const int mergedRank = compacted[index] + 1;
                output[write] = mergedRank;
                ++write;
                score += static_cast<std::uint32_t>(1ULL << mergedRank);
                index += 2;
            } else {
                output[write] = compacted[index];
                ++write;
                ++index;
            }
        }

        tables.leftResult[row] = PackRow20(output);
        tables.leftScore[row] = score;
    }

    for (std::uint32_t row = 0; row < (1u << 20); ++row) {
        const std::uint32_t reversed = ReverseRow20(row);
        tables.rightResult[row] = ReverseRow20(tables.leftResult[reversed]);
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

std::uint32_t ExtractPackedRow(const PackedBoard& board, int row) {
    std::array<int, 4> ranks {};
    for (int col = 0; col < kBoardSize; ++col) {
        ranks[static_cast<std::size_t>(col)] = board.GetRank(row * kBoardSize + col);
    }
    return PackRow20(ranks);
}

PackedBoard SetPackedRow(PackedBoard board, int row, std::uint32_t value) {
    const std::array<int, 4> ranks = UnpackRow20(value);
    for (int col = 0; col < kBoardSize; ++col) {
        board.SetRank(row * kBoardSize + col, ranks[static_cast<std::size_t>(col)]);
    }
    return board;
}

}  // namespace

bool PackedBoardMoveTablesInitialized() {
    return Tables20().initialized;
}

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

FastBoard FastBoard::TransformD4(int transform) const {
    FastBoard out;
    const int normalized = ((transform % 8) + 8) % 8;
    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            int nextRow = row;
            int nextCol = col;
            switch (normalized) {
                case 0:
                    break;
                case 1:
                    nextRow = col;
                    nextCol = kBoardSize - 1 - row;
                    break;
                case 2:
                    nextRow = kBoardSize - 1 - row;
                    nextCol = kBoardSize - 1 - col;
                    break;
                case 3:
                    nextRow = kBoardSize - 1 - col;
                    nextCol = row;
                    break;
                case 4:
                    nextRow = row;
                    nextCol = kBoardSize - 1 - col;
                    break;
                case 5:
                    nextRow = kBoardSize - 1 - row;
                    nextCol = col;
                    break;
                case 6:
                    nextRow = col;
                    nextCol = row;
                    break;
                case 7:
                    nextRow = kBoardSize - 1 - col;
                    nextCol = kBoardSize - 1 - row;
                    break;
            }
            out.SetRank(nextRow * kBoardSize + nextCol, GetRank(row * kBoardSize + col));
        }
    }
    return out;
}

std::uint64_t FastBoard::CanonicalKey() const {
    std::uint64_t best = TransformD4(0).Key();
    for (int transform = 1; transform < 8; ++transform) {
        best = std::min(best, TransformD4(transform).Key());
    }
    return best;
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
    std::array<int, kCellCount> stackIndices {};
    const std::size_t count = CollectEmptyIndices(stackIndices);
    for (std::size_t offset = 0; offset < count; ++offset) {
        indices.push_back(stackIndices[offset]);
    }
    return indices;
}

std::size_t FastBoard::CollectEmptyIndices(std::array<int, kCellCount>& indices) const {
    std::size_t count = 0;
    for (int index = 0; index < kCellCount; ++index) {
        if (GetRank(index) == 0) {
            indices[count++] = index;
        }
    }
    return count;
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

PackedBoard::PackedBoard() = default;

PackedBoard::PackedBoard(std::uint64_t raw, std::uint16_t ext)
    : raw_(raw), ext_(ext) {}

PackedBoard::PackedBoard(const FastBoard& board)
    : raw_(board.Bits()), ext_(0) {}

std::uint64_t PackedBoard::Raw() const {
    return raw_;
}

std::uint16_t PackedBoard::Ext() const {
    return ext_;
}

int PackedBoard::GetRank(int index) const {
    const int low = static_cast<int>((raw_ >> (index * 4)) & 0xFULL);
    const int high = static_cast<int>((ext_ >> index) & 0x1U);
    return low | (high << 4);
}

void PackedBoard::SetRank(int index, int rank) {
    rank = std::clamp(rank, 0, 31);
    const std::uint64_t shift = static_cast<std::uint64_t>(index * 4);
    raw_ &= ~(0xFULL << shift);
    raw_ |= (static_cast<std::uint64_t>(rank) & 0xFULL) << shift;
    const std::uint16_t bit = static_cast<std::uint16_t>(1U << index);
    if ((rank & 0x10) != 0) {
        ext_ |= bit;
    } else {
        ext_ &= static_cast<std::uint16_t>(~bit);
    }
}

PackedMoveResult PackedBoard::MoveLeft() const {
    EnsureTables20();
    const auto& tables = Tables20();
    PackedBoard next(*this);
    std::uint32_t score = 0;
    for (int row = 0; row < kBoardSize; ++row) {
        const std::uint32_t current = ExtractPackedRow(*this, row);
        const std::uint32_t moved = tables.leftResult[current];
        next = SetPackedRow(next, row, moved);
        score += tables.leftScore[current];
    }
    return {next, score, next != *this};
}

PackedMoveResult PackedBoard::MoveRight() const {
    EnsureTables20();
    const auto& tables = Tables20();
    PackedBoard next(*this);
    std::uint32_t score = 0;
    for (int row = 0; row < kBoardSize; ++row) {
        const std::uint32_t current = ExtractPackedRow(*this, row);
        const std::uint32_t moved = tables.rightResult[current];
        next = SetPackedRow(next, row, moved);
        score += tables.rightScore[current];
    }
    return {next, score, next != *this};
}

PackedMoveResult PackedBoard::MoveUp() const {
    const PackedBoard transposed = Transpose();
    const auto moved = transposed.MoveLeft();
    return {moved.board.Transpose(), moved.scoreDelta, moved.changed};
}

PackedMoveResult PackedBoard::MoveDown() const {
    const PackedBoard transposed = Transpose();
    const auto moved = transposed.MoveRight();
    return {moved.board.Transpose(), moved.scoreDelta, moved.changed};
}

bool PackedBoard::CanMove() const {
    if (CountEmpty() > 0) {
        return true;
    }
    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            const int index = row * kBoardSize + col;
            const int rank = GetRank(index);
            if (row + 1 < kBoardSize && rank < 31 && rank == GetRank(index + kBoardSize)) {
                return true;
            }
            if (col + 1 < kBoardSize && rank < 31 && rank == GetRank(index + 1)) {
                return true;
            }
        }
    }
    return false;
}

int PackedBoard::CountEmpty() const {
    int count = 0;
    for (int index = 0; index < kCellCount; ++index) {
        count += GetRank(index) == 0 ? 1 : 0;
    }
    return count;
}

int PackedBoard::MaxRank() const {
    int maxRank = 0;
    for (int index = 0; index < kCellCount; ++index) {
        maxRank = std::max(maxRank, GetRank(index));
    }
    return maxRank;
}

int PackedBoard::MaxTile() const {
    const int rank = MaxRank();
    return rank == 0 || rank >= 31 ? 0 : (1 << rank);
}

PackedBoard PackedBoard::Transpose() const {
    PackedBoard out;
    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            out.SetRank(col * kBoardSize + row, GetRank(row * kBoardSize + col));
        }
    }
    return out;
}

PackedBoard PackedBoard::Mirror() const {
    PackedBoard out;
    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            out.SetRank(row * kBoardSize + (kBoardSize - 1 - col), GetRank(row * kBoardSize + col));
        }
    }
    return out;
}

PackedBoard PackedBoard::TransformD4(int transform) const {
    PackedBoard out;
    const int normalized = ((transform % 8) + 8) % 8;
    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            int nextRow = row;
            int nextCol = col;
            switch (normalized) {
                case 0:
                    break;
                case 1:
                    nextRow = col;
                    nextCol = kBoardSize - 1 - row;
                    break;
                case 2:
                    nextRow = kBoardSize - 1 - row;
                    nextCol = kBoardSize - 1 - col;
                    break;
                case 3:
                    nextRow = kBoardSize - 1 - col;
                    nextCol = row;
                    break;
                case 4:
                    nextRow = row;
                    nextCol = kBoardSize - 1 - col;
                    break;
                case 5:
                    nextRow = kBoardSize - 1 - row;
                    nextCol = col;
                    break;
                case 6:
                    nextRow = col;
                    nextCol = row;
                    break;
                case 7:
                    nextRow = kBoardSize - 1 - col;
                    nextCol = kBoardSize - 1 - row;
                    break;
            }
            out.SetRank(nextRow * kBoardSize + nextCol, GetRank(row * kBoardSize + col));
        }
    }
    return out;
}

std::uint64_t PackedBoard::CanonicalKey64() const {
    std::uint64_t best = TransformD4(0).ToFastBoardClamped().Key();
    for (int transform = 1; transform < 8; ++transform) {
        best = std::min(best, TransformD4(transform).ToFastBoardClamped().Key());
    }
    return best;
}

FastBoard PackedBoard::ToFastBoardClamped(int maxRank) const {
    FastBoard out;
    for (int index = 0; index < kCellCount; ++index) {
        out.SetRank(index, std::min(GetRank(index), maxRank));
    }
    return out;
}

bool PackedBoard::operator==(const PackedBoard& other) const {
    return raw_ == other.raw_ && ext_ == other.ext_;
}

bool PackedBoard::operator!=(const PackedBoard& other) const {
    return !(*this == other);
}

}  // namespace game2048
