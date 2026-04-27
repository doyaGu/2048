#include "runtime/runtime_engine.h"

#include <array>

namespace game2048 {

namespace {

constexpr std::array<ai::AgentKind, 2> kAgentCycle {{
    ai::AgentKind::Greedy,
    ai::AgentKind::Expectimax,
}};

constexpr std::array<AnimationSpeed, 3> kAnimationCycle {{
    AnimationSpeed::Normal,
    AnimationSpeed::Slow,
    AnimationSpeed::Turbo,
}};

template <typename T, std::size_t N>
T NextValue(const std::array<T, N>& values, T current) {
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (values[index] == current) {
            return values[(index + 1) % values.size()];
        }
    }
    return values.front();
}

}  // namespace

RuntimeEngine::RuntimeEngine(RuntimeConfig config)
    : config_(config),
      game_(config.seed) {
    if (config_.initialBoard.has_value()) {
        GameSnapshot snapshot;
        snapshot.board = *config_.initialBoard;
        snapshot.score = 0;
        snapshot.gameOver = !snapshot.board.CanMove();
        snapshot.reached2048 = snapshot.board.HasValue(2048);
        snapshot.rngState = {};
        snapshot.seed = config_.seed;
        game_.Restore(snapshot);
    }
    workerGeneration_ = worker_.Configure(config_.agent, config_.search, config_.ntupleNetwork);
    RefreshSnapshot();
    SyncOverlayState();
    RefreshSnapshot();
    SubmitRecommendationIfNeeded();
}

RuntimeSnapshot RuntimeEngine::Tick(const std::vector<RuntimeEvent>& events, double nowSeconds, bool animationBlocksInput) {
    lastTickSeconds_ = nowSeconds;
    animationBlocksInput_ = animationBlocksInput;
    ConsumeAIResult();
    SyncOverlayState();
    for (const auto& event : events) {
        ApplyEvent(event);
    }

    if ((snapshot_.controlMode == ControlMode::AIAutoplay || snapshot_.controlMode == ControlMode::AISingleStep) &&
        snapshot_.overlayMode == OverlayMode::None &&
        !animationBlocksInput_ &&
        !game_.IsGameOver() &&
        snapshot_.recommendation.valid &&
        snapshot_.recommendationRevision == boardRevision_ &&
        !worker_.Busy()) {
        ExecuteMove(snapshot_.recommendation.direction);
        if (snapshot_.controlMode == ControlMode::AISingleStep) {
            snapshot_.controlMode = ControlMode::Human;
        }
    }

    SubmitRecommendationIfNeeded();
    RefreshSnapshot();
    return snapshot_;
}

const RuntimeSnapshot& RuntimeEngine::Snapshot() const {
    return snapshot_;
}

void RuntimeEngine::ApplyEvent(const RuntimeEvent& event) {
    switch (event.type) {
        case RuntimeEventType::Reset:
            if (snapshot_.overlayMode == OverlayMode::Help) {
                return;
            }
            Reset(game_.Seed());
            return;
        case RuntimeEventType::Undo:
            if (!OverlayBlocksCommands() && game_.Undo()) {
                ++boardRevision_;
                lastMove_.reset();
                snapshot_.recommendation = {};
                snapshot_.recommendationRevision = 0;
            }
            return;
        case RuntimeEventType::Move:
            if (snapshot_.overlayMode == OverlayMode::Victory) {
                snapshot_.overlayMode = OverlayMode::None;
                snapshot_.controlMode = ControlMode::Human;
            }
            if (!OverlayBlocksCommands() && !animationBlocksInput_) {
                ExecuteMove(event.direction);
            }
            return;
        case RuntimeEventType::ToggleAutoplay:
            if (!OverlayBlocksCommands()) {
                if (snapshot_.overlayMode == OverlayMode::Victory) {
                    snapshot_.overlayMode = OverlayMode::None;
                }
                snapshot_.controlMode = snapshot_.controlMode == ControlMode::AIAutoplay
                    ? ControlMode::Human
                    : ControlMode::AIAutoplay;
            }
            return;
        case RuntimeEventType::StepAI:
            if (!OverlayBlocksCommands()) {
                if (snapshot_.overlayMode == OverlayMode::Victory) {
                    snapshot_.overlayMode = OverlayMode::None;
                }
                snapshot_.controlMode = ControlMode::AISingleStep;
                if (!animationBlocksInput_ &&
                    snapshot_.recommendation.valid &&
                    snapshot_.recommendationRevision == boardRevision_) {
                    ExecuteMove(snapshot_.recommendation.direction);
                    snapshot_.controlMode = ControlMode::Human;
                }
            }
            return;
        case RuntimeEventType::CycleAgent:
            if (!OverlayBlocksCommands()) {
                CycleAgent();
            }
            return;
        case RuntimeEventType::CycleAnimation:
            if (!OverlayBlocksCommands()) {
                CycleAnimation();
            }
            return;
        case RuntimeEventType::OpenHelp:
            if (snapshot_.overlayMode != OverlayMode::GameOver) {
                snapshot_.overlayMode = OverlayMode::Help;
                snapshot_.controlMode = ControlMode::Human;
            }
            return;
        case RuntimeEventType::CloseOverlay:
            if (snapshot_.overlayMode == OverlayMode::Help || snapshot_.overlayMode == OverlayMode::Victory) {
                snapshot_.overlayMode = OverlayMode::None;
                snapshot_.controlMode = ControlMode::Human;
            }
            return;
        case RuntimeEventType::Quit:
            quitRequested_ = true;
            return;
    }
}

