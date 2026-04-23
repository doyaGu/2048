#pragma once

#include <cstdint>
#include <filesystem>

#include "core/game.h"

namespace game2048 {

struct MoveExecution {
    bool moved = false;
    Board before {};
    TurnResult turn {};
    bool triggeredGameOver = false;
};

class GameController {
public:
    explicit GameController(std::uint64_t seed = kDefaultSeed,
                            std::filesystem::path bestScorePath = {});

    MoveExecution ExecuteMove(Direction direction);
    void Reset(std::uint64_t seed);
    bool Undo();

    const Game& Model() const;
    const Board& BoardState() const;
    std::uint32_t Score() const;
    std::uint32_t BestScore() const;
    bool IsGameOver() const;
    bool HasReached2048Ever() const;
    std::uint64_t Seed() const;

private:
    static std::filesystem::path DefaultBestScorePath();
    std::uint32_t LoadBestScore() const;
    void SaveBestScore(std::uint32_t score) const;

    Game game_;
    std::filesystem::path bestScorePath_;
    std::uint32_t bestScore_ = 0;
};

}  // namespace game2048
