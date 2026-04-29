#include "core/board_fast.h"

#include <algorithm>
#include <array>
#include <sstream>

#if defined(__AVX2__) && (defined(__x86_64__) || defined(_M_X64))
#include <immintrin.h>
#endif

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

[[maybe_unused]] std::uint16_t ExtractColumn(std::uint64_t bits, int col) {
    const std::uint64_t shifted = bits >> (col * 4);
    return static_cast<std::uint16_t>(
        ((shifted & 0x000F000F000F000FULL) * 0x0001001001001000ULL) >> 48U);
}

[[maybe_unused]] std::uint64_t SetColumn(std::uint64_t bits, int col, std::uint16_t value) {
    const std::uint64_t mask = 0x000F000F000F000FULL << (col * 4);
    const std::uint64_t expanded = static_cast<std::uint64_t>(value) |
                                   (static_cast<std::uint64_t>(value) << 12U) |
                                   (static_cast<std::uint64_t>(value) << 24U) |
                                   (static_cast<std::uint64_t>(value) << 36U);
    return (bits & ~mask) | ((expanded & 0x000F000F000F000FULL) << (col * 4));
}

#if defined(__AVX2__) && (defined(__x86_64__) || defined(_M_X64))
std::uint64_t TransposeBits64(std::uint64_t bits) {
    std::uint64_t buffer = (bits ^ (bits >> 12U)) & 0x0000F0F00000F0F0ULL;
    bits ^= buffer ^ (buffer << 12U);
    buffer = (bits ^ (bits >> 24U)) & 0x00000000FF00FF00ULL;
    bits ^= buffer ^ (buffer << 24U);
    return bits;
}
#endif

}  // namespace

bool PackedBoardMoveTablesInitialized() {
    return Tables20().initialized;
}

FastBoard::FastBoard() = default;

FastBoard::FastBoard(std::uint64_t bits)
    : bits_(bits) {}

