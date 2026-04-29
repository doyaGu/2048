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

Owns deterministic rules and state primitives. Key types:

- `Board` — reference 4×4 board using tile values, used for rendering and debugging
- `FastBoard` — 64-bit packed representation (4 bits per rank), primary AI and training surface; has `CanonicalKey()` for D4 transposition deduplication
- `PackedBoard` — 80-bit packed board (64-bit body + 16-bit extension), supports ranks beyond 15 for high-rank AI evaluation; `ToFastBoardClamped()` converts back with a configurable rank ceiling
- `Game` — owns a `Board`, `Random`, score, undo stack (up to `kUndoCapacity=128`), and terminal flags; `TurnResult` carries move trace and spawn event
- `Random` / `RNG` — deterministic seeding with snapshot/restore support
- `config.h` — global constants and `EvaluatorWeights` defaults; four snake-pattern weight tables for the evaluator

This layer must not depend on raylib, UI metadata, input devices, search, training, experiment execution, or runtime orchestration.

### `src/value`

Owns n-tuple value functions, AVX-512 kernel dispatch, and shared greedy/evaluator primitives. Key types:

- `NtupleNetwork` — stores weight tables, supports three presets (`CompactD4`, `Tdl4x6Khyeh`, `Tdl8x6KMatsuzaki`), optional D4-symmetry expansion, and multistage storage (stage indexed by `MaxRank()`). Methods: `Evaluate`, `UpdateToward`, `UpdateTowardFast`, `Save`/`Load`, `PromoteStageFromPrevious`. Has TC (temporal coherence) state tracking (`HasTcState`, `TouchedTcEntries`). Exposes `Fixed6SingleStageView` / `MutableFixed6SingleStageView` for fast-path kernel dispatch when the network is single-stage with fixed-6-cell patterns.
- `ntuple_kernel` — low-level primitives `EvaluateFixed6`, `CollectFixed6Keys`, `CollectFixed6KeysAndValue`; dispatches to AVX-512 or scalar path via `KernelKind` enum at build time (`GAME2048_ENABLE_AVX512`).
- `Evaluator` — heuristic board evaluator with 12 named features: emptyTiles, monotonicity, smoothness, cornerMax, mergePotential, snakePattern, trapPenalty, mobility, danger (all enabled by default), plus chainContinuity, endgameProgress, patternTable (optional, disabled by default). `Breakdown()` returns `FeatureBreakdown` for the lab side panel.
- `GreedyAgent` — one-ply greedy using `Evaluator`.

`game2048_value` depends on `game2048_core`. The `Evaluator` and `GreedyAgent` source files reside under `src/search/` but are compiled into the `game2048_value` build target because greedy and basic evaluation are also needed by TDL without pulling in the full search stack.

### `src/tdl`

Owns temporal-difference 2048 training and evaluation. The public entry points in `tdl/api.h` are:

- `ChooseTdlBestMove` — selects best afterstate for inference
- `TrainTdlForward` — runs forward-mode TDL training with alpha decay
- `EvaluateTdlBest` — greedy TDL rollout for evaluation

Shared types live in `tdl/types.h`: `TdlRandom` (mt19937 with board spawn helpers), `TdlCandidateMove`, `TdlForwardTrainingOptions`.

The training loop (`training_loop.h`) is a single **template function** `TrainTdlForwardWithBackend<Backend>` and `EvaluateTdlBestWithBackend<Backend>` that work with any backend satisfying the static-interface contract (`ChooseBest`, `Update`, `Valid`, `AfterstateBits`, `ScoreDelta`, `TargetValue`, `InitialStageUpdateCount`). The dispatch ladder in `api.cpp` selects among three concrete backends:

