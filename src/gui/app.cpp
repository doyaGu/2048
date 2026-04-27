#include "gui/app.h"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

#include <raylib.h>

#include "cli/cli_app.h"
#include "input/input_source.h"
#include "input/input_system.h"
#include "gui/runtime_event_mapper.h"
#include "runtime/runtime_engine.h"
#include "ui/animation.h"
#include "ui/layout.h"
#include "ui/renderer.h"
#include "ui/ui.h"

namespace game2048 {

namespace {

constexpr std::uint64_t kNoAnimatedMoveRevision = std::numeric_limits<std::uint64_t>::max();

RuntimeConfig ToRuntimeConfig(const CliOptions& options) {
    RuntimeConfig config;
    config.seed = options.seed;
    config.agent = options.agent;
    config.search = options.search;
    if (options.weightPath.has_value()) {
        config.ntupleNetwork = std::make_shared<ai::NtupleNetwork>(ai::NtupleNetwork::Load(*options.weightPath));
        config.modelLabel = std::filesystem::path(*options.weightPath).filename().string();
    }
    return config;
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

    try {
        if (options.command != CliCommand::Play) {
            return RunCliCommand(options);
        }
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(kDefaultWindowWidth, kDefaultWindowHeight, "2048 AI Lab");
    SetTargetFPS(kTargetFps);

    RuntimeEngine runtime(ToRuntimeConfig(options));
    RuntimeSnapshot snapshot = runtime.Snapshot();
    AnimationController animation;
    InputSource inputSource;
    InputSystem inputSystem;
    RuntimeEventMapper eventMapper;
    Renderer renderer;
    UI ui;

    bool touchHudActive = false;
    std::uint64_t animatedMoveRevision = kNoAnimatedMoveRevision;

    while (!WindowShouldClose()) {
        animation.SetSpeed(snapshot.animationSpeed);
        animation.Update(GetFrameTime());

        const bool animationBlocksInput = animation.Active() && animation.Speed() != AnimationSpeed::Turbo;
        const RawInputState rawInput = inputSource.Poll();
        if (rawInput.pointers[0].connected && rawInput.pointers[0].isTouch) {
            touchHudActive = true;
        }

        const auto layout = ComputeLayout(GetScreenWidth(), GetScreenHeight(), touchHudActive);
        const auto frame = inputSystem.BuildFrame(rawInput, layout, touchHudActive, animationBlocksInput, snapshot.overlayMode);

        const std::vector<RuntimeEvent> events = eventMapper.BuildEvents(frame, snapshot.overlayMode, rawInput.nowSeconds);

        snapshot = runtime.Tick(events, rawInput.nowSeconds, animationBlocksInput);
        animation.SetSpeed(snapshot.animationSpeed);
        if (snapshot.lastMove.has_value() && snapshot.lastMove->revision != animatedMoveRevision) {
            animation.Start(snapshot.lastMove->before,
                            snapshot.lastMove->after,
                            snapshot.lastMove->trace,
                            snapshot.lastMove->spawn);
            if (snapshot.lastMove->triggeredGameOver) {
                animation.TriggerShake(6.0F, 0.45F);
            }
            animatedMoveRevision = snapshot.lastMove->revision;
        } else if (!snapshot.lastMove.has_value() && snapshot.boardRevision != animatedMoveRevision) {
            animation.Reset();
            animatedMoveRevision = snapshot.boardRevision;
        }

        if (snapshot.quitRequested) {
            CloseWindow();
            return 0;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
        renderer.DrawBoard(layout, snapshot.board, animation);
        ui.DrawPanels(layout, snapshot);
        ui.DrawOverlay(layout, snapshot);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}

}  // namespace game2048
