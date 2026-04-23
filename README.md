# 2048 AI Lab

A C++17 + raylib + CMake implementation of 2048 as a board-centered AI laboratory. The deterministic game core remains isolated, AI search runs through a search/benchmark stack, and interactive play is coordinated by a pure C++ event/snapshot runtime.

- A deterministic game core with reference and fast board representations
- A search-oriented AI / benchmark stack centered on expectimax
- A runtime layer that consumes events and emits immutable snapshots
- A raylib frontend that renders the Board-Centered AI Lab

Benchmark and analyze modes run headless and never depend on the render loop.

## Features

- Standard 4x4 2048 rules with correct merge semantics
- Deterministic seeding for tests, AI experiments, and benchmarks
- Reference board for readability and debugging
- 64-bit fast board with precomputed 16-bit row transition tables
- Human play, greedy AI, expectimax AI
- Iterative deepening, time-budget search, move ordering, transposition table
- Benchmark mode with summary stats and optional CSV export
- raylib UI with adaptive Board-Centered Lab layout
- Unified multi-input layer for keyboard, mouse, touch, and gamepad
- Undo, same-seed restart, modal help, autoplay, single-step AI
- Runtime event/snapshot model for overlays, control mode, and AI handoff
- Background AI worker with board-revision stale result rejection
- Lightweight unit test suite covering move correctness, equivalence, evaluator behavior, runtime events, CLI parsing, and seed reproducibility

## Project Layout

```text
game2048/
  CMakeLists.txt
  README.md
  src/
    main.cpp
    shared/
      profiler.h
      stats.h / stats.cpp
    ai/
      ai_engine.h / ai_engine.cpp
      benchmark.h / benchmark.cpp
      evaluator.h / evaluator.cpp
      expectimax.h / expectimax.cpp
      greedy.h / greedy.cpp
      transposition_table.h / transposition_table.cpp
    app/
      app.h / app.cpp
      cli_options.h / cli_options.cpp
    runtime/
      ai_worker.h / ai_worker.cpp
      runtime_engine.h / runtime_engine.cpp
      runtime_types.h
    core/
      board.h / board.cpp
      board_fast.h / board_fast.cpp
      config.h
      game.h / game.cpp
      rng.h / rng.cpp
    input/
      gamepad_input.h / gamepad_input.cpp
      input.h / input.cpp
      input_bindings.h / input_bindings.cpp
      input_source.h / input_source.cpp
      input_system.h / input_system.cpp
      input_types.h
      pointer_input.h / pointer_input.cpp
    ui/
      animation.h / animation.cpp
      layout.h / layout.cpp
      names.h
      overlays.h / overlays.cpp
      panels.h / panels.cpp
      renderer.h / renderer.cpp
      theme.h
      ui.h / ui.cpp
      widgets.h / widgets.cpp
  tests/
    test_framework.h
    test_main.cpp
    test_runtime_engine.cpp
    test_moves.cpp
    test_board_equivalence.cpp
    test_eval.cpp
    test_game.cpp
    test_pointer_input.cpp
    test_gamepad_input.cpp
    test_input_system.cpp
```

## Build

The build uses `find_package(raylib)`.

```bash
cmake -S . -B build
cmake --build build --target game2048
cmake --build build --target game2048_tests
```

If CMake cannot find raylib automatically, set one of these:

- `raylib_ROOT` to a raylib install or source tree root that already has a built raylib library
- `raylib_DIR` to a raylib CMake config directory when you want to point directly at package metadata
- `CMAKE_PREFIX_PATH` to a prefix containing raylib

Example:

```bash
cmake -S . -B build -Draylib_ROOT=/path/to/raylib
cmake --build build --target game2048
```

## Run

Interactive mode:

```bash
./build/game2048
./build/game2048 play --seed 123
./build/game2048 play --ai greedy --depth 2 --time-budget-ms 5
```

Benchmark mode:

```bash
./build/game2048 bench --games 100
./build/game2048 bench --games 200 --seed 123 --ai expectimax --time-budget-ms 10
./build/game2048 bench --games 200 --ai greedy --csv out.csv
```

Analyze mode:

```bash
./build/game2048 analyze --seed 123 --ai expectimax
./build/game2048 analyze --board "2,0,0,0/0,4,0,0/0,0,8,0/0,0,0,16" --ai greedy
```

