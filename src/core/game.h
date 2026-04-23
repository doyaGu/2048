#pragma once

#include <optional>
#include <vector>

#include "core/board.h"
#include "core/rng.h"

namespace game2048 {

enum class RunMode {
    Human,
    AutoAI,
    SingleStepAI
};

struct TurnResult {
    bool moved = false;
    std::uint32_t scoreDelta = 0;
    MoveTrace trace {};
    std::optional<SpawnEvent> spawn {};
};

struct GameSnapshot {
    Board board {};
    std::uint32_t score = 0;
    bool gameOver = false;
    bool reached2048 = false;
    Random::EngineState rngState {};
    std::uint64_t seed = kDefaultSeed;
};

class Game {
public:
    explicit Game(std::uint64_t seed = kDefaultSeed);

    void Reset(std::uint64_t seed);
    void Restore(const GameSnapshot& snapshot);
    TurnResult ApplyMove(Direction direction);
    bool Undo();

    const Board& GetBoard() const;
    std::uint32_t Score() const;
    bool IsGameOver() const;
    bool HasReached2048Ever() const;
    bool Reached2048() const;
    std::uint64_t Seed() const;
    bool CanUndo() const;

private:
    void PushUndo();
    void UpdateTerminalFlags();

    Board board_;
    Random rng_;
    std::uint32_t score_ = 0;
    bool gameOver_ = false;
    bool reached2048_ = false;
    std::vector<GameSnapshot> undo_;
};

}  // namespace game2048
