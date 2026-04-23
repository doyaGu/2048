#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "config.h"

namespace game2048 {

enum class Direction {
    Up,
    Down,
    Left,
    Right
};

struct CellCoord {
    int row = 0;
    int col = 0;
};

struct TileMove {
    CellCoord from {};
    CellCoord to {};
    int value = 0;
    bool merged = false;
};

struct MergeEvent {
    CellCoord cell {};
    int value = 0;
};

struct MoveTrace {
    std::vector<TileMove> moves;
    std::vector<MergeEvent> merges;
};

struct SpawnEvent {
    CellCoord cell {};
    int value = 0;
};

struct MoveResult {
    bool changed = false;
    std::uint32_t scoreDelta = 0;
    MoveTrace trace {};
};

class Board {
public:
    using Storage = std::array<int, kCellCount>;

    Board();
    explicit Board(const Storage& cells);

    static Board FromRows(const std::array<std::array<int, kBoardSize>, kBoardSize>& rows);

    int At(int row, int col) const;
    void Set(int row, int col, int value);
    const Storage& Cells() const;

    MoveResult ApplyMove(Direction direction);
    bool CanMove() const;
    bool HasEmpty() const;
    int EmptyCount() const;
    int MaxTile() const;
    bool HasValue(int value) const;
    std::vector<CellCoord> EmptyCells() const;

    bool operator==(const Board& other) const;
    bool operator!=(const Board& other) const;

    std::string ToString() const;

private:
    Storage cells_ {};
};

}  // namespace game2048
