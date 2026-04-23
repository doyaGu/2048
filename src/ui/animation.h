#pragma once

#include <optional>

#include <raylib.h>

#include "core/board.h"
#include "runtime/runtime_types.h"

namespace game2048 {

struct AnimationSnapshot {
    Board before {};
    Board after {};
    MoveTrace trace {};
    std::optional<SpawnEvent> spawn {};
};

class AnimationController {
public:
    void SetSpeed(AnimationSpeed speed);
    AnimationSpeed Speed() const;
    void Start(const Board& before, const Board& after, const MoveTrace& trace, std::optional<SpawnEvent> spawn);
    void Reset();
    void Update(float dt);
    bool Active() const;
    float SlideProgress() const;
    float SpawnProgress() const;
    float MergeScale(const CellCoord& cell) const;
    float SpawnScale(const CellCoord& cell) const;
    bool IsMovingTo(const CellCoord& cell) const;
    bool IsSpawnCell(const CellCoord& cell) const;
    const AnimationSnapshot& Snapshot() const;

    // Screen shake (triggered on game-over)
    void TriggerShake(float amplitude, float duration);
    Vector2 ShakeOffset() const;

private:
    AnimationSnapshot snapshot_ {};
    AnimationSpeed speed_ = AnimationSpeed::Normal;
    float elapsed_ = 0.0F;
    float duration_ = 0.0F;
    bool active_ = false;

    float shakeElapsed_   = 0.0F;
    float shakeDuration_  = 0.0F;
    float shakeAmplitude_ = 0.0F;
};

}  // namespace game2048
