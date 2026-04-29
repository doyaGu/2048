#include "value/tdl8x6_kernel.h"

#include <array>
#include <stdexcept>

#include "value/ntuple_kernel.h"

#if defined(GAME2048_ENABLE_AVX512) && (defined(__x86_64__) || defined(_M_X64))
#include <immintrin.h>
#endif

namespace game2048::ai {

namespace {

constexpr std::size_t kPatternCount = 8;
constexpr std::size_t kEntriesPerPattern = 1ULL << 24U;

std::uint64_t FlipBits(std::uint64_t bits) {
    return ((bits & 0x000000000000FFFFULL) << 48U) |
           ((bits & 0x00000000FFFF0000ULL) << 16U) |
           ((bits & 0x0000FFFF00000000ULL) >> 16U) |
           ((bits & 0xFFFF000000000000ULL) >> 48U);
}

std::uint64_t TransposeBits(std::uint64_t bits) {
    std::uint64_t buffer = (bits ^ (bits >> 12U)) & 0x0000F0F00000F0F0ULL;
    bits ^= buffer ^ (buffer << 12U);
    buffer = (bits ^ (bits >> 24U)) & 0x00000000FF00FF00ULL;
    bits ^= buffer ^ (buffer << 24U);
    return bits;
}

void BuildTransforms(std::uint64_t bits, std::array<std::uint64_t, 8>& transforms) {
    std::uint64_t iso = bits;
    transforms[0] = iso;
    iso = FlipBits(iso);
    transforms[4] = iso;
    iso = TransposeBits(iso);
    transforms[1] = iso;
    iso = FlipBits(iso);
    transforms[7] = iso;
    iso = TransposeBits(iso);
    transforms[2] = iso;
    iso = FlipBits(iso);
    transforms[6] = iso;
    iso = TransposeBits(iso);
    transforms[3] = iso;
    iso = FlipBits(iso);
    transforms[5] = iso;
}

template <std::size_t Pattern>
constexpr std::size_t PatternOffset() {
    return Pattern * kEntriesPerPattern;
}

[[maybe_unused]] std::size_t RuntimePatternOffset(std::size_t pattern) {
    return pattern * kEntriesPerPattern;
}

template <std::size_t Pattern>
std::size_t Tdl8x6PatternKey(std::uint64_t bits) {
    if constexpr (Pattern == 0U) {
        return static_cast<std::size_t>((bits & 0x0000000000000FFFULL) |
                                        ((bits >> 4U) & 0x0000000000FFF000ULL));
    } else if constexpr (Pattern == 1U) {
        return static_cast<std::size_t>((bits >> 16U) & 0x0000000000FFFFFFULL);
    } else if constexpr (Pattern == 2U) {
        return static_cast<std::size_t>(bits & 0x0000000000FFFFFFULL);
    } else if constexpr (Pattern == 3U) {
        return static_cast<std::size_t>(((bits >> 8U) & 0x00000000000FFFFFULL) |
                                        ((bits >> 16U) & 0x0000000000F00000ULL));
    } else if constexpr (Pattern == 4U) {
        return static_cast<std::size_t>((bits & 0x0000000000000FFFULL) |
                                        ((bits >> 8U) & 0x000000000000F000ULL) |
                                        ((bits >> 20U) & 0x0000000000FF0000ULL));
    } else if constexpr (Pattern == 5U) {
        return static_cast<std::size_t>((bits >> 12U) & 0x0000000000FFFFFFULL);
    } else if constexpr (Pattern == 6U) {
        return static_cast<std::size_t>(((bits >> 4U) & 0x000000000000000FULL) |
                                        ((bits >> 8U) & 0x0000000000FFFFF0ULL));
    } else {
        return static_cast<std::size_t>((bits & 0x00000000000000FFULL) |
                                        ((bits >> 8U) & 0x0000000000000F00ULL) |
                                        ((bits >> 20U) & 0x0000000000FFF000ULL));
    }
}

template <std::size_t Pattern, typename Visitor>
void VisitPatternKeys(const std::array<std::uint64_t, 8>& transforms, Visitor& visitor) {
    constexpr std::size_t offset = PatternOffset<Pattern>();
    visitor(offset + Tdl8x6PatternKey<Pattern>(transforms[0]));
    visitor(offset + Tdl8x6PatternKey<Pattern>(transforms[1]));
    visitor(offset + Tdl8x6PatternKey<Pattern>(transforms[2]));
    visitor(offset + Tdl8x6PatternKey<Pattern>(transforms[3]));
    visitor(offset + Tdl8x6PatternKey<Pattern>(transforms[4]));
    visitor(offset + Tdl8x6PatternKey<Pattern>(transforms[5]));
    visitor(offset + Tdl8x6PatternKey<Pattern>(transforms[6]));
    visitor(offset + Tdl8x6PatternKey<Pattern>(transforms[7]));
}

template <typename Visitor>
void VisitAllPatternKeys(const std::array<std::uint64_t, 8>& transforms, Visitor& visitor) {
    VisitPatternKeys<0>(transforms, visitor);
    VisitPatternKeys<1>(transforms, visitor);
    VisitPatternKeys<2>(transforms, visitor);
    VisitPatternKeys<3>(transforms, visitor);
    VisitPatternKeys<4>(transforms, visitor);
    VisitPatternKeys<5>(transforms, visitor);
    VisitPatternKeys<6>(transforms, visitor);
    VisitPatternKeys<7>(transforms, visitor);
}

std::size_t CollectFastFixed6KeysAndValue(std::uint64_t bits, const float* weights,
                                          std::array<std::uint32_t, 64>& keys, float& value) {
    std::array<std::uint64_t, 8> transforms {};
    BuildTransforms(bits, transforms);
    std::size_t written = 0;
    auto collect = [&](std::size_t fullKey) {
        const std::uint32_t key = static_cast<std::uint32_t>(fullKey);
        keys[written++] = key;
        value += weights[key];
    };
    VisitAllPatternKeys(transforms, collect);
    return written;
}

bool IsCanonicalFixed6View(bool valid, const float* weights, const std::size_t* patternOffsets,
                           const std::uint8_t* shifts, std::size_t patternCount) {
    if (!valid || weights == nullptr || patternOffsets == nullptr || shifts == nullptr ||
        patternCount != kPatternCount) {
        return false;
    }
    for (std::size_t index = 0; index < kPatternCount; ++index) {
        if (patternOffsets[index] != index * kEntriesPerPattern) {
            return false;
        }
    }
    return true;
}

bool IsCanonicalView(NtupleFixed6View view) {
    return IsCanonicalFixed6View(view.valid, view.weights, view.patternOffsets, view.shifts, view.patternCount);
}

bool IsCanonicalView(NtupleMutableFixed6View view) {
    return IsCanonicalFixed6View(view.valid, view.weights, view.patternOffsets, view.shifts, view.patternCount);
}

void ValidateCanonicalView(NtupleFixed6View view) {
    if (!IsCanonicalView(view)) {
        throw std::invalid_argument("TDL 8x6 kernel requires a valid canonical fixed 8x6 view");
    }
}

void ValidateCanonicalView(NtupleMutableFixed6View view) {
    if (!IsCanonicalView(view)) {
        throw std::invalid_argument("TDL 8x6 kernel requires a valid canonical fixed 8x6 view");
    }
}

#if defined(GAME2048_ENABLE_AVX512) && (defined(__x86_64__) || defined(_M_X64))
__attribute__((target("avx512f,avx512vl")))
__m512i Tdl8x6PatternKeyAvx512(__m512i bits, std::size_t pattern) {
    if (pattern == 0U) {
        return _mm512_or_si512(_mm512_and_si512(bits, _mm512_set1_epi64(0x0000000000000FFFLL)),
                               _mm512_and_si512(_mm512_srli_epi64(bits, 4),
                                                _mm512_set1_epi64(0x0000000000FFF000LL)));
    }
    if (pattern == 1U) {
        return _mm512_and_si512(_mm512_srli_epi64(bits, 16), _mm512_set1_epi64(0x0000000000FFFFFFLL));
    }
    if (pattern == 2U) {
        return _mm512_and_si512(bits, _mm512_set1_epi64(0x0000000000FFFFFFLL));
    }
    if (pattern == 3U) {
        return _mm512_or_si512(_mm512_and_si512(_mm512_srli_epi64(bits, 8),
                                                _mm512_set1_epi64(0x00000000000FFFFFLL)),
                               _mm512_and_si512(_mm512_srli_epi64(bits, 16),
                                                _mm512_set1_epi64(0x0000000000F00000LL)));
    }
    if (pattern == 4U) {
        return _mm512_or_si512(
            _mm512_or_si512(_mm512_and_si512(bits, _mm512_set1_epi64(0x0000000000000FFFLL)),
                            _mm512_and_si512(_mm512_srli_epi64(bits, 8),
                                             _mm512_set1_epi64(0x000000000000F000LL))),
            _mm512_and_si512(_mm512_srli_epi64(bits, 20), _mm512_set1_epi64(0x0000000000FF0000LL)));
    }
    if (pattern == 5U) {
        return _mm512_and_si512(_mm512_srli_epi64(bits, 12), _mm512_set1_epi64(0x0000000000FFFFFFLL));
    }
    if (pattern == 6U) {
        return _mm512_or_si512(_mm512_and_si512(_mm512_srli_epi64(bits, 4),
                                                _mm512_set1_epi64(0x000000000000000FLL)),
                               _mm512_and_si512(_mm512_srli_epi64(bits, 8),
                                                _mm512_set1_epi64(0x0000000000FFFFF0LL)));
    }
    return _mm512_or_si512(
        _mm512_or_si512(_mm512_and_si512(bits, _mm512_set1_epi64(0x00000000000000FFLL)),
                        _mm512_and_si512(_mm512_srli_epi64(bits, 8),
                                         _mm512_set1_epi64(0x0000000000000F00LL))),
        _mm512_and_si512(_mm512_srli_epi64(bits, 20), _mm512_set1_epi64(0x0000000000FFF000LL)));
}

__attribute__((target("avx512f,avx512vl")))
double EvaluateStaticAvx512(std::uint64_t bits, const float* weights) {
    std::array<std::uint64_t, 8> transforms {};
    BuildTransforms(bits, transforms);
    const __m512i boards = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(transforms.data()));
    __m256 total = _mm256_setzero_ps();

