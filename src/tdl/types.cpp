#include "tdl/types.h"

#if defined(__BMI2__) && (defined(__x86_64__) || defined(_M_X64))
#include <immintrin.h>
#endif

namespace game2048::ai {

namespace {

std::uint64_t EmptyCellMask(std::uint64_t bits) {
    bits |= bits >> 2U;
    bits |= bits >> 1U;
    return ~bits & 0x1111111111111111ULL;
}

std::uint32_t PopCount64(std::uint64_t value) {
#if defined(__GNUC__) || defined(__clang__)
    return static_cast<std::uint32_t>(__builtin_popcountll(value));
#else
    std::uint32_t count = 0;
    while (value != 0) {
        value &= value - 1U;
        ++count;
    }
    return count;
#endif
}

int CellIndexFromNibbleMask(std::uint64_t mask) {
#if defined(__GNUC__) || defined(__clang__)
    return static_cast<int>(__builtin_ctzll(mask) >> 2U);
#else
    int shift = 0;
    while (((mask >> shift) & 1ULL) == 0) {
        ++shift;
    }
    return shift >> 2;
#endif
}

}  // namespace

TdlRandom::TdlRandom(std::uint32_t seed)
    : engine_(seed) {}

std::uint32_t TdlRandom::NextU32() {
    return static_cast<std::uint32_t>(engine_());
}

FastBoard TdlRandom::InitBoard() {
    const std::uint32_t u = NextU32();
    const std::uint32_t k = u & 0xffffU;
    const std::uint32_t i = k % 16U;
    const std::uint32_t j = (i + 1U + ((k >> 4U) % 15U)) % 16U;
    const std::uint32_t r = (u >> 16U) % 100U;
    std::uint64_t raw = ((r >= 1U ? 1ULL : 2ULL) << (i << 2U)) |
                        ((r >= 19U ? 1ULL : 2ULL) << (j << 2U));
    return FastBoard(raw);
}

bool TdlRandom::SpawnNext(FastBoard& board) {
    std::uint64_t emptyMask = EmptyCellMask(board.Bits());
    const std::uint32_t emptyCount = PopCount64(emptyMask);
    if (emptyCount == 0) {
        return false;
    }

    const std::uint32_t u = NextU32();
    const std::uint32_t selected = (u >> 16U) % emptyCount;
#if defined(__BMI2__) && (defined(__x86_64__) || defined(_M_X64))
    const std::uint64_t selectedMask = _pdep_u64(1ULL << selected, emptyMask);
#else
    std::uint32_t remaining = selected;
    while (remaining != 0U) {
        emptyMask &= emptyMask - 1U;
        --remaining;
    }
    const std::uint64_t selectedMask = emptyMask & (~emptyMask + 1U);
#endif
    const int rank = (u % 10U) != 0U ? 1 : 2;
    board.SetRank(CellIndexFromNibbleMask(selectedMask), rank);
    return true;
}

}  // namespace game2048::ai
