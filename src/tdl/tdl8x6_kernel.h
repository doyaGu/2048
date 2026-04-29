#pragma once

#include <cstdint>
#include <limits>

#include "core/board_fast.h"
#include "value/ntuple.h"

namespace game2048::ai {

struct Tdl8x6BestMove {
    static constexpr double kInvalidValue = -std::numeric_limits<double>::infinity();

    std::uint64_t board = 0;
    double value = kInvalidValue;
    double estimate = 0.0;
    std::uint32_t scoreDelta = 0;

    bool Valid() const { return value != kInvalidValue; }
};

class Tdl8x6Kernel {
public:
    static bool Supports(NtupleFixed6View view);
    static bool Supports(NtupleMutableFixed6View view);

    explicit Tdl8x6Kernel(NtupleFixed6View view);
    explicit Tdl8x6Kernel(NtupleMutableFixed6View view);

    double Evaluate(std::uint64_t bits) const;
    NtupleUpdateStats Update(std::uint64_t bits, double target, double alpha) const;
    NtupleUpdateStats UpdateKnownValue(std::uint64_t bits, double before, double target, double alpha) const;
    Tdl8x6BestMove ChooseBest(const FastBoard& board) const;

private:
    float* weights_ = nullptr;
    const std::size_t* patternOffsets_ = nullptr;
    const std::uint8_t* shifts_ = nullptr;
    bool useFixed6Dispatch_ = false;
};

}  // namespace game2048::ai
