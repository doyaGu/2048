#include "transposition_table.h"

#include <algorithm>

namespace game2048::ai {

TranspositionTable::TranspositionTable(std::size_t entries)
    : entries_(std::max<std::size_t>(1024, entries)) {}

void TranspositionTable::Clear() {
    for (auto& entry : entries_) {
        entry = {};
    }
}

void TranspositionTable::NextGeneration() {
    ++generation_;
    if (generation_ == 0) {
        generation_ = 1;
        Clear();
    }
}

const TTEntry* TranspositionTable::Find(std::uint64_t key, std::uint16_t depth, NodeType nodeType) const {
    const std::size_t start = IndexFor(key);
    for (std::size_t probe = 0; probe < 8; ++probe) {
        const auto& entry = entries_[(start + probe) % entries_.size()];
        if (!entry.occupied) {
            return nullptr;
        }
        if (entry.key == key && entry.nodeType == nodeType && entry.depth >= depth) {
            return &entry;
        }
    }
    return nullptr;
}

void TranspositionTable::Store(std::uint64_t key, std::uint16_t depth, NodeType nodeType, double value, Direction bestMove) {
    const std::size_t start = IndexFor(key);
    std::size_t replacement = start;

    for (std::size_t probe = 0; probe < 8; ++probe) {
        const std::size_t index = (start + probe) % entries_.size();
        auto& entry = entries_[index];
        if (!entry.occupied || entry.key == key) {
            replacement = index;
            break;
        }
        if (entry.generation != generation_ || entry.depth < entries_[replacement].depth) {
            replacement = index;
        }
    }

    entries_[replacement] = {key, value, generation_, depth, nodeType, bestMove, true};
}

std::size_t TranspositionTable::IndexFor(std::uint64_t key) const {
    return static_cast<std::size_t>((key * 11400714819323198485ull) % entries_.size());
}

}  // namespace game2048::ai
