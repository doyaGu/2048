#include <chrono>
#include <array>
#include <thread>

#include "../src/runtime/runtime_engine.h"
#include "test_framework.h"

namespace {

using game2048::Direction;
using game2048::RuntimeConfig;
using game2048::RuntimeEngine;
using game2048::RuntimeEvent;
using game2048::RuntimeEventType;
using game2048::AIStatus;
using game2048::ControlMode;
using game2048::OverlayMode;

RuntimeConfig FastConfig(std::uint64_t seed = 123) {
    RuntimeConfig config;
    config.seed = seed;
    config.agent = game2048::ai::AgentKind::Greedy;
    config.search.maxDepth = 1;
    config.search.timeBudgetMs = 1;
    return config;
}

}  // namespace

TEST_CASE(RuntimeEngine_MoveEvent_ChangesBoardAndRevision) {
    RuntimeEngine runtime(FastConfig(7));
    const auto before = runtime.Tick({}, 0.0);

    const RuntimeEvent move {RuntimeEventType::Move, Direction::Left};
    const auto after = runtime.Tick({move}, 0.1);

    EXPECT_TRUE(after.boardRevision >= before.boardRevision);
    if (after.board != before.board) {
        EXPECT_EQ(after.boardRevision, before.boardRevision + 1U);
    } else {
        EXPECT_EQ(after.boardRevision, before.boardRevision);
    }
}

TEST_CASE(RuntimeEngine_MoveEvent_ExposesAnimationTrace) {
    RuntimeEngine runtime(FastConfig(7));
    const auto before = runtime.Tick({}, 0.0);
    const std::array<Direction, 4> directions {
        Direction::Left,
        Direction::Right,
        Direction::Up,
        Direction::Down,
    };

    game2048::RuntimeSnapshot snapshot = before;
    for (Direction direction : directions) {
        snapshot = runtime.Tick({RuntimeEvent {RuntimeEventType::Move, direction}}, 0.1);
        if (snapshot.boardRevision > before.boardRevision) {
            break;
        }
    }

    EXPECT_TRUE(snapshot.boardRevision > before.boardRevision);
    EXPECT_TRUE(snapshot.lastMove.has_value());
    EXPECT_EQ(snapshot.lastMove->after, snapshot.board);
    EXPECT_TRUE(snapshot.lastMove->spawn.has_value());
}

TEST_CASE(RuntimeEngine_ResetAndUndo_AreRuntimeEvents) {
    RuntimeEngine runtime(FastConfig(1));
    const RuntimeEvent move {RuntimeEventType::Move, Direction::Left};
    const auto before = runtime.Tick({}, 0.0);
    runtime.Tick({move}, 0.1);
    const auto moved = runtime.Tick({}, 0.2);

    runtime.Tick({RuntimeEvent {RuntimeEventType::Undo, Direction::Left}}, 0.3);
    const auto undone = runtime.Tick({}, 0.4);
    EXPECT_EQ(undone.boardRevision, moved.boardRevision + 1U);

    runtime.Tick({RuntimeEvent {RuntimeEventType::Reset, Direction::Left}}, 0.5);
    const auto reset = runtime.Tick({}, 0.6);
    EXPECT_EQ(reset.score, 0U);
    EXPECT_TRUE(reset.boardRevision > before.boardRevision);
}

TEST_CASE(RuntimeEngine_OverlayAndAutoplay_AreInSnapshot) {
    RuntimeEngine runtime(FastConfig(11));

    runtime.Tick({RuntimeEvent {RuntimeEventType::OpenHelp, Direction::Left}}, 0.0);
    auto snapshot = runtime.Tick({}, 0.1);
    EXPECT_EQ(snapshot.overlayMode, OverlayMode::Help);

    runtime.Tick({RuntimeEvent {RuntimeEventType::CloseOverlay, Direction::Left}}, 0.2);
    runtime.Tick({RuntimeEvent {RuntimeEventType::ToggleAutoplay, Direction::Left}}, 0.3);
    snapshot = runtime.Tick({}, 0.4);
    EXPECT_EQ(snapshot.overlayMode, OverlayMode::None);
    EXPECT_EQ(snapshot.controlMode, ControlMode::AIAutoplay);
}

TEST_CASE(RuntimeEngine_AIWorker_PublishesReadyRecommendation) {
    RuntimeEngine runtime(FastConfig(21));

    auto snapshot = runtime.Tick({}, 0.0);
    EXPECT_TRUE(snapshot.aiStatus == AIStatus::Searching || snapshot.aiStatus == AIStatus::Ready);

    for (int attempt = 0; attempt < 50 && snapshot.aiStatus != AIStatus::Ready; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        snapshot = runtime.Tick({}, 0.1 + static_cast<double>(attempt) * 0.01);
    }

    EXPECT_EQ(snapshot.aiStatus, AIStatus::Ready);
    EXPECT_TRUE(snapshot.recommendation.valid);
    EXPECT_EQ(snapshot.recommendationRevision, snapshot.boardRevision);
}

TEST_CASE(RuntimeEngine_StaleAIResults_DoNotReplaceCurrentRevision) {
    RuntimeConfig config = FastConfig(31);
    config.agent = game2048::ai::AgentKind::Expectimax;
    config.search.maxDepth = 4;
    config.search.timeBudgetMs = 20;
    RuntimeEngine runtime(config);

    runtime.Tick({}, 0.0);
    runtime.Tick({RuntimeEvent {RuntimeEventType::Move, Direction::Left}}, 0.01);
    auto snapshot = runtime.Tick({RuntimeEvent {RuntimeEventType::Move, Direction::Up}}, 0.02);

    for (int attempt = 0; attempt < 100 && snapshot.aiStatus != AIStatus::Ready; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        snapshot = runtime.Tick({}, 0.1 + static_cast<double>(attempt) * 0.01);
    }

    if (snapshot.aiStatus == AIStatus::Ready) {
        EXPECT_EQ(snapshot.recommendationRevision, snapshot.boardRevision);
    }
}

TEST_CASE(RuntimeEngine_AIStatus_RecoversAfterFailedSearch) {
    RuntimeConfig config = FastConfig(41);
    config.agent = game2048::ai::AgentKind::Expectimax;
    config.search.maxDepth = -1;
    RuntimeEngine runtime(config);

    auto snapshot = runtime.Tick({}, 0.0);
    for (int attempt = 0; attempt < 50 && snapshot.aiStatus != AIStatus::Failed; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        snapshot = runtime.Tick({}, 0.1 + static_cast<double>(attempt) * 0.01);
    }
    EXPECT_EQ(snapshot.aiStatus, AIStatus::Failed);

    runtime.Tick({RuntimeEvent {RuntimeEventType::CycleAgent, Direction::Left}}, 1.0);
    for (int attempt = 0; attempt < 50 && snapshot.aiStatus != AIStatus::Ready; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        snapshot = runtime.Tick({}, 1.1 + static_cast<double>(attempt) * 0.01);
    }

    EXPECT_EQ(snapshot.aiStatus, AIStatus::Ready);
    EXPECT_TRUE(snapshot.recommendation.valid);
}
