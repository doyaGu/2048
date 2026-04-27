# 2048 AI Lab

A C++17 + raylib + CMake implementation of 2048 as a board-centered AI laboratory. The deterministic game core remains isolated, AI search runs through a search/benchmark/training stack, and interactive play is coordinated by a pure C++ event/snapshot runtime.

- A deterministic game core with reference and fast board representations
- A search-oriented AI / benchmark / n-tuple training stack centered on expectimax
- A runtime layer that consumes events and emits immutable snapshots
- A raylib frontend that renders the Board-Centered AI Lab

Training, benchmark, matrix sweep, and inspect modes are available through a raylib-free headless CLI for Linux servers.

## Features

- Standard 4x4 2048 rules with correct merge semantics
- Deterministic seeding for tests, AI experiments, and benchmarks
- Reference board for readability and debugging
- 64-bit fast board plus 80-bit packed board support for high-rank AI evaluation
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
    shared/
      profiler.h
      stats.h / stats.cpp
    core/
      board.h / board.cpp
      board_fast.h / board_fast.cpp
      config.h
      game.h / game.cpp
      rng.h / rng.cpp
    value/
      ntuple.h / ntuple.cpp
      ntuple_kernel.h / ntuple_kernel.cpp
    search/
      ai_engine.h / ai_engine.cpp
      evaluator.h / evaluator.cpp
      expectimax.h / expectimax.cpp
      greedy.h / greedy.cpp
      transposition_table.h / transposition_table.cpp
    training/
      benchmark.h / benchmark.cpp
      selection.h / selection.cpp
    experiment/
      profile.h / profile.cpp
      runner.h / runner.cpp
    cli/
      cli_app.h / cli_app.cpp
      cli_options.h / cli_options.cpp
      main_cli.cpp
    gui/
      app.h / app.cpp
      main.cpp
      runtime_event_mapper.h / runtime_event_mapper.cpp
    runtime/
      ai_worker.h / ai_worker.cpp
      runtime_engine.h / runtime_engine.cpp
      runtime_types.h
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
  profiles/
    smoke.toml
    ntuple_server.toml
  tests/
    test_framework.h
    test_main.cpp
    test_app_cli.cpp
    test_cli_options.cpp
    test_experiment_profile.cpp
    test_runtime_engine.cpp
    test_runtime_event_mapper.cpp
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
cmake --build build --target game2048_gui
cmake --build build --target game2048_tests
```

If CMake cannot find raylib automatically, set one of these:

- `raylib_ROOT` to a raylib install or source tree root that already has a built raylib library
- `raylib_DIR` to a raylib CMake config directory when you want to point directly at package metadata
- `CMAKE_PREFIX_PATH` to a prefix containing raylib

Example:

```bash
cmake -S . -B build -Draylib_ROOT=/path/to/raylib
cmake --build build --target game2048_gui
```

### Headless Server Build

Training and batch evaluation do not require raylib, X11, or a GUI session. Build only the server CLI target:

```bash
cmake -S . -B build-server -DGAME2048_BUILD_GUI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-server --target game2048_cli
```

For a machine-specific optimized binary, add `-DGAME2048_NATIVE_OPT=ON`. This enables native CPU flags such as `-march=native` in Release builds.

## Run

Interactive mode:

```bash
./build/game2048_gui
./build/game2048_gui play --seed 123
./build/game2048_gui play --ai greedy --depth 2 --time-budget-ms 5
```

Headless train and bench:

```bash
./build-server/game2048_cli train --profile profiles/smoke.toml
./build-server/game2048_cli bench --profile profiles/smoke.toml --weights artifacts/profile-smoke/best.weights
./build-server/game2048_cli matrix --profile profiles/smoke.toml --max-jobs 2
./build-server/game2048_cli microbench --profile profiles/smoke.toml --games 200
```

Server pipeline and concurrent matrix:

```bash
SMOKE=1 scripts/train_ntuple_pipeline.sh
MAX_JOBS=4 PROFILE=profiles/ntuple_server.toml scripts/server_train_matrix.sh
```

Training is profile-driven. Each run writes `config.toml`, `manifest.json`, `train.log`, `bench.txt`, `metrics.csv`, `phase-*.weights`, and `best.weights` under the profile artifact directory. Matrix runs summarize jobs in `summary.csv` and copy the selected global best to `best/best.weights`.

Profiles use the current V5 training schema. The relevant AI sections look like:

```toml
[search.downgrade]
enabled = true
steps = 2
floor_rank = 13

[value]
preset = "compact-d4" # or "tdl-4x6-khyeh", "tdl-8x6-kmatsuzaki"
optimistic_init = 200000.0
multistage = true
stage_boundaries = [11, 12, 13, 14, 15]

