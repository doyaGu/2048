#include "game.h"

namespace game2048 {

Game::Game(std::uint64_t seed)
    : rng_(seed) {
    Reset(seed);
}

void Game::Reset(std::uint64_t seed) {
    board_ = Board();
    rng_.Seed(seed);
    score_ = 0;
    gameOver_ = false;
    reached2048_ = false;
    undo_.clear();
    rng_.Spawn(board_);
    rng_.Spawn(board_);
    UpdateTerminalFlags();
}

TurnResult Game::ApplyMove(Direction direction) {
    const GameSnapshot snapshot {board_, score_, gameOver_, reached2048_, rng_.State()};
    auto move = board_.ApplyMove(direction);

    TurnResult result;
    result.moved = move.changed;
    result.scoreDelta = move.scoreDelta;
    result.trace = std::move(move.trace);

    if (!result.moved) {
        return result;
    }

    undo_.push_back(snapshot);
    if (undo_.size() > static_cast<std::size_t>(kUndoCapacity)) {
        undo_.erase(undo_.begin());
    }

    score_ += result.scoreDelta;
    result.spawn = rng_.Spawn(board_);
    UpdateTerminalFlags();
    return result;
}

bool Game::Undo() {
    if (undo_.empty()) {
        return false;
    }

    const auto snapshot = undo_.back();
    undo_.pop_back();
    board_ = snapshot.board;
    score_ = snapshot.score;
    gameOver_ = snapshot.gameOver;
    reached2048_ = snapshot.reached2048;
    rng_.RestoreState(snapshot.rngState);
    return true;
}

const Board& Game::GetBoard() const {
    return board_;
}

std::uint32_t Game::Score() const {
    return score_;
}

bool Game::IsGameOver() const {
    return gameOver_;
}

bool Game::Reached2048() const {
    return reached2048_;
}

std::uint64_t Game::Seed() const {
    return rng_.SeedValue();
}

bool Game::CanUndo() const {
    return !undo_.empty();
}

void Game::PushUndo() {
    undo_.push_back({board_, score_, gameOver_, reached2048_, rng_.State()});
}

void Game::UpdateTerminalFlags() {
    reached2048_ = reached2048_ || board_.HasValue(2048);
    gameOver_ = !board_.CanMove();
}

}  // namespace game2048
