#include "app.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

#include <raylib.h>

#include "ai/benchmark.h"
#include "animation.h"
#include "board_fast.h"
#include "game.h"
#include "input.h"
#include "interaction_session.h"
#include "layout.h"
#include "renderer.h"
#include "ui.h"

namespace game2048 {

namespace {

struct CliOptions {
    std::optional<std::size_t> benchmarkGames;
    std::uint64_t seed = kDefaultSeed;
    ai::AgentKind agent = ai::AgentKind::Expectimax;
    ai::SearchConfig search {};
    std::optional<std::string> csvPath;
};

std::filesystem::path BestScorePath() {
    return std::filesystem::current_path() / "best_score.txt";
}

std::uint32_t LoadBestScore() {
    std::ifstream in(BestScorePath());
    std::uint32_t value = 0;
    in >> value;
    return value;
}

void SaveBestScore(std::uint32_t score) {
    std::ofstream out(BestScorePath(), std::ios::trunc);
    out << score;
}

ai::AgentKind NextAgent(ai::AgentKind current) {
    return current == ai::AgentKind::Greedy ? ai::AgentKind::Expectimax : ai::AgentKind::Greedy;
}

AnimationSpeed NextSpeed(AnimationSpeed current) {
    switch (current) {
        case AnimationSpeed::Normal: return AnimationSpeed::Slow;
        case AnimationSpeed::Slow: return AnimationSpeed::Turbo;
        case AnimationSpeed::Turbo: return AnimationSpeed::Normal;
    }
    return AnimationSpeed::Normal;
}

CliOptions ParseArgs(int argc, char** argv) {
    CliOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        auto requireValue = [&](const char* name) -> std::string {
            if (index + 1 >= argc) {
                throw std::runtime_error(std::string("missing value for ") + name);
            }
            return argv[++index];
        };

        if (arg == "--benchmark") {
            options.benchmarkGames = static_cast<std::size_t>(std::stoul(requireValue("--benchmark")));
        } else if (arg == "--seed") {
            options.seed = std::stoull(requireValue("--seed"));
        } else if (arg == "--ai") {
            const auto value = requireValue("--ai");
            if (value == "greedy") {
                options.agent = ai::AgentKind::Greedy;
            } else if (value == "expectimax") {
                options.agent = ai::AgentKind::Expectimax;
            } else {
                throw std::runtime_error("unknown AI: " + value);
            }
        } else if (arg == "--depth") {
            options.search.maxDepth = std::stoi(requireValue("--depth"));
        } else if (arg == "--time-budget-ms") {
            options.search.timeBudgetMs = std::stoi(requireValue("--time-budget-ms"));
        } else if (arg == "--csv") {
            options.csvPath = requireValue("--csv");
        }
    }
    return options;
}

}  // namespace

int App::Run(int argc, char** argv) {
    CliOptions options;
    try {
        options = ParseArgs(argc, argv);
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

    Game game(options.seed);
    InteractionSession session(options.seed);
    ai::AIEngine engine;
    engine.SetAgent(options.agent);
    engine.Expectimax().SetConfig(options.search);

    AnimationController animation;
    Renderer renderer;
    UI ui;

    std::uint32_t bestScore = LoadBestScore();
    SearchStats lastSearch {};
    ai::MoveDecision recommendation {};
    bool recommendationDirty = true;

    auto invalidateRecommendation = [&]() {
        recommendation = {};
        lastSearch = {};
        recommendationDirty = true;
    };

    auto executeMove = [&](Direction direction, const SearchStats* searchStats) {
        const Board before = game.GetBoard();
        const bool wasGameOver = game.IsGameOver();
        const auto turn = game.ApplyMove(direction);
        if (!turn.moved) {
            return false;
        }

        animation.Start(before, game.GetBoard(), turn.trace, turn.spawn);
        if (searchStats != nullptr) {
            lastSearch = *searchStats;
        }

        if (game.Score() > bestScore) {
            bestScore = game.Score();
            SaveBestScore(bestScore);
        }

        recommendationDirty = true;
        if (!wasGameOver && game.IsGameOver()) {
            animation.TriggerShake(6.0F, 0.45F);
        }
        return true;
    };

    while (!WindowShouldClose()) {
        animation.Update(GetFrameTime());

        const bool animationBlocksInput = animation.Active() && animation.Speed() != AnimationSpeed::Turbo;

        InteractionInput input {};
        input.nowSeconds = GetTime();
        input.animationBlocksInput = animationBlocksInput;
        input.gameOver = game.IsGameOver();
        input.reached2048Ever = game.HasReached2048Ever();
        input.pressedMove = PollMoveInput();
        input.heldMove = PollMoveInputHeld();
        if (!animation.Active()) {
            input.command = PollCommandInput();
        }

        const auto actions = session.Tick(input);

        if (actions.exitRequested) {
            CloseWindow();
            return 0;
        }

        if (actions.cycleAgentRequested) {
            options.agent = NextAgent(options.agent);
            engine.SetAgent(options.agent);
            invalidateRecommendation();
        }

        if (actions.cycleAnimationRequested) {
            animation.SetSpeed(NextSpeed(animation.Speed()));
        }

        if (actions.resetRequested) {
            game.Reset(game.Seed());
            session.OnReset(game.Seed());
            invalidateRecommendation();
        }

        if (actions.undoRequested && game.Undo()) {
            invalidateRecommendation();
        }

        if (actions.hintInvalidated) {
            recommendationDirty = true;
        }

        if (actions.moveToExecute.has_value()) {
            executeMove(*actions.moveToExecute, nullptr);
        } else if (actions.aiMoveRequested && !game.IsGameOver()) {
            const auto decision = engine.Recommend(FastBoard::FromReference(game.GetBoard()));
            recommendation = decision;
            executeMove(decision.direction, &decision.stats);
        }

        if (recommendationDirty && !game.IsGameOver()) {
            recommendation = engine.Recommend(FastBoard::FromReference(game.GetBoard()));
            lastSearch = recommendation.stats;
            recommendationDirty = false;
        }

        const auto layout = ComputeLayout(GetScreenWidth(), GetScreenHeight());
        const auto breakdown = engine.Expectimax().GetEvaluator().Breakdown(FastBoard::FromReference(game.GetBoard()));
        const HUDState hud {
            game.Score(),
            bestScore,
            game.GetBoard().MaxTile(),
            game.IsGameOver(),
            game.HasReached2048Ever(),
            session.CanDismissOverlay(),
            session.ShowContinueHint(),
            game.Seed(),
            session.Control(),
            session.Overlay(),
            session.Gate(),
            options.agent,
            recommendation,
            lastSearch,
            breakdown,
            animation.Speed()
        };

        BeginDrawing();
        ClearBackground(RAYWHITE);
        renderer.DrawBoard(layout, game.GetBoard(), animation);
        ui.DrawPanels(layout, hud);
        ui.DrawOverlay(layout, hud);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}

}  // namespace game2048