FastBoard FastBoard::FromReference(const Board& board) {
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

void FastBoard::TdlOrderMoves(std::array<FastMoveResult, 4>& moves) const {
#if defined(__AVX2__) && (defined(__x86_64__) || defined(_M_X64))
    __m256i dst;
    __m256i buffer;
    __m256i ranks;
    __m256i reward;
    __m256i check;

    const std::uint64_t transposed = TransposeBits64(bits_);
    dst = _mm256_set_epi64x(static_cast<long long>(bits_), 0, 0, static_cast<long long>(transposed));
    buffer = _mm256_set_epi64x(0, static_cast<long long>(transposed), static_cast<long long>(bits_), 0);
    dst = _mm256_or_si256(dst, _mm256_slli_epi16(buffer, 12));
    dst = _mm256_or_si256(dst, _mm256_slli_epi16(_mm256_and_si256(buffer, _mm256_set1_epi16(0x00f0)), 4));
    dst = _mm256_or_si256(dst, _mm256_srli_epi16(_mm256_and_si256(buffer, _mm256_set1_epi16(0x0f00)), 4));
    dst = _mm256_or_si256(dst, _mm256_srli_epi16(buffer, 12));

    buffer = _mm256_and_si256(dst, _mm256_set1_epi16(0x0f00));
    check = _mm256_and_si256(_mm256_cmpeq_epi16(buffer, _mm256_setzero_si256()),
                             _mm256_set1_epi16(static_cast<short>(0xff00)));
    dst = _mm256_or_si256(_mm256_and_si256(check, _mm256_srli_epi16(dst, 4)), _mm256_andnot_si256(check, dst));
    buffer = _mm256_and_si256(dst, _mm256_set1_epi16(0x00f0));
    check = _mm256_and_si256(_mm256_cmpeq_epi16(buffer, _mm256_setzero_si256()),
                             _mm256_set1_epi16(static_cast<short>(0xfff0)));
    dst = _mm256_or_si256(_mm256_and_si256(check, _mm256_srli_epi16(dst, 4)), _mm256_andnot_si256(check, dst));
    buffer = _mm256_and_si256(dst, _mm256_set1_epi16(0x000f));
    check = _mm256_cmpeq_epi16(buffer, _mm256_setzero_si256());
    dst = _mm256_or_si256(_mm256_and_si256(check, _mm256_srli_epi16(dst, 4)), _mm256_andnot_si256(check, dst));

    buffer = _mm256_srli_epi16(_mm256_add_epi8(dst, _mm256_set1_epi16(0x0010)), 4);
    ranks = _mm256_and_si256(dst, _mm256_set1_epi16(0x000f));
    check = _mm256_and_si256(_mm256_srli_epi16(dst, 4), _mm256_set1_epi16(0x000f));
    check = _mm256_andnot_si256(_mm256_cmpeq_epi16(ranks, _mm256_setzero_si256()), _mm256_cmpeq_epi16(ranks, check));
    dst = _mm256_or_si256(_mm256_and_si256(check, buffer), _mm256_andnot_si256(check, dst));
    check = _mm256_cvtepu16_epi32(_mm256_extracti128_si256(_mm256_and_si256(check, _mm256_set1_epi16(0x0001)), 0));
    ranks = _mm256_cvtepu16_epi32(_mm256_extracti128_si256(_mm256_add_epi16(ranks, _mm256_set1_epi16(0x0001)), 0));
    reward = _mm256_sllv_epi32(check, ranks);

    buffer = _mm256_add_epi8(_mm256_srli_epi16(dst, 4), _mm256_set1_epi16(0x0010));
    ranks = _mm256_and_si256(buffer, _mm256_set1_epi16(0x000f));
    check = _mm256_and_si256(_mm256_srli_epi16(dst, 8), _mm256_set1_epi16(0x000f));
    check = _mm256_andnot_si256(_mm256_cmpeq_epi16(ranks, _mm256_setzero_si256()), _mm256_cmpeq_epi16(ranks, check));
    check = _mm256_and_si256(check, _mm256_set1_epi16(static_cast<short>(0xfff0)));
    dst = _mm256_or_si256(_mm256_and_si256(check, buffer), _mm256_andnot_si256(check, dst));
    check = _mm256_cvtepu16_epi32(_mm256_extracti128_si256(_mm256_srli_epi16(check, 15), 0));
    ranks = _mm256_cvtepu16_epi32(_mm256_extracti128_si256(_mm256_add_epi16(ranks, _mm256_set1_epi16(0x0001)), 0));
    reward = _mm256_add_epi32(reward, _mm256_sllv_epi32(check, ranks));

    buffer = _mm256_srli_epi16(_mm256_add_epi16(dst, _mm256_set1_epi16(0x1000)), 4);
    ranks = _mm256_srli_epi16(dst, 12);
    check = _mm256_and_si256(_mm256_srli_epi16(dst, 8), _mm256_set1_epi16(0x000f));
    check = _mm256_andnot_si256(_mm256_cmpeq_epi16(ranks, _mm256_setzero_si256()), _mm256_cmpeq_epi16(ranks, check));
    check = _mm256_and_si256(check, _mm256_set1_epi16(static_cast<short>(0xff00)));
    dst = _mm256_or_si256(_mm256_and_si256(check, buffer), _mm256_andnot_si256(check, dst));
    check = _mm256_cvtepu16_epi32(_mm256_extracti128_si256(_mm256_srli_epi16(check, 15), 0));
    ranks = _mm256_cvtepu16_epi32(_mm256_extracti128_si256(_mm256_add_epi16(ranks, _mm256_set1_epi16(0x0001)), 0));
    reward = _mm256_add_epi32(reward, _mm256_sllv_epi32(check, ranks));

    const std::uint64_t left = static_cast<std::uint64_t>(_mm256_extract_epi64(dst, 3));
    buffer = _mm256_slli_epi16(dst, 12);
    buffer = _mm256_or_si256(buffer, _mm256_slli_epi16(_mm256_and_si256(dst, _mm256_set1_epi16(0x00f0)), 4));
    buffer = _mm256_or_si256(buffer, _mm256_srli_epi16(_mm256_and_si256(dst, _mm256_set1_epi16(0x0f00)), 4));
    buffer = _mm256_or_si256(buffer, _mm256_srli_epi16(dst, 12));
    const std::uint64_t right = static_cast<std::uint64_t>(_mm256_extract_epi64(buffer, 1));

    buffer = _mm256_blend_epi32(dst, buffer, 0b11111100);
    ranks = _mm256_and_si256(_mm256_xor_si256(buffer, _mm256_srli_epi64(buffer, 12)),
                             _mm256_set1_epi64x(0x0000f0f00000f0f0LL));
    buffer = _mm256_xor_si256(buffer, _mm256_xor_si256(ranks, _mm256_slli_epi64(ranks, 12)));
    ranks = _mm256_and_si256(_mm256_xor_si256(buffer, _mm256_srli_epi64(buffer, 24)),
                             _mm256_set1_epi64x(0x00000000ff00ff00LL));
    buffer = _mm256_xor_si256(buffer, _mm256_xor_si256(ranks, _mm256_slli_epi64(ranks, 24)));
    const std::uint64_t up = static_cast<std::uint64_t>(_mm256_extract_epi64(buffer, 0));
    const std::uint64_t down = static_cast<std::uint64_t>(_mm256_extract_epi64(buffer, 2));

    reward = _mm256_add_epi64(reward, _mm256_srli_si256(reward, 8));
    reward = _mm256_add_epi64(reward, _mm256_srli_epi64(reward, 32));
    const std::uint32_t upScore = static_cast<std::uint32_t>(_mm256_extract_epi32(reward, 0));
    const std::uint32_t rightScore = static_cast<std::uint32_t>(_mm256_extract_epi32(reward, 4));
    const __m256i selectedScores = _mm256_set_epi64x(static_cast<long long>(rightScore),
                                                     static_cast<long long>(upScore),
                                                     static_cast<long long>(rightScore),
                                                     static_cast<long long>(upScore));
    check = _mm256_cmpeq_epi64(_mm256_set_epi64x(static_cast<long long>(left),
                                                static_cast<long long>(down),
                                                static_cast<long long>(right),
                                                static_cast<long long>(up)),
                               _mm256_set1_epi64x(static_cast<long long>(bits_)));
    const __m256i infos = _mm256_or_si256(selectedScores, check);

    const std::uint32_t upInfo = static_cast<std::uint32_t>(_mm256_extract_epi32(infos, 0));
    const std::uint32_t rightInfo = static_cast<std::uint32_t>(_mm256_extract_epi32(infos, 2));
    const std::uint32_t downInfo = static_cast<std::uint32_t>(_mm256_extract_epi32(infos, 4));
    const std::uint32_t leftInfo = static_cast<std::uint32_t>(_mm256_extract_epi32(infos, 6));
    const bool upChanged = upInfo != UINT32_MAX;
    const bool rightChanged = rightInfo != UINT32_MAX;
    const bool downChanged = downInfo != UINT32_MAX;
    const bool leftChanged = leftInfo != UINT32_MAX;
    moves[0] = {up, upChanged ? upInfo : 0U, upChanged};
    moves[1] = {right, rightChanged ? rightInfo : 0U, rightChanged};
    moves[2] = {down, downChanged ? downInfo : 0U, downChanged};
    moves[3] = {left, leftChanged ? leftInfo : 0U, leftChanged};
    return;
#else
    EnsureTables();
    const auto& tables = Tables();

    std::uint64_t up = bits_;
    std::uint64_t right = bits_;
    std::uint64_t down = bits_;
    std::uint64_t left = bits_;
    std::uint32_t upScore = 0;
    std::uint32_t rightScore = 0;
    std::uint32_t downScore = 0;
    std::uint32_t leftScore = 0;

    for (int row = 0; row < kBoardSize; ++row) {
        const std::uint16_t current = ExtractRow(bits_, row);
        const std::uint16_t movedLeft = tables.leftResult[current];
        const std::uint16_t movedRight = tables.rightResult[current];
        left = SetRow(left, row, movedLeft);
        right = SetRow(right, row, movedRight);
        leftScore += tables.leftScore[current];
        rightScore += tables.rightScore[current];
    }

    for (int col = 0; col < kBoardSize; ++col) {
        const std::uint16_t current = ExtractColumn(bits_, col);
        const std::uint16_t movedUp = tables.leftResult[current];
        const std::uint16_t movedDown = tables.rightResult[current];
        up = SetColumn(up, col, movedUp);
        down = SetColumn(down, col, movedDown);
        upScore += tables.leftScore[current];
        downScore += tables.rightScore[current];
    }

    moves[0] = {up, upScore, up != bits_};
    moves[1] = {right, rightScore, right != bits_};
    moves[2] = {down, downScore, down != bits_};
    moves[3] = {left, leftScore, left != bits_};
#endif
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
    std::uint64_t out = 0;
    out |= bits_ & 0xFULL;
    out |= ((bits_ >> 4U) & 0xFULL) << 16U;
    out |= ((bits_ >> 8U) & 0xFULL) << 32U;
    out |= ((bits_ >> 12U) & 0xFULL) << 48U;
    out |= ((bits_ >> 16U) & 0xFULL) << 4U;
    out |= bits_ & (0xFULL << 20U);
    out |= ((bits_ >> 24U) & 0xFULL) << 36U;
    out |= ((bits_ >> 28U) & 0xFULL) << 52U;
    out |= ((bits_ >> 32U) & 0xFULL) << 8U;
    out |= ((bits_ >> 36U) & 0xFULL) << 24U;
    out |= bits_ & (0xFULL << 40U);
    out |= ((bits_ >> 44U) & 0xFULL) << 56U;
    out |= ((bits_ >> 48U) & 0xFULL) << 12U;
    out |= ((bits_ >> 52U) & 0xFULL) << 28U;
    out |= ((bits_ >> 56U) & 0xFULL) << 44U;
    out |= bits_ & (0xFULL << 60U);
    return FastBoard(out);
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
