#pragma once

#include <vector>

#include "runtime/ai_worker.h"
#include "runtime/runtime_types.h"
#include "core/game.h"

namespace game2048 {

class RuntimeEngine {
public:
    explicit RuntimeEngine(RuntimeConfig config = {});

    RuntimeSnapshot Tick(const std::vector<RuntimeEvent>& events, double nowSeconds, bool animationBlocksInput = false);
    const RuntimeSnapshot& Snapshot() const;

private:
    void ApplyEvent(const RuntimeEvent& event);
    void ExecuteMove(Direction direction);
    void Reset(std::uint64_t seed);
    void SubmitRecommendationIfNeeded();
    void ConsumeAIResult();
    void RefreshSnapshot();
    void RecomputeGate();
    void SyncOverlayState();
    void CycleAgent();
    void CycleAnimation();
    bool OverlayBlocksCommands() const;

    RuntimeConfig config_ {};
    Game game_;
    AIWorker worker_;
    RuntimeSnapshot snapshot_ {};
    std::optional<RuntimeMoveAnimation> lastMove_ {};
    std::uint64_t boardRevision_ = 0;
    std::uint64_t submittedRevision_ = static_cast<std::uint64_t>(-1);
    std::uint64_t workerGeneration_ = 0;
    bool quitRequested_ = false;
    bool victoryOverlayShown_ = false;
    double lastTickSeconds_ = 0.0;
    bool animationBlocksInput_ = false;
};

}  // namespace game2048