    for (std::size_t pattern = 0; pattern < kPatternCount; ++pattern) {
        __m512i key = Tdl8x6PatternKeyAvx512(boards, pattern);
        key = _mm512_add_epi64(key, _mm512_set1_epi64(static_cast<long long>(RuntimePatternOffset(pattern))));
        total = _mm256_add_ps(total, _mm512_i64gather_ps(key, weights, 4));
    }

    alignas(32) std::array<float, 8> lanes {};
    _mm256_store_ps(lanes.data(), total);
    float value = 0.0F;
    for (float lane : lanes) {
        value += lane;
    }
    return static_cast<double>(value);
}

__attribute__((target("avx512f,avx512vl")))
void ApplyStaticDeltaAvx512(std::uint64_t bits, float* weights, float delta) {
    std::array<std::uint64_t, 8> transforms {};
    BuildTransforms(bits, transforms);
    const __m512i boards = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(transforms.data()));
    alignas(64) std::array<std::uint64_t, 8> lanes {};

    for (std::size_t pattern = 0; pattern < kPatternCount; ++pattern) {
        __m512i key = Tdl8x6PatternKeyAvx512(boards, pattern);
        key = _mm512_add_epi64(key, _mm512_set1_epi64(static_cast<long long>(RuntimePatternOffset(pattern))));
        _mm512_store_si512(reinterpret_cast<__m512i*>(lanes.data()), key);
        for (std::uint64_t lane : lanes) {
            weights[static_cast<std::size_t>(lane)] += delta;
        }
    }
}
#endif

}  // namespace