void RuntimeEngine::ExecuteMove(Direction direction) {
    const Board before = game_.GetBoard();
    const auto result = game_.ApplyMove(direction);
    if (!result.moved) {
        return;
    }
    ++boardRevision_;
    lastMove_ = RuntimeMoveAnimation {
        boardRevision_,
        before,
        game_.GetBoard(),
        result.trace,
        result.spawn,
        game_.IsGameOver()
    };
    snapshot_.recommendation = {};
    snapshot_.recommendationRevision = 0;
    snapshot_.aiStatus = AIStatus::Idle;
    SyncOverlayState();
}

void RuntimeEngine::Reset(std::uint64_t seed) {
    config_.seed = seed;
    game_.Reset(seed);
    ++boardRevision_;
    lastMove_.reset();
    submittedRevision_ = kNoSubmittedRevision;
    snapshot_.recommendation = {};
    snapshot_.recommendationRevision = 0;
    snapshot_.lastSearch = {};
    snapshot_.aiStatus = AIStatus::Idle;
    snapshot_.controlMode = ControlMode::Human;
    snapshot_.overlayMode = OverlayMode::None;
    victoryOverlayShown_ = false;
}

void RuntimeEngine::SubmitRecommendationIfNeeded() {
    if (game_.IsGameOver() || snapshot_.overlayMode == OverlayMode::Help) {
        return;
    }
    if (snapshot_.recommendation.valid && snapshot_.recommendationRevision == boardRevision_) {
        return;
    }
    if (submittedRevision_ == boardRevision_) {
        return;
    }
    submittedRevision_ = boardRevision_;
    worker_.Submit(game_.GetBoard(), boardRevision_);
}

void RuntimeEngine::ConsumeAIResult() {
    const auto result = worker_.Poll();
    if (!result.has_value()) {
        return;
    }
    if (result->generation != workerGeneration_) {
        return;
    }
    if (result->revision != boardRevision_) {
        return;
    }
    if (result->failed) {
        snapshot_.aiStatus = AIStatus::Failed;
        snapshot_.recommendation = {};
        snapshot_.recommendationRevision = 0;
        snapshot_.lastSearch = {};
        return;
    }
    snapshot_.recommendation = result->decision;
    snapshot_.recommendationRevision = result->revision;
    snapshot_.lastSearch = result->decision.stats;
    snapshot_.aiStatus = AIStatus::Ready;
}

void RuntimeEngine::RefreshSnapshot() {
    snapshot_.board = game_.GetBoard();
    snapshot_.score = game_.Score();
    snapshot_.maxTile = game_.GetBoard().MaxTile();
    snapshot_.gameOver = game_.IsGameOver();
    snapshot_.reached2048 = game_.HasReached2048Ever();
    snapshot_.seed = game_.Seed();
    snapshot_.boardRevision = boardRevision_;
    snapshot_.quitRequested = quitRequested_;
    snapshot_.agent = config_.agent;
    snapshot_.hasTrainedModel = static_cast<bool>(config_.ntupleNetwork);
    snapshot_.modelLabel = config_.modelLabel;
    snapshot_.showContinueHint = victoryOverlayShown_ && snapshot_.overlayMode != OverlayMode::GameOver;
    snapshot_.canDismissOverlay = snapshot_.overlayMode == OverlayMode::Help || snapshot_.overlayMode == OverlayMode::Victory;
    snapshot_.evaluatorBreakdown = ai::Evaluator().Breakdown(FastBoard::FromReference(game_.GetBoard()));
    snapshot_.lastMove = lastMove_;
    if (snapshot_.recommendation.valid && snapshot_.recommendationRevision == boardRevision_) {
        snapshot_.aiStatus = AIStatus::Ready;
    } else if (snapshot_.aiStatus != AIStatus::Failed && (worker_.Busy() || submittedRevision_ == boardRevision_)) {
        snapshot_.aiStatus = AIStatus::Searching;
    } else if (snapshot_.aiStatus != AIStatus::Failed) {
        snapshot_.aiStatus = AIStatus::Idle;
    }
    RecomputeGate();
}

void RuntimeEngine::RecomputeGate() {
    if (snapshot_.overlayMode != OverlayMode::None) {
        snapshot_.inputGate = InputGate::BlockedByOverlay;
        return;
    }
    snapshot_.inputGate = animationBlocksInput_
        ? InputGate::BlockedByAnimation
        : InputGate::Accepting;
}

void RuntimeEngine::SyncOverlayState() {
    if (game_.IsGameOver()) {
        snapshot_.overlayMode = OverlayMode::GameOver;
        snapshot_.controlMode = ControlMode::Human;
        return;
    }
    if (snapshot_.overlayMode == OverlayMode::GameOver) {
        snapshot_.overlayMode = OverlayMode::None;
    }
    if (game_.HasReached2048Ever() && !victoryOverlayShown_ && snapshot_.overlayMode == OverlayMode::None) {
        victoryOverlayShown_ = true;
        snapshot_.overlayMode = OverlayMode::Victory;
        snapshot_.controlMode = ControlMode::Human;
    }
}

void RuntimeEngine::CycleAgent() {
    config_.agent = NextValue(kAgentCycle, config_.agent);
    workerGeneration_ = worker_.Configure(config_.agent, config_.search, config_.ntupleNetwork);
    submittedRevision_ = kNoSubmittedRevision;
    snapshot_.recommendation = {};
    snapshot_.recommendationRevision = 0;
    snapshot_.lastSearch = {};
    snapshot_.aiStatus = AIStatus::Idle;
}

void RuntimeEngine::CycleAnimation() {
    snapshot_.animationSpeed = NextValue(kAnimationCycle, snapshot_.animationSpeed);
}

bool RuntimeEngine::OverlayBlocksCommands() const {
    return snapshot_.overlayMode == OverlayMode::Help || snapshot_.overlayMode == OverlayMode::GameOver;
}

}  // namespace game2048
