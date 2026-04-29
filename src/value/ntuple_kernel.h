#pragma once

#include <cstddef>
#include <cstdint>

namespace game2048::ai::ntuple_kernel {

inline constexpr std::size_t kFixed6Cells = 6;
inline constexpr std::size_t kFixed6Transforms = 8;

enum class KernelKind {
    Auto,
    Scalar,
    Avx512,
};

bool IsAvx512Available();
const char* ActiveKernelName();

std::size_t CollectFixed6Keys(std::uint64_t boardBits,
                              const std::size_t* patternOffsets,
                              const std::uint8_t* shifts,
                              std::size_t patternCount,
                              std::size_t* outKeys,
                              KernelKind kind = KernelKind::Auto);

double EvaluateFixed6(std::uint64_t boardBits,
                      const float* weights,
                      const std::size_t* patternOffsets,
                      const std::uint8_t* shifts,
                      std::size_t patternCount,
                      KernelKind kind = KernelKind::Auto);

std::size_t CollectFixed6KeysAndValue(std::uint64_t boardBits,
                                      const float* weights,
                                      const std::size_t* patternOffsets,
                                      const std::uint8_t* shifts,
                                      std::size_t patternCount,
                                      std::size_t* outKeys,
                                      double& value,
                                      KernelKind kind = KernelKind::Auto);

}  // namespace game2048::ai::ntuple_kernel
