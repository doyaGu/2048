#include "app/app.h"

#include <array>
#include <chrono>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <raylib.h>

#include "ai/benchmark.h"
#include "ai/ai_engine.h"
#include "app/cli_options.h"
#include "app/runtime_event_mapper.h"
#include "input/input_source.h"
#include "input/input_system.h"
#include "runtime/runtime_engine.h"
#include "ui/animation.h"
#include "ui/layout.h"
#include "ui/renderer.h"
#include "ui/ui.h"

namespace game2048 {

namespace {

RuntimeConfig ToRuntimeConfig(const CliOptions& options) {
    RuntimeConfig config;
    config.seed = options.seed;
    config.agent = options.agent;
    config.search = options.search;
    return config;
}

std::array<std::array<int, kBoardSize>, kBoardSize> ParseBoardRows(const std::string& rows) {
    std::array<std::array<int, kBoardSize>, kBoardSize> values {};
    std::stringstream rowStream(rows);
    std::string rowText;
    int row = 0;
    while (std::getline(rowStream, rowText, '/')) {
        if (row >= kBoardSize) {
            throw std::runtime_error("--board has too many rows");
        }
        std::stringstream cellStream(rowText);
        std::string cellText;
        int col = 0;
        while (std::getline(cellStream, cellText, ',')) {
            if (col >= kBoardSize) {
                throw std::runtime_error("--board row has too many cells");
            }
            values[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] = std::stoi(cellText);
            ++col;
        }
        if (col != kBoardSize) {
            throw std::runtime_error("--board row must have four cells");
        }
        ++row;
    }
    if (row != kBoardSize) {
        throw std::runtime_error("--board must have four rows");
    }
    return values;
}

int RunBench(const CliOptions& options) {
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

int RunAnalyze(const CliOptions& options) {
    Board board = options.boardRows.has_value()
        ? Board::FromRows(ParseBoardRows(*options.boardRows))
        : Game(options.seed).GetBoard();

    ai::AIEngine engine;
    engine.SetAgent(options.agent);
    engine.Expectimax().SetConfig(options.search);
    const auto decision = engine.Recommend(FastBoard::FromReference(board));
    const auto breakdown = engine.Expectimax().GetEvaluator().Breakdown(FastBoard::FromReference(board));

    std::cout << "valid=" << (decision.valid ? "true" : "false")
              << " direction=" << static_cast<int>(decision.direction)
              << " nodes=" << decision.stats.nodes
              << " cache_hits=" << decision.stats.cacheHits
              << " depth=" << decision.stats.maxDepthReached
              << " elapsed_ms=" << decision.stats.elapsedMs
              << " evaluation=" << decision.stats.evaluation
              << " breakdown_total=" << breakdown.total
              << '\n';
    return decision.valid ? 0 : 2;
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
        if (options.command == CliCommand::Bench) {
            return RunBench(options);
        }
        if (options.command == CliCommand::Analyze) {
            return RunAnalyze(options);
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
    std::uint64_t animatedMoveRevision = static_cast<std::uint64_t>(-1);

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

        snapshot = runtime.Tick(events, rawInput.nowSeconds);
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
