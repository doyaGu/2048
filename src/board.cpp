#include "board.h"

#include <algorithm>
#include <sstream>

namespace game2048 {

namespace {

struct LineTile {
    int value = 0;
    CellCoord from {};
};

using LinePositions = std::array<CellCoord, kBoardSize>;

LinePositions PositionsForLine(Direction direction, int line) {
    LinePositions positions {};
    for (int offset = 0; offset < kBoardSize; ++offset) {
        switch (direction) {
            case Direction::Left:
                positions[offset] = {line, offset};
                break;
            case Direction::Right:
                positions[offset] = {line, kBoardSize - 1 - offset};
                break;
            case Direction::Up:
                positions[offset] = {offset, line};
                break;
            case Direction::Down:
                positions[offset] = {kBoardSize - 1 - offset, line};
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
    for (int row = 0; row < kBoardSize; ++row) {
        for (int col = 0; col < kBoardSize; ++col) {
            cells[row * kBoardSize + col] = rows[row][col];
        }
    }
    return Board(cells);
}

int Board::At(int row, int col) const {
    return cells_[row * kBoardSize + col];
}

void Board::Set(int row, int col, int value) {
    cells_[row * kBoardSize + col] = value;
}

const Board::Storage& Board::Cells() const {
    return cells_;
}

MoveResult Board::ApplyMove(Direction direction) {
    MoveResult result;
    Storage next = cells_;

    for (int line = 0; line < kBoardSize; ++line) {
        const auto positions = PositionsForLine(direction, line);

        std::vector<LineTile> tiles;
        tiles.reserve(kBoardSize);
        for (const auto& position : positions) {
            const int value = At(position.row, position.col);
            if (value != 0) {
                tiles.push_back({value, position});
            }
        }

        int writeIndex = 0;
        for (std::size_t index = 0; index < tiles.size();) {
            const auto& current = tiles[index];
            if (index + 1 < tiles.size() && tiles[index + 1].value == current.value) {
                const int mergedValue = current.value * 2;
                const auto destination = positions[writeIndex];

                next[destination.row * kBoardSize + destination.col] = mergedValue;
                result.scoreDelta += static_cast<std::uint32_t>(mergedValue);
                result.trace.merges.push_back({destination, mergedValue});

                const auto& other = tiles[index + 1];
                if (current.from.row != destination.row || current.from.col != destination.col || mergedValue != current.value) {
                    result.trace.moves.push_back({current.from, destination, current.value, true});
                }
                if (other.from.row != destination.row || other.from.col != destination.col || mergedValue != other.value) {
                    result.trace.moves.push_back({other.from, destination, other.value, true});
                }

                ++writeIndex;
                index += 2;
            } else {
                const auto destination = positions[writeIndex];
                next[destination.row * kBoardSize + destination.col] = current.value;
                if (current.from.row != destination.row || current.from.col != destination.col) {
                    result.trace.moves.push_back({current.from, destination, current.value, false});
                }
                ++writeIndex;
                ++index;
            }
        }

        for (; writeIndex < kBoardSize; ++writeIndex) {
            const auto destination = positions[writeIndex];
            next[destination.row * kBoardSize + destination.col] = 0;
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
