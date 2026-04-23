#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include "core/board.h"
#include "core/board_fast.h"

namespace game2048 {

class Random {
public:
    using EngineState = std::mt19937_64;

    explicit Random(std::uint64_t seed = kDefaultSeed);

    void Seed(std::uint64_t seed);
    std::uint64_t SeedValue() const;
    EngineState State() const;
    void RestoreState(const EngineState& state);
    std::uint32_t NextU32();
    std::size_t NextIndex(std::size_t upperExclusive);
    double NextUnit();

    int SampleSpawnValue();
    SpawnEvent Spawn(Board& board);
    int SpawnOnFastBoard(FastBoard& board);

private:
    std::uint64_t seed_ = 0;
    std::mt19937_64 engine_;
};

}  // namespace game2048
