#include "value/ntuple_kernel.h"

#include <array>

#if defined(GAME2048_ENABLE_AVX512) && (defined(__x86_64__) || defined(_M_X64))
#include <immintrin.h>
#endif

namespace game2048::ai::ntuple_kernel {

namespace {

std::size_t KeyForTransform6(std::uint64_t bits, const std::uint8_t* shifts, std::size_t transform) {
    std::size_t key = 0;
    key |= static_cast<std::size_t>((bits >> shifts[0 * kFixed6Transforms + transform]) & 0xFULL);
    key |= static_cast<std::size_t>((bits >> shifts[1 * kFixed6Transforms + transform]) & 0xFULL) << 4U;
    key |= static_cast<std::size_t>((bits >> shifts[2 * kFixed6Transforms + transform]) & 0xFULL) << 8U;
    key |= static_cast<std::size_t>((bits >> shifts[3 * kFixed6Transforms + transform]) & 0xFULL) << 12U;
    key |= static_cast<std::size_t>((bits >> shifts[4 * kFixed6Transforms + transform]) & 0xFULL) << 16U;
    key |= static_cast<std::size_t>((bits >> shifts[5 * kFixed6Transforms + transform]) & 0xFULL) << 20U;
    return key;
}

std::size_t CollectFixed6KeysScalar(std::uint64_t boardBits,
                                    const std::size_t* patternOffsets,
                                    const std::uint8_t* shifts,
                                    std::size_t patternCount,
                                    std::size_t* outKeys) {
    std::size_t written = 0;
    for (std::size_t pattern = 0; pattern < patternCount; ++pattern) {
        const std::size_t offset = patternOffsets[pattern];
        const std::uint8_t* patternShifts = shifts + pattern * kFixed6Transforms * kFixed6Cells;
        for (std::size_t transform = 0; transform < kFixed6Transforms; ++transform) {
            outKeys[written] = offset + KeyForTransform6(boardBits, patternShifts, transform);
            ++written;
        }
    }
    return written;
}

double EvaluateFixed6Scalar(std::uint64_t boardBits,
                            const float* weights,
                            const std::size_t* patternOffsets,
                            const std::uint8_t* shifts,
                            std::size_t patternCount) {
    double value = 0.0;
    for (std::size_t pattern = 0; pattern < patternCount; ++pattern) {
        const std::size_t offset = patternOffsets[pattern];
        const std::uint8_t* patternShifts = shifts + pattern * kFixed6Transforms * kFixed6Cells;
        for (std::size_t transform = 0; transform < kFixed6Transforms; ++transform) {
            value += weights[offset + KeyForTransform6(boardBits, patternShifts, transform)];
        }
    }
    return value;
}

#if defined(GAME2048_ENABLE_AVX512) && (defined(__x86_64__) || defined(_M_X64))
bool CpuHasAvx512() {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_cpu_supports("avx512f") != 0 &&
           __builtin_cpu_supports("avx512vl") != 0;
#else
    return false;
#endif
}

__attribute__((target("avx512f,avx512vl")))
std::size_t CollectFixed6KeysAvx512(std::uint64_t boardBits,
                                    const std::size_t* patternOffsets,
                                    const std::uint8_t* shifts,
                                    std::size_t patternCount,
                                    std::size_t* outKeys) {
    const __m512i bits = _mm512_set1_epi64(static_cast<long long>(boardBits));
    const __m512i mask = _mm512_set1_epi64(0xFULL);
    std::size_t written = 0;
    alignas(64) std::array<std::uint64_t, kFixed6Transforms> lanes {};

    for (std::size_t pattern = 0; pattern < patternCount; ++pattern) {
        const std::uint8_t* patternShifts = shifts + pattern * kFixed6Transforms * kFixed6Cells;
        __m512i key = _mm512_setzero_si512();
        for (std::size_t cell = 0; cell < kFixed6Cells; ++cell) {
            const __m128i shiftBytes = _mm_loadl_epi64(
                reinterpret_cast<const __m128i*>(patternShifts + cell * kFixed6Transforms));
            const __m512i shiftCounts = _mm512_cvtepu8_epi64(shiftBytes);
            const __m512i ranks = _mm512_and_si512(_mm512_srlv_epi64(bits, shiftCounts), mask);
            key = _mm512_or_si512(key, _mm512_slli_epi64(ranks, static_cast<int>(cell * 4U)));
        }
        key = _mm512_add_epi64(key, _mm512_set1_epi64(static_cast<long long>(patternOffsets[pattern])));
        _mm512_store_si512(reinterpret_cast<__m512i*>(lanes.data()), key);
        for (std::size_t index = 0; index < kFixed6Transforms; ++index) {
            outKeys[written] = static_cast<std::size_t>(lanes[index]);
            ++written;
        }
    }
    return written;
}

__attribute__((target("avx512f,avx512vl")))
double EvaluateFixed6Avx512(std::uint64_t boardBits,
                            const float* weights,
                            const std::size_t* patternOffsets,
                            const std::uint8_t* shifts,
                            std::size_t patternCount) {
    const __m512i bits = _mm512_set1_epi64(static_cast<long long>(boardBits));
    const __m512i mask = _mm512_set1_epi64(0xFULL);
    __m256 total = _mm256_setzero_ps();

    for (std::size_t pattern = 0; pattern < patternCount; ++pattern) {
        const std::uint8_t* patternShifts = shifts + pattern * kFixed6Transforms * kFixed6Cells;
        __m512i key = _mm512_setzero_si512();
        for (std::size_t cell = 0; cell < kFixed6Cells; ++cell) {
            const __m128i shiftBytes = _mm_loadl_epi64(
                reinterpret_cast<const __m128i*>(patternShifts + cell * kFixed6Transforms));
            const __m512i shiftCounts = _mm512_cvtepu8_epi64(shiftBytes);
            const __m512i ranks = _mm512_and_si512(_mm512_srlv_epi64(bits, shiftCounts), mask);
            key = _mm512_or_si512(key, _mm512_slli_epi64(ranks, static_cast<int>(cell * 4U)));
        }
        key = _mm512_add_epi64(key, _mm512_set1_epi64(static_cast<long long>(patternOffsets[pattern])));
        total = _mm256_add_ps(total, _mm512_i64gather_ps(key, weights, 4));
    }

    alignas(32) std::array<float, kFixed6Transforms> lanes {};
    _mm256_store_ps(lanes.data(), total);
    double value = 0.0;
    for (float lane : lanes) {
        value += lane;
    }
    return value;
}
#else
bool CpuHasAvx512() {
    return false;
}
#endif

[[maybe_unused]] bool ShouldUseAvx512(KernelKind kind) {
    if (kind == KernelKind::Scalar) {
        return false;
    }
    if (kind == KernelKind::Avx512 || kind == KernelKind::Auto) {
        return CpuHasAvx512();
    }
    return false;
}

}  // namespace

bool IsAvx512Available() {
    return CpuHasAvx512();
}

const char* ActiveKernelName() {
    return IsAvx512Available() ? "avx512" : "scalar";
}

std::size_t CollectFixed6Keys(std::uint64_t boardBits,
                              const std::size_t* patternOffsets,
                              const std::uint8_t* shifts,
                              std::size_t patternCount,
                              std::size_t* outKeys,
                              KernelKind kind) {
#if defined(GAME2048_ENABLE_AVX512) && (defined(__x86_64__) || defined(_M_X64))
    if (ShouldUseAvx512(kind)) {
        return CollectFixed6KeysAvx512(boardBits, patternOffsets, shifts, patternCount, outKeys);
    }
#else
    static_cast<void>(kind);
#endif
    return CollectFixed6KeysScalar(boardBits, patternOffsets, shifts, patternCount, outKeys);
}

double EvaluateFixed6(std::uint64_t boardBits,
                      const float* weights,
                      const std::size_t* patternOffsets,
                      const std::uint8_t* shifts,
                      std::size_t patternCount,
                      KernelKind kind) {
#if defined(GAME2048_ENABLE_AVX512) && (defined(__x86_64__) || defined(_M_X64))
    if (ShouldUseAvx512(kind)) {
        return EvaluateFixed6Avx512(boardBits, weights, patternOffsets, shifts, patternCount);
    }
#else
    static_cast<void>(kind);
#endif
    return EvaluateFixed6Scalar(boardBits, weights, patternOffsets, shifts, patternCount);
}

}  // namespace game2048::ai::ntuple_kernel
