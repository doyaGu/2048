# 2048 AI Lab Architecture

## Purpose

This project keeps deterministic 2048 rules, AI search, runtime policy, and raylib presentation separated so each slice can be tested independently. The interactive app is a Board-Centered AI Lab: the board remains primary, while the side panel shows recommendation state, search metrics, and evaluator features.

The main architectural rules are:

- core gameplay stays raylib-free
- AI consumes core state and does not own runtime flow
- runtime consumes events and emits immutable snapshots
- UI consumes snapshots, not gameplay services
- app is a thin raylib shell that maps input frames to runtime events

## Layer Map

### `src/core`

Owns deterministic rules and state primitives: `Board`, `FastBoard`, `Game`, RNG, and constants. This layer must not depend on raylib, UI metadata, input devices, or runtime orchestration.

### `src/ai`

Owns greedy search, expectimax, evaluation, transposition table, benchmark mode, and AI engine selection. It depends on `core` and `shared`, and should not know about overlays, window state, or input routing.

### `src/runtime`

Owns application policy without raylib:

- `RuntimeEvent`: reset, undo, move, autoplay, step AI, agent/speed cycling, overlays, quit
- `RuntimeSnapshot`: immutable board, score, revision, overlay/control/input state, AI status, recommendation, search stats, evaluator breakdown
- `RuntimeEngine`: applies events, owns `Game`, maintains board revisions, and publishes snapshots
- `AIWorker`: one background search worker with latest-request semantics and stale-result rejection

`runtime` depends on `core` and `ai`. It must not include raylib, layout geometry, widgets, or raw input polling.

### `src/input`

Owns raylib input normalization: raw polling, pointer gestures, gamepad debounce, bindings, and `InputFrame` collapse. It can depend on lightweight runtime overlay enums and UI layout geometry, but it never mutates gameplay directly.

### `src/ui`

Owns presentation and layout: renderer, animation, Board-Centered Lab layout, panels, overlays, widgets, theme, and display names. UI consumes `RuntimeSnapshot` and does not hold `Game`, `AIEngine`, or `RuntimeEngine`.

### `src/app`

Owns CLI parsing and the raylib composition shell. `app.cpp` selects `play`, `bench`, or `analyze`; in play mode it polls input, builds runtime events, ticks `RuntimeEngine`, and draws the returned snapshot.

## Runtime Flow

1. `InputSource` polls raw keyboard, pointer, touch, and gamepad state.
2. `InputSystem` collapses that into one normalized `InputFrame`.
3. `app.cpp` maps the frame into `RuntimeEvent` values.
4. `RuntimeEngine` applies events, updates gameplay state, and emits `RuntimeSnapshot`.
5. `AIWorker` computes recommendations on a background thread and publishes revision-tagged results.
6. `Renderer` and `UI` draw from the immutable snapshot.

Benchmark and analyze modes bypass raylib entirely.

## Build Targets

- `game2048_core`: rules, boards, RNG, shared stats
- `game2048_ai`: search, evaluator, benchmark, AI engine
- `game2048_runtime`: event/snapshot runtime and background AI worker
- `game2048_frontend`: raylib input, layout, rendering, app shell
- `game2048`: executable entrypoint

Only `src/` is exported as the include root. Internal includes should remain root-relative, such as `core/board.h`, `runtime/runtime_engine.h`, and `ui/layout.h`.

## Test Seams

- `RuntimeEngine` is testable from `RuntimeEvent` values without raylib.
- `AIWorker` uses board revisions so stale-result behavior can be verified.
- `InputSystem` is testable from constructed `RawInputState` values.
- Core move/equivalence/evaluator tests validate rule and search correctness.
- CLI tests validate subcommand parsing for `play`, `bench`, and `analyze`.

## Forbidden Crossings

- `core` must not know about overlays, panels, input devices, or raylib.
- `ai` must not own session state or UI/runtime control flow.
- `input` must not mutate gameplay directly.
- `ui` must not call `Game`, `AIEngine`, or `RuntimeEngine`.
- `app.cpp` must not own gameplay policy beyond translating frontend input to runtime events.
