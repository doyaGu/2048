#include "core/board.h"

#include <algorithm>
#include <cassert>
#include <sstream>

namespace game2048 {

namespace {

struct LineTile {
    int value = 0;
    CellCoord from {};
};

using LinePositions = std::array<CellCoord, kBoardSize>;

std::size_t CellIndex(int row, int col) {
    assert(row >= 0 && row < kBoardSize);
    assert(col >= 0 && col < kBoardSize);

    const auto rowIndex = static_cast<std::size_t>(row);
    const auto colIndex = static_cast<std::size_t>(col);
    return rowIndex * static_cast<std::size_t>(kBoardSize) + colIndex;
}

bool SameCell(CellCoord lhs, CellCoord rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

LinePositions PositionsForLine(Direction direction, int line) {
    LinePositions positions {};
    for (std::size_t offset = 0; offset < positions.size(); ++offset) {
        const int coordinate = static_cast<int>(offset);
        switch (direction) {
            case Direction::Left:
                positions[offset] = {line, coordinate};
                break;
            case Direction::Right:
                positions[offset] = {line, kBoardSize - 1 - coordinate};
                break;
            case Direction::Up:
                positions[offset] = {coordinate, line};
                break;
            case Direction::Down:
                positions[offset] = {kBoardSize - 1 - coordinate, line};
                break;
        }
    }
    return positions;
}

}  // namespace

Board::Board() = default;

Board::Board(const Storage& cells)
    : cells_(cells) {}

Board Board::FromRows(const std::array<std::array<int, kBoardSize>, kBoardSize>& rows) {
    Storage cells {};
    for (std::size_t row = 0; row < rows.size(); ++row) {
        for (std::size_t col = 0; col < rows[row].size(); ++col) {
            cells[row * rows[row].size() + col] = rows[row][col];
        }
    }
    return Board(cells);
}

int Board::At(int row, int col) const {
    return cells_[CellIndex(row, col)];
}

void Board::Set(int row, int col, int value) {
    cells_[CellIndex(row, col)] = value;
}

const Board::Storage& Board::Cells() const {
    return cells_;
}

MoveResult Board::ApplyMove(Direction direction) {
    MoveResult result;
    Storage next = cells_;

    for (std::size_t line = 0; line < static_cast<std::size_t>(kBoardSize); ++line) {
        const auto positions = PositionsForLine(direction, static_cast<int>(line));

        std::vector<LineTile> tiles;
        tiles.reserve(kBoardSize);
        for (const auto& position : positions) {
            const int value = At(position.row, position.col);
            if (value != 0) {
                tiles.push_back({value, position});
            }
        }

        std::size_t writeIndex = 0;
        for (std::size_t index = 0; index < tiles.size();) {
            const auto& current = tiles[index];
            if (index + 1 < tiles.size() && tiles[index + 1].value == current.value) {
                const int mergedValue = current.value * 2;
                const auto destination = positions[writeIndex];

                next[CellIndex(destination.row, destination.col)] = mergedValue;
                result.scoreDelta += static_cast<std::uint32_t>(mergedValue);
                result.trace.merges.push_back({destination, mergedValue});

                const auto& other = tiles[index + 1];
                if (!SameCell(current.from, destination) || mergedValue != current.value) {
                    result.trace.moves.push_back({current.from, destination, current.value, true});
                }
                if (!SameCell(other.from, destination) || mergedValue != other.value) {
                    result.trace.moves.push_back({other.from, destination, other.value, true});
                }

                ++writeIndex;
                index += 2;
            } else {
                const auto destination = positions[writeIndex];
                next[CellIndex(destination.row, destination.col)] = current.value;
                if (!SameCell(current.from, destination)) {
                    result.trace.moves.push_back({current.from, destination, current.value, false});
                }
                ++writeIndex;
                ++index;
            }
        }

        for (; writeIndex < positions.size(); ++writeIndex) {
            const auto destination = positions[writeIndex];
            next[CellIndex(destination.row, destination.col)] = 0;
        }
    }

    result.changed = next != cells_;
    if (result.changed) {
        cells_ = next;
    } else {
        result.scoreDelta = 0;
        result.trace.moves.clear();
        result.trace.merges.clear();
    }
    return result;
}

bool Board::CanMove() const {
    if (HasEmpty()) {
        return true;
    }

    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            const int value = At(row, col);
            if (row + 1 < kBoardSize && value == At(row + 1, col)) {
                return true;
            }
            if (col + 1 < kBoardSize && value == At(row, col + 1)) {
                return true;
            }
        }
    }
    return false;
}

bool Board::HasEmpty() const {
    return std::any_of(cells_.begin(), cells_.end(), [](int value) { return value == 0; });
}

int Board::EmptyCount() const {
    return static_cast<int>(std::count(cells_.begin(), cells_.end(), 0));
}

int Board::MaxTile() const {
    return *std::max_element(cells_.begin(), cells_.end());
}

bool Board::HasValue(int value) const {
    return std::find(cells_.begin(), cells_.end(), value) != cells_.end();
}

std::vector<CellCoord> Board::EmptyCells() const {
    std::vector<CellCoord> cells;
    cells.reserve(kCellCount);
    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            if (At(row, col) == 0) {
                cells.push_back({row, col});
            }
        }
    }
    return cells;
}

bool Board::operator==(const Board& other) const {
    return cells_ == other.cells_;
}

bool Board::operator!=(const Board& other) const {
    return !(*this == other);
}

std::string Board::ToString() const {
    std::ostringstream oss;
    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            if (col != 0) {
                oss << ' ';
            }
            oss << At(row, col);
        }
        if (row + 1 < kBoardSize) {
            oss << '\n';
        }
    }
    return oss.str();
}

}  // namespace game2048
