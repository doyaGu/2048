# 2048 Engine

A maintainable C++17 + raylib + CMake implementation of 2048 built as two connected subsystems:

- A deterministic game core with reference and fast board representations
- A search-oriented AI / benchmark stack centered on expectimax

The project is designed for correctness first, then observability and speed. Rendering is a thin layer over a pure logic core. Benchmark mode runs headless and never depends on the render loop.

## Features

- Standard 4x4 2048 rules with correct merge semantics
- Deterministic seeding for tests, AI experiments, and benchmarks
- Reference board for readability and debugging
- 64-bit fast board with precomputed 16-bit row transition tables
- Human play, greedy AI, expectimax AI
- Iterative deepening, time-budget search, move ordering, transposition table
- Benchmark mode with summary stats and optional CSV export
- raylib UI with adaptive layout, overlays, slide / merge / spawn animation
- Undo, help overlay, autoplay, single-step AI
- Lightweight unit test suite covering move correctness, equivalence, evaluator behavior, and seed reproducibility

## Project Layout

```text
game2048/
  CMakeLists.txt
  README.md
  src/
    main.cpp
    config.h
    app.h / app.cpp
    game.h / game.cpp
    board.h / board.cpp
    board_fast.h / board_fast.cpp
    rng.h / rng.cpp
    input.h / input.cpp
    renderer.h / renderer.cpp
    animation.h / animation.cpp
    layout.h / layout.cpp
    ui.h / ui.cpp
    stats.h / stats.cpp
    profiler.h
    ai/
      ai_engine.h / ai_engine.cpp
      expectimax.h / expectimax.cpp
      greedy.h / greedy.cpp
      evaluator.h / evaluator.cpp
      transposition_table.h / transposition_table.cpp
      benchmark.h / benchmark.cpp
  tests/
    test_framework.h
    test_main.cpp
    test_moves.cpp
    test_board_equivalence.cpp
    test_eval.cpp
    test_game.cpp
```

## Build

This repository expects a local raylib source checkout at `../raylib`. The supplied `CMakeLists.txt` builds raylib as a subdirectory and then links the game against it.

```bash
cmake -S . -B build
cmake --build build --target game2048
cmake --build build --target game2048_tests
```

## Run

Interactive mode:

```bash
./build/game2048
./build/game2048 --seed 123
./build/game2048 --ai greedy
```

Benchmark mode:

```bash
./build/game2048 --benchmark 100
./build/game2048 --benchmark 200 --seed 123 --ai expectimax --time-budget-ms 10
./build/game2048 --benchmark 200 --ai greedy --csv out.csv
```

## Controls

- `Arrow Keys` / `WASD`: move
- `R`: restart
- `U`: undo
- `Space`: toggle autoplay
- `N`: execute one AI move
- `Tab`: switch greedy / expectimax AI
- `T`: cycle animation speed (`Normal`, `Slow`, `Turbo`)
- `H` / `F1`: help
- `Esc`: close help or exit

## Architecture

### 1. Logic / Render split

- `board.*`, `board_fast.*`, `game.*`, `rng.*`, `stats.*`, and the AI stack are raylib-free.
- `renderer.*`, `ui.*`, `layout.*`, `input.*`, and `animation.*` are the only raylib-facing pieces.
- The UI consumes immutable game state snapshots plus move traces. It does not decide rules.

### 2. Dual board model

`Board` is the readable reference model:

- `std::array<int, 16>`
- stores tile values directly (`0, 2, 4, 8, ...`)
- used for gameplay logic, move trace generation, tests, and rendering

`FastBoard` is the AI / benchmark model:

- 64-bit bitboard
- each cell stores `log2(tile)` in 4 bits, empty is `0`
- supports cheap copies, hashing, move generation, and table-driven row transitions

### 3. Game state

`Game` owns:

- current board
- deterministic RNG
- score
- 2048 reached flag
- game-over flag
- undo history

Spawning stays outside raw board movement. A tile is only spawned after a valid move.

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

All feature toggles and weights live in `src/config.h` and `src/ai/evaluator.h`.

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

Covered areas:

- move / merge correctness
- invalid move no-op behavior
- score accumulation
- game-over detection
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