[value.storage]
mode = "dense-stage"
```

Weight files use the V5 `G2048NT5` chunked format and are not compatible with older V4 files.

Inspect mode:

```bash
./build-server/game2048_cli inspect --board "2,0,0,0/0,4,0,0/0,0,8,0/0,0,0,16"
```

`inspect --board` expects four slash-separated rows with four comma-separated integer tile values per row. Invalid cells report the row and column that failed to parse.

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
- `src/value`: n-tuple value function, weight IO, multistage promotion, and TC state
- `src/search`: evaluator, greedy policy, expectimax, tile downgrading, and AI engine selection
- `src/training`: benchmark runner and checkpoint selection policy
- `src/experiment`: TOML profile parsing, profile execution, matrix expansion, and artifact writing
- `src/cli`: raylib-free headless command adapter
- `src/gui`: raylib app shell; `runtime`, `input`, and `ui` remain GUI-facing support layers

Dependency direction is intentionally narrow:

- `core` depends on no higher layer
- `value` depends on `core`
- `search` depends on `value`
- `training` depends on `search`
- `experiment` depends on `training`
- `cli` depends on `experiment`
- `runtime` depends on `core` and `search`
- `shared` contains cross-cutting value types and utilities that may be reused by multiple layers without becoming a new behavior-owning subsystem
- `input` depends on runtime overlay enums and UI layout geometry, but not on the full app loop
- `ui` depends on immutable runtime snapshots, not on `Game`, `AIEngine`, or `RuntimeEngine` internals
- `gui` wires raylib input/rendering to runtime events and snapshots

### 2. Include policy

All project targets now include only `src/` as the project include root. Internal headers are referenced with explicit root-relative paths such as:

- `core/board.h`
- `runtime/runtime_engine.h`
- `input/input_system.h`
- `ui/layout.h`

This avoids relying on CMake to expose each subdirectory as a fallback include root and makes cross-layer dependencies visible in code review.

### 3. Logic / Render split

- `core`, `value`, `search`, `training`, `experiment`, and `cli` are raylib-free.
- `renderer.*`, `ui.*`, `layout.*`, `input.*`, `animation.*`, and `gui/app.cpp` are the raylib-facing pieces.
- `input_source.*` polls raw raylib state, while `pointer_input.*`, `gamepad_input.*`, and `input_system.*` normalize devices into a single per-frame `InputFrame`.
- `gui/runtime_event_mapper.*` translates each normalized `InputFrame` into runtime events and owns held-move repeat timing.
- `RuntimeEngine` owns gameplay state, overlay/control policy, board revisions, and snapshot production.
- `AIWorker` owns background recommendation search and returns revision-tagged results.
- The UI consumes immutable `RuntimeSnapshot` values. It does not decide rules or call AI services.

### 4. Dual board model

`Board` is the readable reference model:

- `std::array<int, 16>`
- stores tile values directly (`0, 2, 4, 8, ...`)
- used for gameplay logic, move trace generation, tests, and rendering

`FastBoard` is the common AI / benchmark model:

- 64-bit bitboard
- each cell stores `log2(tile)` in 4 bits, empty is `0`
- supports cheap copies, hashing, move generation, and table-driven row transitions

`PackedBoard` is the high-rank transform model:

- 80-bit packed board (`uint64_t raw` + `uint16_t ext`)
- each cell stores a 5-bit rank, so search/eval helpers can represent tiles beyond the 4-bit fast-board range
- uses 20-bit row transition tables for high-rank row moves
- exposes D4 transforms and conversion back to clamped `FastBoard` values for leaf evaluation

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
- `RuntimeEventMapper` maps the normalized frame into `RuntimeEvent` values.
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

The standard fast representation uses precomputed row tables for all `2^16` 4-cell rows:

- left-move result row
- right-move result row
- score delta per row

This lets horizontal moves run as four table lookups plus row packing. Vertical moves are implemented as transpose + horizontal move + transpose back.

The packed high-rank representation uses equivalent `2^20` row tables and is used for transform/downgrade behavior that needs ranks above `15`.

## AI Design

### Agents

- `GreedyAgent`: baseline policy using immediate move score + evaluator
- `ExpectimaxAgent`: main agent for autoplay and benchmark
- `NtupleAgent`: direct policy over the learned value function
- `AIEngine`: runtime switch between greedy, expectimax, and n-tuple agents

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
- D4 canonical transposition keys
- optional tile-downgrading transform for n-tuple leaf evaluation

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

All feature toggles and weights live in `src/core/config.h` and `src/search/evaluator.h`.

## Performance Notes

- row transition tables avoid per-node line simulation
- bitboard copies are trivial
- expectimax reuses a fixed-size transposition table
- move ordering reduces wasted deep branches
- benchmark mode is fully headless
- benchmark output exposes nodes, leaf evals, transposition hit rate, and average think time per move

## Benchmark Output

Current benchmark summary includes:

- number of games
- average / median / P90 / P99 score
- best / worst score
- average max tile
- average moves
- average nodes per move
- average leaf evals per move
- transposition hit rate
- average think time per move
- total elapsed time
- max tile distribution
- achievement rates for configured milestone tiles up to `32768`

CSV output writes one row per game:

```text
seed,score,max_tile,moves,nodes,cache_hits,leaf_evals,think_time_ms
```

## Testing

Build and run:

```bash
cmake --build build-server --target game2048_cli_tests
./build-server/game2048_cli_tests
cmake --build build --target game2048_tests
./build/game2048_tests
ctest --test-dir build --output-on-failure
ctest --test-dir build-server --output-on-failure
```

Input-specific coverage includes:

- swipe threshold and dominant-axis resolution
- gamepad stick recenter / debounce behavior
- single-frame intent collapsing
- overlay-specific gamepad command semantics
- adaptive touch HUD layout exposure
- runtime event dispatch and snapshot updates
- frontend input-to-runtime event mapping
- stale AI recommendation rejection by board revision
- CLI subcommand parsing
- CLI error messages for invalid inspect boards
- TOML profile parsing and matrix expansion
- profile-driven train/bench artifact generation

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
- Headless profile train/bench/matrix mode runs
- Interactive GUI builds and runs with raylib
- Evaluator breakdown is visible in the side panel

## Extension Ideas

- deeper benchmark reports with per-move CSV
- MCTS / rollout baseline
- 5x5 or other board sizes
- persistent settings file
- richer animation interpolation and sound
- offline tuning of evaluator weights
- exact principal variation reporting