bool Tdl8x6Kernel::Supports(NtupleMutableFixed6View view) {
    return IsCanonicalView(view);
}

bool Tdl8x6Kernel::Supports(NtupleFixed6View view) {
    return IsCanonicalView(view);
}

Tdl8x6Kernel::Tdl8x6Kernel(NtupleFixed6View view)
    : weights_(const_cast<float*>(view.weights)),
      patternOffsets_(view.patternOffsets),
      shifts_(view.shifts),
      useFixed6Dispatch_(ntuple_kernel::IsAvx512Available()) {
    ValidateCanonicalView(view);
}

Tdl8x6Kernel::Tdl8x6Kernel(NtupleMutableFixed6View view)
    : weights_(view.weights),
      patternOffsets_(view.patternOffsets),
      shifts_(view.shifts),
      useFixed6Dispatch_(ntuple_kernel::IsAvx512Available()) {
    ValidateCanonicalView(view);
}

double Tdl8x6Kernel::Evaluate(std::uint64_t bits) const {
    if (useFixed6Dispatch_) {
#if defined(GAME2048_ENABLE_AVX512) && (defined(__x86_64__) || defined(_M_X64))
        return EvaluateStaticAvx512(bits, weights_);
#else
        return ntuple_kernel::EvaluateFixed6(bits, weights_, patternOffsets_, shifts_, kPatternCount);
#endif
    }
    std::array<std::uint64_t, 8> transforms {};
    BuildTransforms(bits, transforms);
    float value = 0.0F;
    auto accumulate = [&](std::size_t key) {
        value += weights_[key];
    };
    VisitAllPatternKeys(transforms, accumulate);
    return static_cast<double>(value);
}