1. `Tdl8x6TdBackend` / `Tdl8x6EvalBackend` — fastest path; wraps `Tdl8x6Kernel` which operates directly on a `NtupleMutableFixed6View` / `NtupleFixed6View`. Supports only the TDL8x6KMatsuzaki pattern with a fixed 8-pattern × 6-cell layout. `Tdl8x6Kernel::Supports()` gates this path. `UpdateKnownValue()` avoids re-evaluating the board when the prior estimate is already known.
2. `Fixed6TdBackend` — generic fixed-6 TD backend for any single-stage network whose patterns all have exactly 6 cells. Dispatched when `Tdl8x6Kernel` does not apply but `Fixed6TdBackend::Supports()` returns true.
3. `NetworkTdlBackend` / `NetworkEvalBackend` — full network fallback, multistage-aware (`InitialStageUpdateCount` returns `max(1, StageCount())`). Handles arbitrary pattern sets, multistage promotion, and all `LearningMode` variants (TD, TC, OptimisticTD, OptimisticTC).

`kTdlMoveDirections` (Up, Right, Down, Left) in `move_order.h` is the canonical TDL move-ordering convention used for tie-breaking in `ChooseTdlBestMove`.

This layer depends on `game2048_value` (and transitively `game2048_core`). It is the only layer that dispatches between TDL backends.

### `src/search`

Owns expectimax, transposition table, and the `AIEngine` selector. Key types:

- `SearchConfig` — full search configuration struct:
  - depth / time budget / iterative deepening toggle
  - transposition table toggle and D4 `canonicalizeTranspositionKeys`
  - `approximateChanceNodes` + `maxChanceBranchesPerValue` + `preserveChanceProbabilityMass`
  - `adaptiveEndgameSearch` with `endgameMinRank`, `endgameDepthBonus`, `endgameMaxChanceBranchesPerValue`, `endgamePessimism`
  - `useRootRollout` / `rootRolloutDepth` / `rootRolloutWeight` for shallow-rollout blending at the root
  - `useTileDowngrading` / `useRootTileDowngrading` / `tileDowngradeSteps` / `tileDowngradeFloorRank`
  - `chanceDepthLimitByEmpty` — 17-element array capping chance-node depth by remaining empty-cell count
- `ExpectimaxAgent` — iterative-deepening expectimax with time budget; optional leaf `NtupleNetwork` (shared or owned) for n-tuple leaf evaluation; optional `leafPriorWeight` blending
- `TranspositionTable` — generation-tagged hash table; `ComposeSearchKey` builds keys from board bits, depth, node type, and optional D4 canonicalization
- `AIEngine` — thin selector that owns one `GreedyAgent`, one `ExpectimaxAgent`, and one `NtupleAgent`; routes `Recommend()` calls by `AgentKind`
- Free functions: `SelectChanceCellsForSearch`, `EndgameMoveSafetyPenalty`, `DowngradeTilesForSearch` (FastBoard and PackedBoard overloads), `PaperTileDowngradeRoot`

`MoveDecision` (defined in `search/greedy.h`) carries the chosen direction, validity flag, value estimate, and a `SearchStats` snapshot for the side panel.

`game2048_search` depends on `game2048_value`. It must not know about overlays, window state, input routing, profile files, or artifact writing.

### `src/training`

Owns benchmark execution and training-result selection policy. Key types:

- `BenchmarkRunner` — parallel game runner using `AIEngine`; options include `evalThreads`, per-game random seeds, optional CSV output, and `ntuplePriorWeight` for leaf-network blending
- `BenchmarkOptions` — configures games, seed strategy, agent, search config, optional weight path, shared network pointer, prior weight, thread count, and optional CSV path
- `SelectionPolicy` — chooses the best checkpoint by `SelectionMetric` (AverageScore or TileRate); `confidenceZ` applies a lower-confidence-bound penalty for noisy metrics; `confirmGames` triggers a re-evaluation run before committing a new best
- `IsBetterSelection` / `SelectionValue` — policy comparison helpers

`game2048_training` depends on `game2048_search` and remains raylib-free and independent of CLI parsing.

