#include "app/game_controller.h"

#include <fstream>
#include <utility>

namespace game2048 {

std::filesystem::path GameController::DefaultBestScorePath() {
    return std::filesystem::current_path() / "best_score.txt";
}

std::uint32_t GameController::LoadBestScore() const {
    std::ifstream input(bestScorePath_);
    std::uint32_t value = 0;
    input >> value;
    return value;
}

void GameController::SaveBestScore(std::uint32_t score) const {
    std::ofstream output(bestScorePath_, std::ios::trunc);
    output << score;
}

GameController::GameController(std::uint64_t seed, std::filesystem::path bestScorePath)
    : game_(seed),
      bestScorePath_(bestScorePath.empty() ? DefaultBestScorePath() : std::move(bestScorePath)),
      bestScore_(LoadBestScore()) {}

MoveExecution GameController::ExecuteMove(Direction direction) {
    MoveExecution result {};
    result.before = game_.GetBoard();
    const bool wasGameOver = game_.IsGameOver();
    result.turn = game_.ApplyMove(direction);
    result.moved = result.turn.moved;
    if (!result.moved) {
        return result;
    }

    if (game_.Score() > bestScore_) {
        bestScore_ = game_.Score();
        SaveBestScore(bestScore_);
    }

    result.triggeredGameOver = !wasGameOver && game_.IsGameOver();
    return result;
}

void GameController::Reset(std::uint64_t seed) {
    game_.Reset(seed);
}

bool GameController::Undo() {
    return game_.Undo();
}

const Game& GameController::Model() const {
    return game_;
}

const Board& GameController::BoardState() const {
    return game_.GetBoard();
}

std::uint32_t GameController::Score() const {
    return game_.Score();
}

std::uint32_t GameController::BestScore() const {
    return bestScore_;
}

bool GameController::IsGameOver() const {
    return game_.IsGameOver();
}

bool GameController::HasReached2048Ever() const {
    return game_.HasReached2048Ever();
}

std::uint64_t GameController::Seed() const {
    return game_.Seed();
}

}  // namespace game2048
