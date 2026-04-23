#include "ai/greedy.h"

#include <array>
#include <limits>

namespace game2048::ai {

namespace {

FastMoveResult ApplyMove(const FastBoard& board, Direction direction) {
    switch (direction) {
        case Direction::Left:
            return board.MoveLeft();
        case Direction::Right:
            return board.MoveRight();
        case Direction::Up:
            return board.MoveUp();
        case Direction::Down:
            return board.MoveDown();
    }
    return {};
}

}  // namespace

GreedyAgent::GreedyAgent()
    : evaluator_(Evaluator()) {}

GreedyAgent::GreedyAgent(Evaluator evaluator)
    : evaluator_(std::move(evaluator)) {}

MoveDecision GreedyAgent::ChooseMove(const FastBoard& board) const {
    constexpr std::array<Direction, 4> kDirections {
        Direction::Up, Direction::Left, Direction::Right, Direction::Down
    };

    MoveDecision best;
    best.value = -std::numeric_limits<double>::infinity();

    SearchStats stats;
    for (Direction direction : kDirections) {
        const auto move = ApplyMove(board, direction);
        if (!move.changed) {
            continue;
        }

        ++stats.nodes;
        const double score = static_cast<double>(move.scoreDelta) +
                             evaluator_.Evaluate(FastBoard(move.board));
        if (!best.valid || score > best.value) {
            best.valid = true;
            best.direction = direction;
            best.value = score;
        }
    }

    best.stats = stats;
    best.stats.evaluation = best.value;
    return best;
}

}  // namespace game2048::ai