### `src/experiment`

Owns TOML profile parsing, training/benchmark/matrix/parity orchestration, and artifact writing. Key types:

- `ExperimentProfile` — top-level config object with sub-structs:
  - `RunConfig` (name, seed)
  - `SearchProfileConfig` (full search tuning including downgrade mode Leaf/Root, eval games, eval interval, progress interval)
  - `ValueProfileConfig` (preset, storage mode `dense-stage`, optimistic init, multistage boundaries)
  - `TrainingPhaseConfig` (per-phase: games, alpha, finalAlpha, epsilon, finalEpsilon, learningMode, updateOrder Online/Backward, priorWeight, startRank, replayStartRank, replayCaptureRank, enableMultistage)
  - `TrainerProfileConfig` (mode, games, progressIntervalGames, alpha, learningMode, checkpoints, fastPath)
  - `EvalProfileConfig` (mode, games, parityTolerance, referenceCachePath)
  - `MatrixConfig` (seed list, prior-weight list for grid sweep)
  - `ArtifactConfig` (output directory)
- `RunTrainingProfile` / `RunBenchmarkProfile` / `RunMatrixProfile` / `RunParityProfile` — top-level orchestration functions; `RunMatrixProfile` fans out jobs up to `maxJobs` in parallel
- `BuildSearchConfig` — converts `SearchProfileConfig` to `ai::SearchConfig`
- `ParseExperimentProfileText` / `LoadExperimentProfile` / `ExpandMatrixJobs` / `SaveProfileCopy` — profile I/O

`game2048_experiment` depends on `game2048_training` and `game2048_tdl`, and is the highest headless orchestration layer.

### `src/cli`

Owns raylib-free command-line parsing and command dispatch for server workflows. CLI subcommands: `play`, `bench`, `inspect`, `train`, `matrix`, `microbench`, `parity`. `game2048_cli` uses this layer directly; the GUI executable delegates all non-`play` commands here before opening a window.

- `CliOptions` — seed, agent, search config, optional profile path, optional weight path, optional TDL binary path (for parity), optional board rows string (for inspect), maxJobs, microbench game count
- `ParseCliOptions` / `RunCliCommand` / `RunCli` — public API

### `src/runtime`

Owns application policy without raylib:

- `RuntimeEvent` — tagged union of event types: Reset, Undo, Move (with `Direction`), ToggleAutoplay, StepAI, CycleAgent, CycleAnimation, OpenHelp, CloseOverlay, Quit
- `RuntimeSnapshot` — immutable value type capturing full UI-visible state: board, score, maxTile, gameOver, reached2048, seed, boardRevision, quitRequested, controlMode (Human/AIAutoplay/AISingleStep), overlayMode (None/Help/Victory/GameOver), inputGate (Accepting/BlockedByOverlay/BlockedByAnimation), animationSpeed, agent kind, model label, AIStatus (Idle/Searching/Ready/Failed), recommendation (`MoveDecision`), recommendationRevision, `SearchStats`, `FeatureBreakdown` (for the lab panel), and optional `RuntimeMoveAnimation` (carries before/after boards, `MoveTrace`, optional spawn, gameOver flag)
- `RuntimeConfig` — seed, agent kind, `SearchConfig`, shared `NtupleNetwork` pointer, model label, optional initial board override
- `RuntimeEngine` — applies event vectors via `Tick()`; owns `Game`, `AIWorker`, board revision counter, undo history, victory overlay gate, and animation-blocking input gate; `SubmitRecommendationIfNeeded` deduplicates by `submittedRevision_`; `workerGeneration_` tracks config changes for stale-result rejection
- `AIWorker` — background thread; `Configure()` bumps generation; `Submit()` enqueues a board revision; `Poll()` consumes the completed result; `Busy()` remains true until `Poll()` drains the result

`runtime` depends on `game2048_core` and `game2048_search`. It must not include raylib, layout geometry, widgets, raw input polling, CLI parsing, or experiment artifact code.

