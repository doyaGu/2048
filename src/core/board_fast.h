#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/board.h"

namespace game2048 {

struct FastMoveResult {
    std::uint64_t board = 0;
    std::uint32_t scoreDelta = 0;
    bool changed = false;
};

class PackedBoard;
struct PackedMoveResult;
bool PackedBoardMoveTablesInitialized();

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
    void TdlOrderMoves(std::array<FastMoveResult, 4>& moves) const;

    bool CanMove() const;
    int CountEmpty() const;
    int MaxRank() const;
    int MaxTile() const;

    FastBoard Transpose() const;
    FastBoard Mirror() const;
    FastBoard TransformD4(int transform) const;
    std::uint64_t CanonicalKey() const;

    std::uint64_t Key() const;
    Board ToReference() const;
    std::size_t CollectEmptyIndices(std::array<int, kCellCount>& indices) const;
    std::vector<int> EmptyIndices() const;

    bool operator==(const FastBoard& other) const;
    bool operator!=(const FastBoard& other) const;

    std::string ToString() const;

private:
    std::uint64_t bits_ = 0;
};

class PackedBoard {
public:
    PackedBoard();
    explicit PackedBoard(std::uint64_t raw, std::uint16_t ext = 0);
    explicit PackedBoard(const FastBoard& board);

    std::uint64_t Raw() const;
    std::uint16_t Ext() const;
    int GetRank(int index) const;
    void SetRank(int index, int rank);

    PackedMoveResult MoveLeft() const;
    PackedMoveResult MoveRight() const;
    PackedMoveResult MoveUp() const;
    PackedMoveResult MoveDown() const;

    bool CanMove() const;
    int CountEmpty() const;
    int MaxRank() const;
    int MaxTile() const;

    PackedBoard Transpose() const;
    PackedBoard Mirror() const;
    PackedBoard TransformD4(int transform) const;
    std::uint64_t CanonicalKey64() const;
    FastBoard ToFastBoardClamped(int maxRank = 15) const;

    bool operator==(const PackedBoard& other) const;
    bool operator!=(const PackedBoard& other) const;

private:
    std::uint64_t raw_ = 0;
    std::uint16_t ext_ = 0;
};

struct PackedMoveResult {
    PackedBoard board;
    std::uint32_t scoreDelta = 0;
    bool changed = false;
};

}  // namespace game2048
