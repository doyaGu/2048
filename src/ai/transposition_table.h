#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "../board.h"

namespace game2048::ai {

enum class NodeType {
    Max,
    Chance
};

struct TTEntry {
    std::uint64_t key = 0;
    double value = 0.0;
    std::uint32_t generation = 0;
    std::uint16_t depth = 0;
    NodeType nodeType = NodeType::Max;
    Direction bestMove = Direction::Left;
    bool occupied = false;
};

class TranspositionTable {
public:
    explicit TranspositionTable(std::size_t entries = kDefaultTranspositionTableEntries);

    void Clear();
    void NextGeneration();
    const TTEntry* Find(std::uint64_t key, std::uint16_t depth, NodeType nodeType) const;
    void Store(std::uint64_t key, std::uint16_t depth, NodeType nodeType, double value, Direction bestMove);

private:
    std::size_t IndexFor(std::uint64_t key) const;

    std::vector<TTEntry> entries_;
    std::uint32_t generation_ = 1;
};

}  // namespace game2048::ai