### `src/input`

Owns raylib input normalization: raw polling, pointer gestures, gamepad debounce, key bindings, and per-frame `InputFrame` collapse. Key types:

- `RawInputState` — keyboard direction/command slots, up to 8 pointer states, up to 4 gamepad states, timestamp
- `InputFrame` — normalized per-frame result: `InputCommand`, optional `pressedMove`, optional `heldMove`, `primaryControl` (for touch HUD highlight)
- `InputSource` — polls raw raylib keyboard/mouse/touch/gamepad state each frame
- `InputSystem` — combines all input sources into one `InputFrame`; priority order is overlay pointer → overlay gamepad/keyboard → HUD control pointer → gamepad command → keyboard command → pointer gesture/move → gamepad move → keyboard held move
- `PointerInputRouter` — hit-tests `LayoutMetrics` control rects for tap targets; `PointerGestureState` tracks swipe start/current position
- `GamepadInputRouter` — debounce and analog-stick hold repeat

`input` depends on `runtime/runtime_types.h` (for `OverlayMode`, `ControlId`, `AnimationSpeed`) and `ui/layout.h` (for `LayoutMetrics` and overlay action rects). This compile-time coupling is wider than strictly necessary but does not violate behavioral isolation: `input` never mutates gameplay state directly.

### `src/ui`

Owns presentation and layout: `Renderer`, `AnimationController`, Board-Centered Lab layout, panels, overlays, and widgets. Key types:

- `LayoutMetrics` — computed each frame from screen size and touch-HUD flag; contains board rect, tile rects (16), score box rects, control rects (up to 16), overlay action rects (up to 3)
- `AnimationController` — drives tile slide progress (`SlideProgress`), spawn pop (`SpawnProgress`), per-tile merge scale (`MergeScale`), and spawn scale (`SpawnScale`); also drives screen shake (`TriggerShake` / `ShakeOffset`) triggered on game-over
- `Renderer` — draws the animated board using `AnimationController` and `LayoutMetrics`
- `UI` — draws side panels (score, AI status, recommendation, search stats, evaluator breakdown) and modal overlays from `RuntimeSnapshot`

UI depends on `runtime/runtime_types.h` for `RuntimeSnapshot` and related enums. It does not hold `Game`, `AIEngine`, or `RuntimeEngine`.

### `src/gui`

Owns the raylib composition shell for interactive play. `App::Run` parses CLI options, delegates headless commands to `RunCliCommand`, then opens the raylib window for `play`, runs the main loop:

1. `AnimationController::Update(GetFrameTime())`
2. `InputSource::Poll()` → `InputSystem::BuildFrame()` → `RuntimeEventMapper::BuildEvents()`
3. `RuntimeEngine::Tick()` → `RuntimeSnapshot`
4. `AnimationController::Start()` when `lastMove` revision is new; `TriggerShake` on game-over
5. `BeginDrawing()` → `Renderer::DrawBoard()` → `UI::DrawPanels()` → `UI::DrawOverlay()` → `EndDrawing()`

`RuntimeEventMapper` translates `InputFrame` values into `RuntimeEvent` vectors and owns held-move repeat timing (initial delay + repeat rate).

## Runtime Flow

1. `InputSource` polls raw keyboard, pointer, touch, and gamepad state.
2. `InputSystem` collapses that into one normalized `InputFrame` with priority ordering.
3. `RuntimeEventMapper` maps the frame into `RuntimeEvent` values and manages held-move repeat timing.
4. `RuntimeEngine` applies events, updates gameplay state, maintains board revisions, and emits an immutable `RuntimeSnapshot`.
5. `AIWorker` computes recommendations on a background thread; results are tagged with revision and generation and rejected if stale.
6. `AnimationController` drives tile slide, merge pop, spawn pop, and game-over screen shake from `lastMove` in the snapshot.
7. `Renderer` and `UI` draw from the immutable snapshot and animation state.

