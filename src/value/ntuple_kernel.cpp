#include "value/ntuple_kernel.h"

#include <array>

#if defined(GAME2048_ENABLE_AVX512) && (defined(__x86_64__) || defined(_M_X64))
#include <immintrin.h>
#endif

namespace game2048::ai::ntuple_kernel {

namespace {

void CollectPatternFixed6Keys(std::uint64_t bits, const std::uint8_t* shifts, std::size_t* keys) {
    for (std::size_t transform = 0; transform < kFixed6Transforms; ++transform) {
        keys[transform] = 0;
    }
    for (std::size_t cell = 0; cell < kFixed6Cells; ++cell) {
        const std::uint8_t* cellShifts = shifts + cell * kFixed6Transforms;
        const std::size_t keyShift = cell * 4U;
        for (std::size_t transform = 0; transform < kFixed6Transforms; ++transform) {
            keys[transform] |= static_cast<std::size_t>((bits >> cellShifts[transform]) & 0xFULL) << keyShift;
        }
    }
}

std::size_t CollectFixed6KeysScalar(std::uint64_t boardBits,
                                    const std::size_t* patternOffsets,
                                    const std::uint8_t* shifts,
                                    std::size_t patternCount,
                                    std::size_t* outKeys) {
    std::size_t written = 0;
    std::array<std::size_t, kFixed6Transforms> keys {};
    for (std::size_t pattern = 0; pattern < patternCount; ++pattern) {
        const std::size_t offset = patternOffsets[pattern];
        const std::uint8_t* patternShifts = shifts + pattern * kFixed6Transforms * kFixed6Cells;
        CollectPatternFixed6Keys(boardBits, patternShifts, keys.data());
        for (std::size_t transform = 0; transform < kFixed6Transforms; ++transform) {
            outKeys[written] = offset + keys[transform];
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
    float value = 0.0F;
    std::array<std::size_t, kFixed6Transforms> keys {};
    for (std::size_t pattern = 0; pattern < patternCount; ++pattern) {
        const std::size_t offset = patternOffsets[pattern];
        const std::uint8_t* patternShifts = shifts + pattern * kFixed6Transforms * kFixed6Cells;
        CollectPatternFixed6Keys(boardBits, patternShifts, keys.data());
        for (std::size_t transform = 0; transform < kFixed6Transforms; ++transform) {
            value += weights[offset + keys[transform]];
        }
    }
    return static_cast<double>(value);
}

std::size_t CollectFixed6KeysAndValueScalar(std::uint64_t boardBits,
                                            const float* weights,
                                            const std::size_t* patternOffsets,
                                            const std::uint8_t* shifts,
                                            std::size_t patternCount,
                                            std::size_t* outKeys,
                                            double& value) {
    std::size_t written = 0;
    std::array<std::size_t, kFixed6Transforms> keys {};
    float sum = 0.0F;
    for (std::size_t pattern = 0; pattern < patternCount; ++pattern) {
        const std::size_t offset = patternOffsets[pattern];
        const std::uint8_t* patternShifts = shifts + pattern * kFixed6Transforms * kFixed6Cells;
        CollectPatternFixed6Keys(boardBits, patternShifts, keys.data());
        for (std::size_t transform = 0; transform < kFixed6Transforms; ++transform) {
            const std::size_t key = offset + keys[transform];
            outKeys[written] = key;
            sum += weights[key];
            ++written;
        }
    }
    value = static_cast<double>(sum);
    return written;
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
    float value = 0.0F;
    for (float lane : lanes) {
        value += lane;
    }
    return static_cast<double>(value);
}

__attribute__((target("avx512f,avx512vl")))
std::size_t CollectFixed6KeysAndValueAvx512(std::uint64_t boardBits,
                                            const float* weights,
                                            const std::size_t* patternOffsets,
                                            const std::uint8_t* shifts,
                                            std::size_t patternCount,
                                            std::size_t* outKeys,
                                            double& value) {
    const __m512i bits = _mm512_set1_epi64(static_cast<long long>(boardBits));
    const __m512i mask = _mm512_set1_epi64(0xFULL);
    __m256 total = _mm256_setzero_ps();
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
        total = _mm256_add_ps(total, _mm512_i64gather_ps(key, weights, 4));
    }

    alignas(32) std::array<float, kFixed6Transforms> values {};
    _mm256_store_ps(values.data(), total);
    float sum = 0.0F;
    for (float lane : values) {
        sum += lane;
    }
    value = static_cast<double>(sum);
    return written;
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

std::size_t CollectFixed6KeysAndValue(std::uint64_t boardBits,
                                      const float* weights,
                                      const std::size_t* patternOffsets,
                                      const std::uint8_t* shifts,
                                      std::size_t patternCount,
                                      std::size_t* outKeys,
                                      double& value,
                                      KernelKind kind) {
#if defined(GAME2048_ENABLE_AVX512) && (defined(__x86_64__) || defined(_M_X64))
    if (ShouldUseAvx512(kind)) {
        return CollectFixed6KeysAndValueAvx512(boardBits, weights, patternOffsets, shifts,
                                               patternCount, outKeys, value);
    }
#else
    static_cast<void>(kind);
#endif
    return CollectFixed6KeysAndValueScalar(boardBits, weights, patternOffsets, shifts,
                                           patternCount, outKeys, value);
}

}  // namespace game2048::ai::ntuple_kernel
