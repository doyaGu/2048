#include "app/app.h"

#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>

#include <raylib.h>

#include "ai/benchmark.h"
#include "app/ai_advisor.h"
#include "app/hud_mapper.h"
#include "ui/animation.h"
#include "app/cli_options.h"
#include "app/game_controller.h"
#include "input/input_source.h"
#include "input/input_system.h"
#include "app/interaction_session.h"
#include "ui/layout.h"
#include "ui/renderer.h"
#include "ui/ui.h"

namespace game2048 {

namespace {

constexpr std::array<AnimationSpeed, 3> kAnimationSpeedCycle {{
    AnimationSpeed::Normal,
    AnimationSpeed::Slow,
    AnimationSpeed::Turbo,
}};

AnimationSpeed NextSpeed(AnimationSpeed current) {
    for (std::size_t index = 0; index < kAnimationSpeedCycle.size(); ++index) {
        if (kAnimationSpeedCycle[index] == current) {
            return kAnimationSpeedCycle[(index + 1) % kAnimationSpeedCycle.size()];
        }
    }
    return kAnimationSpeedCycle.front();
}

}  // namespace

int App::Run(int argc, char** argv) {
    CliOptions options;
    try {
        options = ParseCliOptions(argc, argv);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }

    if (options.benchmarkGames.has_value()) {
        ai::BenchmarkRunner runner;
        const auto started = std::chrono::steady_clock::now();
        const auto results = runner.Run({
            *options.benchmarkGames,
            options.seed,
            false,
            options.agent,
            options.search,
            options.csvPath
        });
        const double elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        const auto summary = SummarizeBenchmark(results, elapsed);
        std::cout << FormatBenchmarkSummary(summary) << '\n';
        return 0;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(kDefaultWindowWidth, kDefaultWindowHeight, "2048 Engine");
    SetTargetFPS(kTargetFps);

    GameController gameController(options.seed);
    InteractionSession session(options.seed);
    AIAdvisor advisor(options.agent, options.search);

    AnimationController animation;
    InputSource inputSource;
    InputSystem inputSystem;
    Renderer renderer;
    UI ui;

    bool touchHudActive = false;

    auto executeMove = [&](Direction direction) {
        const auto execution = gameController.ExecuteMove(direction);
        if (!execution.moved) {
            return false;
        }

        animation.Start(execution.before, gameController.BoardState(), execution.turn.trace, execution.turn.spawn);
        advisor.Invalidate();
        if (execution.triggeredGameOver) {
            animation.TriggerShake(6.0F, 0.45F);
        }
        return true;
    };

    while (!WindowShouldClose()) {
        animation.Update(GetFrameTime());

        const bool animationBlocksInput = animation.Active() && animation.Speed() != AnimationSpeed::Turbo;

        const RawInputState rawInput = inputSource.Poll();
        if (rawInput.pointers[0].connected && rawInput.pointers[0].isTouch) {
            touchHudActive = true;
        }
        const auto layout = ComputeLayout(GetScreenWidth(), GetScreenHeight(), touchHudActive);
        const auto frame = inputSystem.BuildFrame(rawInput, layout, touchHudActive, animationBlocksInput, session.Overlay());

        InteractionInput input {};
        input.nowSeconds = rawInput.nowSeconds;
        input.animationBlocksInput = animationBlocksInput;
        input.gameOver = gameController.IsGameOver();
        input.reached2048Ever = gameController.HasReached2048Ever();
        input.command = frame.command;
        input.pressedMove = frame.pressedMove;
        input.heldMove = frame.heldMove;

        const auto actions = session.Tick(input);

        if (actions.exitRequested) {
            CloseWindow();
            return 0;
        }

        if (actions.cycleAgentRequested) {
            advisor.CycleAgent();
        }

        if (actions.cycleAnimationRequested) {
            animation.SetSpeed(NextSpeed(animation.Speed()));
        }

        if (actions.resetRequested) {
            gameController.Reset(gameController.Seed());
            session.OnReset(gameController.Seed());
            animation.Reset();
            advisor.ResetRecommendation();
            touchHudActive = false;
        }

        if (actions.undoRequested && gameController.Undo()) {
            animation.Reset();
            advisor.ResetRecommendation();
        }

        if (actions.hintInvalidated) {
            advisor.Invalidate();
        }

        if (actions.moveToExecute.has_value()) {
            executeMove(*actions.moveToExecute);
        } else if (actions.aiMoveRequested && !gameController.IsGameOver()) {
            const auto decision = advisor.RequestMove(gameController.BoardState());
            executeMove(decision.direction);
        }

        if (!gameController.IsGameOver()) {
            advisor.EnsureRecommendation(gameController.BoardState());
        }

        const HUDState hud = BuildHUDState(gameController, session, advisor, animation.Speed());

        BeginDrawing();
        ClearBackground(RAYWHITE);
        renderer.DrawBoard(layout, gameController.BoardState(), animation);
        ui.DrawPanels(layout, hud);
        ui.DrawOverlay(layout, hud);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}

}  // namespace game2048
