#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "board.h"

namespace game2048 {

struct FastMoveResult {
    std::uint64_t board = 0;
    std::uint32_t scoreDelta = 0;
    bool changed = false;
};

class FastBoard {
public:
    FastBoard();
    explicit FastBoard(std::uint64_t bits);

    static FastBoard FromReference(const Board& board);

    std::uint64_t Bits() const;
    int GetRank(int index) const;
    int GetValue(int index) const;
    bool IsEmpty(int index) const;
    void SetRank(int index, int rank);

    FastMoveResult MoveLeft() const;
    FastMoveResult MoveRight() const;
    FastMoveResult MoveUp() const;
    FastMoveResult MoveDown() const;

    bool CanMove() const;
    int CountEmpty() const;
    int MaxRank() const;
    int MaxTile() const;

    FastBoard Transpose() const;
    FastBoard Mirror() const;

    std::uint64_t Key() const;
    Board ToReference() const;
    std::vector<int> EmptyIndices() const;

    bool operator==(const FastBoard& other) const;
    bool operator!=(const FastBoard& other) const;

    std::string ToString() const;

private:
    std::uint64_t bits_ = 0;
};

}  // namespace game2048
