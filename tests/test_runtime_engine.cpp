#include <chrono>
#include <array>
#include <memory>
#include <thread>

#include "../src/runtime/runtime_engine.h"
#include "../src/value/ntuple.h"
#include "test_framework.h"

namespace {

using game2048::Direction;
using game2048::RuntimeConfig;
using game2048::RuntimeEngine;
using game2048::RuntimeEvent;
using game2048::RuntimeEventType;
using game2048::AIStatus;
using game2048::ControlMode;
using game2048::InputGate;
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
    EXPECT_TRUE(moved.lastMove.has_value());

    runtime.Tick({RuntimeEvent {RuntimeEventType::Undo, Direction::Left}}, 0.3);
    const auto undone = runtime.Tick({}, 0.4);
    EXPECT_EQ(undone.boardRevision, moved.boardRevision + 1U);
    EXPECT_FALSE(undone.lastMove.has_value());

    runtime.Tick({move}, 0.45);
    const auto movedAgain = runtime.Tick({}, 0.46);
    EXPECT_TRUE(movedAgain.lastMove.has_value());
    runtime.Tick({RuntimeEvent {RuntimeEventType::Reset, Direction::Left}}, 0.5);
    const auto reset = runtime.Tick({}, 0.6);
    EXPECT_EQ(reset.score, 0U);
    EXPECT_TRUE(reset.boardRevision > before.boardRevision);
    EXPECT_FALSE(reset.lastMove.has_value());
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

TEST_CASE(RuntimeEngine_Snapshot_ExposesConfiguredTrainedModel) {
    RuntimeConfig config = FastConfig(12);
    config.ntupleNetwork = std::make_shared<game2048::ai::NtupleNetwork>(
        std::vector<game2048::ai::NtuplePattern> {game2048::ai::NtuplePattern {{0, 1, 2, 3}}});
    config.modelLabel = "best.weights";

    RuntimeEngine runtime(config);
    const auto snapshot = runtime.Tick({}, 0.0);

    EXPECT_TRUE(snapshot.hasTrainedModel);
    EXPECT_EQ(snapshot.modelLabel, std::string("best.weights"));
}

TEST_CASE(RuntimeEngine_InputGate_TracksAnimationBlocking) {
    RuntimeEngine runtime(FastConfig(15));

    auto snapshot = runtime.Tick({}, 0.0, true);
    EXPECT_EQ(snapshot.inputGate, InputGate::BlockedByAnimation);

    snapshot = runtime.Tick({RuntimeEvent {RuntimeEventType::OpenHelp, Direction::Left}}, 0.1, true);
    EXPECT_EQ(snapshot.inputGate, InputGate::BlockedByOverlay);
}

TEST_CASE(RuntimeEngine_ToggleAutoplay_DismissesVictoryOverlay) {
    RuntimeConfig config = FastConfig(11);
    config.initialBoard = game2048::Board::FromRows({{
        {{2048, 0, 0, 0}},
        {{0, 0, 0, 0}},
        {{0, 0, 0, 0}},
        {{0, 0, 0, 0}},
    }});
    RuntimeEngine runtime(config);

    auto snapshot = runtime.Tick({}, 0.1);
    EXPECT_EQ(snapshot.overlayMode, OverlayMode::Victory);

    snapshot = runtime.Tick({RuntimeEvent {RuntimeEventType::ToggleAutoplay, Direction::Left}}, 0.2);

    EXPECT_EQ(snapshot.overlayMode, OverlayMode::None);
    EXPECT_EQ(snapshot.controlMode, ControlMode::AIAutoplay);
}

TEST_CASE(RuntimeEngine_HelpOverlay_BlocksGameplayStateChanges) {
    RuntimeEngine runtime(FastConfig(13));
    const auto before = runtime.Tick({}, 0.0);

    auto snapshot = runtime.Tick({RuntimeEvent {RuntimeEventType::OpenHelp, Direction::Left}}, 0.1);
    EXPECT_EQ(snapshot.overlayMode, OverlayMode::Help);

    snapshot = runtime.Tick({RuntimeEvent {RuntimeEventType::Reset, Direction::Left}}, 0.2);
    EXPECT_EQ(snapshot.overlayMode, OverlayMode::Help);
    EXPECT_EQ(snapshot.boardRevision, before.boardRevision);

    snapshot = runtime.Tick({RuntimeEvent {RuntimeEventType::Move, Direction::Left}}, 0.3);
    EXPECT_EQ(snapshot.overlayMode, OverlayMode::Help);
    EXPECT_EQ(snapshot.boardRevision, before.boardRevision);
}

TEST_CASE(RuntimeEngine_StepAI_DismissesVictoryOverlay) {
    RuntimeConfig config = FastConfig(17);
    config.initialBoard = game2048::Board::FromRows({{
        {{2048, 0, 0, 0}},
        {{0, 0, 0, 0}},
        {{0, 0, 0, 0}},
        {{0, 0, 0, 0}},
    }});
    RuntimeEngine runtime(config);

    auto snapshot = runtime.Tick({}, 0.1);
    EXPECT_EQ(snapshot.overlayMode, OverlayMode::Victory);

    snapshot = runtime.Tick({RuntimeEvent {RuntimeEventType::StepAI, Direction::Left}}, 0.2);

    EXPECT_EQ(snapshot.overlayMode, OverlayMode::None);
    EXPECT_TRUE(snapshot.controlMode == ControlMode::AISingleStep || snapshot.controlMode == ControlMode::Human);
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

TEST_CASE(AIWorker_BusyAndPoll_PreserveCompletedResultsUntilConsumed) {
    game2048::AIWorker worker;

    game2048::ai::SearchConfig search;
    search.maxDepth = -1;
    const auto generation = worker.Configure(game2048::ai::AgentKind::Expectimax, search);

    const game2048::Board board;
    worker.Submit(board, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_TRUE(worker.Busy());

    worker.Submit(board, 2);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto first = worker.Poll();
    EXPECT_TRUE(first.has_value());
    if (first.has_value()) {
        EXPECT_EQ(first->revision, 1U);
        EXPECT_EQ(first->generation, generation);
        EXPECT_TRUE(first->failed);
    }

    auto second = worker.Poll();
    for (int attempt = 0; attempt < 50 && !second.has_value(); ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        second = worker.Poll();
    }

    EXPECT_TRUE(second.has_value());
    if (second.has_value()) {
        EXPECT_EQ(second->revision, 2U);
        EXPECT_EQ(second->generation, generation);
        EXPECT_TRUE(second->failed);
    }

    EXPECT_FALSE(worker.Busy());
}

TEST_CASE(AIWorker_UsesConfiguredNtupleLeafNetwork) {
    game2048::AIWorker worker;

    game2048::ai::SearchConfig search;
    search.maxDepth = 0;
    search.timeBudgetMs = 0;
    search.iterativeDeepening = false;
    search.useTranspositionTable = false;

    auto network = std::make_shared<game2048::ai::NtupleNetwork>(
        std::vector<game2048::ai::NtuplePattern> {game2048::ai::NtuplePattern {{0, 1, 2, 3}}});
    const game2048::Board board = game2048::Board::FromRows({{
        {{0, 0, 2, 4}},
        {{0, 0, 0, 0}},
        {{0, 0, 0, 0}},
        {{0, 0, 0, 0}},
    }});
    const game2048::FastBoard afterLeft(game2048::FastBoard::FromReference(board).MoveLeft().board);
    network->UpdateToward(afterLeft, 1000.0, 1.0);

    const auto generation = worker.Configure(game2048::ai::AgentKind::Expectimax, search, network);
    worker.Submit(board, 7);

    auto result = worker.Poll();
    for (int attempt = 0; attempt < 50 && !result.has_value(); ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        result = worker.Poll();
    }

    EXPECT_TRUE(result.has_value());
    if (result.has_value()) {
        EXPECT_EQ(result->generation, generation);
        EXPECT_TRUE(result->decision.valid);
        EXPECT_EQ(result->decision.direction, game2048::Direction::Left);
    }
}
