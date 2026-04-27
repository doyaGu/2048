# 2048 AI Lab Architecture

## Purpose

This project keeps deterministic 2048 rules, value/search logic, experiment execution, runtime policy, and raylib presentation separated so each slice can be tested independently. The interactive app is a Board-Centered AI Lab: the board remains primary, while the side panel shows recommendation state, search metrics, and evaluator features.

The main architectural rules are:

- core gameplay stays raylib-free
- value/search consumes core state and does not own runtime flow
- training and experiment code stay headless
- runtime consumes events and emits immutable snapshots
- UI consumes snapshots, not gameplay services
- gui is a thin raylib shell that maps input frames to runtime events

## Layer Map

### `src/core`

Owns deterministic rules and state primitives: `Board`, `FastBoard`, `PackedBoard`, `Game`, RNG, and constants. This layer must not depend on raylib, UI metadata, input devices, search, training, experiment execution, or runtime orchestration.

### `src/value`

Owns n-tuple value functions, presets, weight serialization, multistage storage, and fixed-pattern kernel dispatch. It depends on `core` and shared stats/types, and exposes value estimators used by search and training.

### `src/search`

Owns evaluator heuristics, greedy policy, expectimax, tile downgrading, transposition table, and AI engine selection. It depends on `core` and `value`, and must not know about overlays, window state, input routing, profile files, or artifact writing.

### `src/training`

Owns benchmark execution and training-result selection policy. It depends on `search` and shared benchmark stats, but remains raylib-free and independent of CLI parsing.

### `src/experiment`

Owns TOML profile parsing, profile execution, matrix expansion, and artifact writing for training, benchmark, inspect, and matrix workflows. It depends on `training`, `search`, and `value`, and is the highest headless orchestration layer.

### `src/cli`

Owns raylib-free command-line parsing and command dispatch for server workflows. `game2048_cli` uses this layer directly, while the GUI executable delegates non-`play` commands here before opening a window.

### `src/runtime`

Owns application policy without raylib:

- `RuntimeEvent`: reset, undo, move, autoplay, step AI, agent/speed cycling, overlays, quit
- `RuntimeSnapshot`: immutable board, score, revision, overlay/control/input state, AI status, recommendation, search stats, evaluator breakdown
- `RuntimeEngine`: applies events, owns `Game`, maintains board revisions, and publishes snapshots
- `AIWorker`: one background search worker with latest-request semantics and stale-result rejection

`runtime` depends on `core` and `search`. It must not include raylib, layout geometry, widgets, raw input polling, CLI parsing, or experiment artifact code.

### `src/input`

Owns raylib input normalization: raw polling, pointer gestures, gamepad debounce, bindings, and `InputFrame` collapse. It can depend on lightweight runtime overlay enums and UI layout geometry, but it never mutates gameplay directly.

### `src/ui`

Owns presentation and layout: renderer, animation, Board-Centered Lab layout, panels, overlays, widgets, theme, and display names. UI consumes `RuntimeSnapshot` and does not hold `Game`, `AIEngine`, or `RuntimeEngine`.

### `src/gui`

Owns the raylib composition shell for interactive play. `App` parses CLI options, delegates headless commands to `cli`, opens the raylib window for `play`, polls `InputSource`, uses `RuntimeEventMapper` to translate `InputFrame` values into `RuntimeEvent` values, ticks `RuntimeEngine`, and draws snapshots through `Renderer` and `UI`.

## Runtime Flow

1. `InputSource` polls raw keyboard, pointer, touch, and gamepad state.
2. `InputSystem` collapses that into one normalized `InputFrame`.
3. `RuntimeEventMapper` maps the frame into `RuntimeEvent` values.
4. `RuntimeEngine` applies events, updates gameplay state, and emits `RuntimeSnapshot`.
5. `AIWorker` computes recommendations on a background thread and publishes revision-tagged results.
6. `Renderer` and `UI` draw from the immutable snapshot.

Train, bench, matrix, microbench, and inspect modes bypass raylib entirely through `game2048_cli` or through `game2048_gui` before window initialization.

## Build Targets

- `game2048_core`: rules, boards, RNG, shared stats
- `game2048_value`: n-tuple value functions and evaluator support
- `game2048_search`: greedy, expectimax, transposition table, and AI engine
- `game2048_training`: benchmark runner and checkpoint selection
- `game2048_experiment`: profile parsing, matrix expansion, and artifact orchestration
- `game2048_cli_lib`: command parsing and headless command dispatch
- `game2048_cli`: raylib-free CLI executable
- `game2048_runtime`: event/snapshot runtime and background AI worker
- `game2048_gui_lib`: raylib input, UI, rendering, runtime-event mapping, and app shell
- `game2048_gui`: interactive raylib executable

Only `src/` is exported as the include root. Internal includes should remain root-relative, such as `core/board.h`, `runtime/runtime_engine.h`, `gui/runtime_event_mapper.h`, and `ui/layout.h`.

## Test Seams

- `RuntimeEngine` is testable from `RuntimeEvent` values without raylib.
- `RuntimeEventMapper` is testable from constructed `InputFrame` values without opening a window.
- `AIWorker` uses board revisions so stale-result behavior can be verified.
- `InputSystem` is testable from constructed `RawInputState` values.
- Core move/equivalence/evaluator tests validate rule and search correctness.
- CLI tests validate subcommand parsing for `play`, `train`, `bench`, `matrix`, `microbench`, and `inspect`.
- Experiment tests validate profile parsing, matrix expansion, selection policy, and artifact behavior.

## Forbidden Crossings

- `core` must not know about overlays, panels, input devices, raylib, search, training, or experiments.
- `value` and `search` must not own session state or UI/runtime control flow.
- `training` and `experiment` must stay raylib-free.
- `input` must not mutate gameplay directly.
- `ui` must not call `Game`, `AIEngine`, or `RuntimeEngine`.
- `gui` must not own gameplay policy beyond translating frontend input to runtime events and forwarding snapshots to rendering.
