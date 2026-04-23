#include "rng.h"

namespace game2048 {

Random::Random(std::uint64_t seed)
    : seed_(seed), engine_(seed) {}

void Random::Seed(std::uint64_t seed) {
    seed_ = seed;
    engine_.seed(seed);
}

std::uint64_t Random::SeedValue() const {
    return seed_;
}

Random::EngineState Random::State() const {
    return engine_;
}

void Random::RestoreState(const EngineState& state) {
    engine_ = state;
}

std::uint32_t Random::NextU32() {
    return static_cast<std::uint32_t>(engine_());
}

std::size_t Random::NextIndex(std::size_t upperExclusive) {
    if (upperExclusive == 0) {
        return 0;
    }
    std::uniform_int_distribution<std::size_t> distribution(0, upperExclusive - 1);
    return distribution(engine_);
}

double Random::NextUnit() {
    std::uniform_real_distribution<double> distribution(0.0, 1.0);
    return distribution(engine_);
}

int Random::SampleSpawnValue() {
    return NextUnit() < kSpawnProbability4 ? 4 : 2;
}

SpawnEvent Random::Spawn(Board& board) {
    const auto empties = board.EmptyCells();
    if (empties.empty()) {
        return {};
    }

    const auto cell = empties[NextIndex(empties.size())];
    const int value = SampleSpawnValue();
    board.Set(cell.row, cell.col, value);
    return {cell, value};
}

int Random::SpawnOnFastBoard(FastBoard& board) {
    const auto empties = board.EmptyIndices();
    if (empties.empty()) {
        return -1;
    }

    const int index = empties[NextIndex(empties.size())];
    const int value = SampleSpawnValue();
    const int rank = value == 2 ? 1 : 2;
    board.SetRank(index, rank);
    return index;
}

}  // namespace game2048
