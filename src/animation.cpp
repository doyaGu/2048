#include "animation.h"

#include <algorithm>
#include <cmath>

#include <raylib.h>

namespace game2048 {

namespace {

float DurationForSpeed(AnimationSpeed speed) {
    switch (speed) {
        case AnimationSpeed::Normal:
            return static_cast<float>(kDefaultAnimationMs) / 1000.0F;
        case AnimationSpeed::Slow:
            return static_cast<float>(kSlowAnimationMs) / 1000.0F;
        case AnimationSpeed::Turbo:
            return static_cast<float>(kTurboAnimationMs) / 1000.0F;
    }
    return 0.0F;
}

float EaseOutBack(float t) {
    const float c1 = 1.70158F;
    const float c3 = c1 + 1.0F;
    const float u = t - 1.0F;
    return 1.0F + c3 * u * u * u + c1 * u * u;
}

bool MatchesCell(const CellCoord& lhs, const CellCoord& rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

}  // namespace

void AnimationController::SetSpeed(AnimationSpeed speed) {
    speed_ = speed;
}

AnimationSpeed AnimationController::Speed() const {
    return speed_;
}

void AnimationController::Start(const Board& before, const Board& after, const MoveTrace& trace, std::optional<SpawnEvent> spawn) {
    snapshot_.before = before;
    snapshot_.after = after;
    snapshot_.trace = trace;
    snapshot_.spawn = std::move(spawn);
    elapsed_ = 0.0F;
    duration_ = DurationForSpeed(speed_);
    active_ = duration_ > 0.0F && (!snapshot_.trace.moves.empty() || snapshot_.spawn.has_value());
}

void AnimationController::Update(float dt) {
    if (!active_) {
        // still advance shake even when tile animation is done
    } else {
        elapsed_ += dt;
        if (elapsed_ >= duration_) {
            elapsed_ = duration_;
            active_ = false;
        }
    }
    if (shakeElapsed_ < shakeDuration_) {
        shakeElapsed_ = std::min(shakeElapsed_ + dt, shakeDuration_);
    }
}

bool AnimationController::Active() const {
    return active_;
}

float AnimationController::SlideProgress() const {
    if (duration_ <= 0.0F) {
        return 1.0F;
    }
    return std::clamp(elapsed_ / duration_, 0.0F, 1.0F);
}

float AnimationController::SpawnProgress() const {
    if (!snapshot_.spawn.has_value()) {
        return 1.0F;
    }
    if (duration_ <= 0.0F) {
        return 1.0F;
    }
    const float start = duration_ * 0.35F;
    const float t = std::clamp((elapsed_ - start) / (duration_ - start), 0.0F, 1.0F);
    return EaseOutBack(t);
}

float AnimationController::MergeScale(const CellCoord& cell) const {
    if (duration_ <= 0.0F) {
        return 1.0F;
    }
    for (const auto& merge : snapshot_.trace.merges) {
        if (!MatchesCell(merge.cell, cell)) {
            continue;
        }
        const float start = duration_ * 0.45F;
        const float t = std::clamp((elapsed_ - start) / (duration_ - start), 0.0F, 1.0F);
        return 1.0F + 0.24F * std::sin(t * 3.1415926F);
    }
    return 1.0F;
}

float AnimationController::SpawnScale(const CellCoord& cell) const {
    if (snapshot_.spawn.has_value() && MatchesCell(snapshot_.spawn->cell, cell)) {
        return 0.72F + 0.28F * SpawnProgress();
    }
    return 1.0F;
}

bool AnimationController::IsMovingTo(const CellCoord& cell) const {
    if (!active_) {
        return false;
    }
    return std::any_of(snapshot_.trace.moves.begin(), snapshot_.trace.moves.end(), [&cell](const TileMove& move) {
        return MatchesCell(move.to, cell);
    });
}

bool AnimationController::IsSpawnCell(const CellCoord& cell) const {
    return snapshot_.spawn.has_value() && MatchesCell(snapshot_.spawn->cell, cell);
}

const AnimationSnapshot& AnimationController::Snapshot() const {
    return snapshot_;
}

void AnimationController::TriggerShake(float amplitude, float duration) {
    shakeAmplitude_ = amplitude;
    shakeDuration_  = duration;
    shakeElapsed_   = 0.0F;
}

Vector2 AnimationController::ShakeOffset() const {
    if (shakeElapsed_ >= shakeDuration_ || shakeDuration_ <= 0.0F) {
        return {0.0F, 0.0F};
    }
    const float decay  = 1.0F - shakeElapsed_ / shakeDuration_;
    const float offset = sinf(shakeElapsed_ * 62.0F) * shakeAmplitude_ * decay;
    return {offset, offset * 0.55F};
}

}  // namespace game2048