Train, bench, matrix, microbench, parity, and inspect modes bypass raylib entirely through `game2048_cli` or through `game2048_gui` before window initialization.

## Build Targets

- `game2048_core`: `Board`, `FastBoard`, `PackedBoard`, `Game`, `Random`, shared stats
- `game2048_value`: `NtupleNetwork`, `ntuple_kernel` (with optional AVX-512), `Evaluator`, `GreedyAgent` — note that `evaluator.cpp` and `greedy.cpp` reside in `src/search/` but are compiled into this target
- `game2048_tdl`: TDL public API (`api.h`), training loop template, `Tdl8x6Kernel`, three concrete backends, `TdlRandom`
- `game2048_search`: `ExpectimaxAgent`, `TranspositionTable`, `AIEngine` — depends on `game2048_value`
- `game2048_training`: `BenchmarkRunner`, `SelectionPolicy` — depends on `game2048_search`
- `game2048_experiment`: profile parsing, `BuildSearchConfig`, training/benchmark/matrix/parity runners — depends on `game2048_training` and `game2048_tdl`
- `game2048_cli_lib`: `ParseCliOptions`, `RunCliCommand` — depends on `game2048_experiment`
- `game2048_cli`: raylib-free CLI executable
- `game2048_runtime`: `RuntimeEngine`, `AIWorker` — depends on `game2048_search`
- `game2048_gui_lib`: `InputSource`/`InputSystem`/`InputBindings`/`PointerInput`/`GamepadInput`, `AnimationController`, `LayoutMetrics`, `Renderer`, `UI`, panels, overlays, widgets, `App`, `RuntimeEventMapper` — depends on `game2048_runtime`, `game2048_cli_lib`, and raylib
- `game2048_gui`: interactive raylib executable

AVX-512 is enabled when `GAME2048_ENABLE_AVX512=ON` (default) and the target is `x86_64`. LTO is opt-in via `GAME2048_ENABLE_LTO`. Native-CPU optimization for release builds is opt-in via `GAME2048_NATIVE_OPT`.

Only `src/` is exported as the include root. Internal headers use root-relative paths: `core/board.h`, `runtime/runtime_engine.h`, `gui/runtime_event_mapper.h`, `ui/layout.h`.

## Test Seams

- `RuntimeEngine` is testable from `RuntimeEvent` values without raylib.
- `RuntimeEventMapper` is testable from constructed `InputFrame` values without opening a window.
- `AIWorker` uses board revisions and generation counters so both stale-result rejection and busy/poll semantics can be verified.
- `InputSystem` is testable from constructed `RawInputState` values.
- Core move/equivalence/evaluator tests validate rule and search correctness.
- CLI tests validate subcommand parsing for `play`, `train`, `bench`, `matrix`, `microbench`, `inspect`, and `parity`.
- Experiment tests validate profile parsing (including all phase fields), matrix expansion, selection policy with confidence bounds, and artifact behavior.

## Performance Validation

TDL throughput acceptance uses remote same-session control/candidate runs on `touyou@touyou.org`. Local MacBook Air measurements are only smoke checks because thermal variability can swamp small changes. Structural TDL refactors must preserve the canonical TDL8x6 hot path unless a remote candidate median stays within the accepted regression budget.

## Forbidden Crossings

- `core` must not know about overlays, panels, input devices, raylib, search, training, or experiments.
- `value`, `tdl`, and `search` must not own session state or UI/runtime control flow.
- `value` must not dispatch TDL training policy; TDL-specific backend selection belongs in `tdl`.
- `training` and `experiment` must stay raylib-free.
- `input` must not mutate gameplay directly.
- `ui` must not call `Game`, `AIEngine`, or `RuntimeEngine`.
- `gui` must not own gameplay policy beyond translating frontend input to runtime events and forwarding snapshots to rendering.