NtupleUpdateStats Tdl8x6Kernel::Update(std::uint64_t bits, double target, double alpha) const {
    if (useFixed6Dispatch_) {
        constexpr std::size_t keyCount = kPatternCount * 8U;
        const double before = Evaluate(bits);
        const float error = static_cast<float>(target) - static_cast<float>(before);
        const float delta = static_cast<float>(alpha) * error / static_cast<float>(keyCount);
#if defined(GAME2048_ENABLE_AVX512) && (defined(__x86_64__) || defined(_M_X64))
        ApplyStaticDeltaAvx512(bits, weights_, delta);
#else
        std::array<std::size_t, keyCount> keys {};
        const std::size_t written =
            ntuple_kernel::CollectFixed6Keys(bits, patternOffsets_, shifts_, kPatternCount, keys.data());
        for (std::size_t index = 0; index < written; ++index) {
            weights_[keys[index]] += delta;
        }
#endif
        return {before, 0.0, error};
    }

    std::array<std::uint32_t, 64> keys {};
    float before = 0.0F;
    const std::size_t keyCount = CollectFastFixed6KeysAndValue(bits, weights_, keys, before);
    const float error = static_cast<float>(target) - before;
    const float delta = static_cast<float>(alpha) * error / static_cast<float>(keyCount);
    for (std::size_t index = 0; index < keyCount; ++index) {
        const std::size_t key = keys[index];
        weights_[key] += delta;
    }
    return {before, 0.0, error};
}

NtupleUpdateStats Tdl8x6Kernel::UpdateKnownValue(std::uint64_t bits, double before, double target,
                                                 double alpha) const {
    constexpr std::size_t keyCount = kPatternCount * 8U;
    const float error = static_cast<float>(target) - static_cast<float>(before);
    const float delta = static_cast<float>(alpha) * error / static_cast<float>(keyCount);
    if (useFixed6Dispatch_) {
#if defined(GAME2048_ENABLE_AVX512) && (defined(__x86_64__) || defined(_M_X64))
        ApplyStaticDeltaAvx512(bits, weights_, delta);
#else
        std::array<std::size_t, keyCount> keys {};
        const std::size_t written =
            ntuple_kernel::CollectFixed6Keys(bits, patternOffsets_, shifts_, kPatternCount, keys.data());
        for (std::size_t index = 0; index < written; ++index) {
            weights_[keys[index]] += delta;
        }
#endif
        return {before, 0.0, error};
    }

    std::array<std::uint64_t, 8> transforms {};
    BuildTransforms(bits, transforms);
    auto apply = [&](std::size_t key) {
        weights_[key] += delta;
    };
    VisitAllPatternKeys(transforms, apply);
    return {before, 0.0, error};
}

Tdl8x6BestMove Tdl8x6Kernel::ChooseBest(const FastBoard& board) const {
    Tdl8x6BestMove best;
    std::array<FastMoveResult, 4> moves {};
    board.TdlOrderMoves(moves);

    if (moves[0].changed) {
        const double estimate = Evaluate(moves[0].board);
        const double value = static_cast<double>(moves[0].scoreDelta) + estimate;
        if (value > best.value) {
            best.board = moves[0].board;
            best.scoreDelta = moves[0].scoreDelta;
            best.value = value;
            best.estimate = estimate;
        }
    }
    if (moves[1].changed) {
        const double estimate = Evaluate(moves[1].board);
        const double value = static_cast<double>(moves[1].scoreDelta) + estimate;
        if (value > best.value) {
            best.board = moves[1].board;
            best.scoreDelta = moves[1].scoreDelta;
            best.value = value;
            best.estimate = estimate;
        }
    }
    if (moves[2].changed) {
        const double estimate = Evaluate(moves[2].board);
        const double value = static_cast<double>(moves[2].scoreDelta) + estimate;
        if (value > best.value) {
            best.board = moves[2].board;
            best.scoreDelta = moves[2].scoreDelta;
            best.value = value;
            best.estimate = estimate;
        }
    }
    if (moves[3].changed) {
        const double estimate = Evaluate(moves[3].board);
        const double value = static_cast<double>(moves[3].scoreDelta) + estimate;
        if (value > best.value) {
            best.board = moves[3].board;
            best.scoreDelta = moves[3].scoreDelta;
            best.value = value;
            best.estimate = estimate;
        }
    }
    return best;
}

}  // namespace game2048::ai