## Controls

- `Arrow Keys` / `WASD`: move
- `Mouse`: click on-screen controls
- `Touch`: swipe on the board or tap touch HUD controls
- `Gamepad`: D-pad / left stick move, `A` single-step AI / confirm, `B` dismiss / exit, `X` undo, `Y` restart
- `R`: restart with the current seed
- `U`: undo
- `Space`: toggle autoplay
- `N`: execute one AI move
- `Tab`: switch greedy / expectimax AI
- `T`: cycle animation speed (`Normal`, `Slow`, `Turbo`)
- `H` / `F1`: help
- `Esc`: dismiss win/help overlays or exit

Interaction notes:

- Reaching `2048` shows a one-shot victory overlay; the game continues after dismissal.
- Move keys dismiss the victory overlay and immediately execute that move.
- `Help` is modal and does not buffer hidden moves behind the panel.
- On touch-capable interaction, the layout switches to an expanded touch HUD and keeps it active for the rest of the session.

## Architecture

### 1. Layered modules

The codebase is organized around explicit layers instead of one flat `src/` bucket:

- `src/core`: deterministic rules, board state, RNG, and gameplay model
- `src/ai`: search, evaluation, benchmark, and AI engine selection
- `src/runtime`: pure C++ event processing, snapshots, control policy, and background AI worker
- `src/input`: raw device polling plus normalized input routing
- `src/ui`: layout, renderer, panels, overlays, animation, and UI metadata
- `src/app`: CLI parsing and thin raylib application shell

Dependency direction is intentionally narrow:

- `core` depends on no higher layer
- `ai` depends on `core`
- `runtime` depends on `core` and `ai`
- `shared` contains cross-cutting value types and utilities that may be reused by multiple layers without becoming a new behavior-owning subsystem
- `input` depends on runtime overlay enums and UI layout geometry, but not on the full app loop
- `ui` depends on immutable runtime snapshots, not on `Game`, `AIEngine`, or `RuntimeEngine` internals
- `app` wires raylib input/rendering to runtime events and snapshots

### 2. Include policy

All project targets now include only `src/` as the project include root. Internal headers are referenced with explicit root-relative paths such as:

- `core/board.h`
- `runtime/runtime_engine.h`
- `input/input_system.h`
- `ui/layout.h`

This avoids relying on CMake to expose each subdirectory as a fallback include root and makes cross-layer dependencies visible in code review.

### 3. Logic / Render split

- `board.*`, `board_fast.*`, `game.*`, `rng.*`, `stats.*`, and the AI stack are raylib-free.
- `renderer.*`, `ui.*`, `layout.*`, `input.*`, `animation.*`, and `app.cpp` are the raylib-facing pieces.
- `input_source.*` polls raw raylib state, while `pointer_input.*`, `gamepad_input.*`, and `input_system.*` normalize devices into a single per-frame `InputFrame`.
- `RuntimeEngine` owns gameplay state, overlay/control policy, board revisions, and snapshot production.
- `AIWorker` owns background recommendation search and returns revision-tagged results.
- The UI consumes immutable `RuntimeSnapshot` values. It does not decide rules or call AI services.

### 4. Dual board model

`Board` is the readable reference model:

- `std::array<int, 16>`
- stores tile values directly (`0, 2, 4, 8, ...`)
- used for gameplay logic, move trace generation, tests, and rendering

`FastBoard` is the AI / benchmark model:

- 64-bit bitboard
- each cell stores `log2(tile)` in 4 bits, empty is `0`
- supports cheap copies, hashing, move generation, and table-driven row transitions

### 5. Game state

`Game` owns:

- current board
- deterministic RNG
- score
- 2048 reached flag
- game-over flag
- undo history

Spawning stays outside raw board movement. A tile is only spawned after a valid move.

`RuntimeEngine` owns:

- control mode (`Human`, `AIAutoplay`, `AISingleStep`)
- overlay mode (`None`, `Help`, `Victory`, `GameOver`)
- input gate (`Accepting`, `BlockedByOverlay`, `BlockedByAnimation`)
- board revision and latest AI recommendation revision
- stale AI result rejection

This keeps overlay behavior and AI pause/resume rules out of `Game`.

### 6. Unified input flow

The input stack is layered so new devices can be added without touching rule code:

- `InputSource` gathers raw keyboard, pointer, touch, and gamepad state from raylib.
- `PointerInputRouter` resolves swipe gestures and control hit-tests.
- `GamepadInputRouter` handles button mapping and analog-stick debounce.
- `InputSystem` collapses all active devices into a single `InputFrame` per render tick.
- `app.cpp` maps the normalized frame into `RuntimeEvent` values.
- `RuntimeEngine` consumes events and applies overlay, control-mode, and autoplay policy.

This keeps device-specific ambiguity out of the gameplay state machine and makes the routing logic unit-testable without raylib globals.

## Board Implementation

### Reference board semantics

Moves follow the standard 2048 rule:

1. compress toward the move direction
2. merge equal adjacent tiles once
3. compress again

Examples covered by tests:

- `[2, 2, 2, 0] -> [4, 2, 0, 0]`
- `[2, 2, 2, 2] -> [4, 4, 0, 0]`
- `[4, 0, 4, 4] -> [8, 4, 0, 0]`
- `[2, 0, 2, 2] -> [4, 2, 0, 0]`

`Board::ApplyMove()` also emits a `MoveTrace` for slide / merge animation.

### Fast board mechanics

The fast representation uses precomputed row tables for all `2^16` 4-cell rows:

- left-move result row
- right-move result row
- score delta per row

This lets horizontal moves run as four table lookups plus row packing. Vertical moves are implemented as transpose + horizontal move + transpose back.

## AI Design

### Agents

- `GreedyAgent`: baseline policy using immediate move score + evaluator
- `ExpectimaxAgent`: main agent for autoplay and benchmark
- `AIEngine`: runtime switch between the two

### Expectimax tree

- Max nodes expand valid player moves
- Chance nodes expand `(empty cell, spawn value)` outcomes
- Spawn probabilities match game rules exactly: `0.9` for `2`, `0.1` for `4`
- Invalid moves are skipped

### Search features

- fixed max depth
- iterative deepening
- time-budget search
- move ordering from immediate score + heuristic
- fixed-size transposition table
- optional approximate chance-node expansion

Approximate chance expansion is off by default because it trades exact expectation for speed. When enabled, only the highest-impact empty cells are expanded and the subset is renormalized.

## Evaluator

The evaluator is modular and exposes a per-feature breakdown:

- empty tiles
- monotonicity
- smoothness
- max tile in corner
- merge potential
- weighted snake pattern (4 templates, best selected)
- trap penalty

All feature toggles and weights live in `src/core/config.h` and `src/ai/evaluator.h`.

## Performance Notes

- 16-bit row transition tables avoid per-node line simulation
- bitboard copies are trivial
- expectimax reuses a fixed-size transposition table
- move ordering reduces wasted deep branches
- benchmark mode is fully headless
- benchmark output exposes nodes per move and average think time per move

## Benchmark Output

Current benchmark summary includes:

- number of games
- average / median / P90 / P99 score
- best / worst score
- average max tile
- average moves
- average nodes per move
- average think time per move
- total elapsed time
- max tile distribution
- achievement rates for `1024 / 2048 / 4096 / 8192`

CSV output writes one row per game:

```text
seed,score,max_tile,moves,nodes,think_time_ms
```

## Testing

Build and run:

```bash
cmake --build build --target game2048_tests
./build/game2048_tests
ctest --test-dir build --output-on-failure
```

Input-specific coverage includes:

- swipe threshold and dominant-axis resolution
- gamepad stick recenter / debounce behavior
- single-frame intent collapsing
- overlay-specific gamepad command semantics
- adaptive touch HUD layout exposure
- runtime event dispatch and snapshot updates
- stale AI recommendation rejection by board revision
- CLI subcommand parsing

Covered areas:

- move / merge correctness
- invalid move no-op behavior
- score accumulation
- game-over detection
- runtime control/overlay state transitions
- reference vs fast board equivalence
- evaluator ordering sanity
- deterministic seed reproducibility

## Current Status

- Core rules compile and pass tests
- Headless benchmark mode runs
- Interactive app builds and runs with raylib
- Evaluator breakdown is visible in the side panel

## Extension Ideas

- deeper benchmark reports with per-move CSV
- MCTS / rollout baseline
- 5x5 or other board sizes
- persistent settings file
- richer animation interpolation and sound
- offline tuning of evaluator weights
- exact principal variation reporting
