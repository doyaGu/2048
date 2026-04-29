#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_GEN_DIR="${BUILD_GEN_DIR:-"$ROOT_DIR/build-server-pgo-generate"}"
BUILD_USE_DIR="${BUILD_USE_DIR:-"$ROOT_DIR/build-server-pgo-use"}"
ARTIFACT_DIR="${ARTIFACT_DIR:-"$ROOT_DIR/artifacts/pgo"}"
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
GAME2048_NATIVE_OPT="${GAME2048_NATIVE_OPT:-ON}"
GAME2048_ENABLE_LTO="${GAME2048_ENABLE_LTO:-OFF}"
MICRO_PROFILE="${MICRO_PROFILE:-"$ROOT_DIR/profiles/smoke.toml"}"
MICRO_GAMES="${MICRO_GAMES:-200}"
MICRO_REPEATS="${MICRO_REPEATS:-3}"
RUN_PARITY="${RUN_PARITY:-1}"
PARITY_PROFILE="${PARITY_PROFILE:-"$ROOT_DIR/profiles/tdl_parity_smoke.toml"}"
RUN_USE_BENCH="${RUN_USE_BENCH:-1}"
RUN_USE_PARITY="${RUN_USE_PARITY:-0}"

absolute_path() {
  case "$1" in
    /*) printf '%s\n' "$1" ;;
    *) printf '%s\n' "$ROOT_DIR/$1" ;;
  esac
}

BUILD_GEN_DIR="$(absolute_path "$BUILD_GEN_DIR")"
BUILD_USE_DIR="$(absolute_path "$BUILD_USE_DIR")"
ARTIFACT_DIR="$(absolute_path "$ARTIFACT_DIR")"
MICRO_PROFILE="$(absolute_path "$MICRO_PROFILE")"
PARITY_PROFILE="$(absolute_path "$PARITY_PROFILE")"

if [[ -n "${LLVM_PROFDATA:-}" ]]; then
  PROFDATA="$LLVM_PROFDATA"
elif command -v llvm-profdata >/dev/null 2>&1; then
  PROFDATA="$(command -v llvm-profdata)"
else
  PROFDATA="$(xcrun --find llvm-profdata 2>/dev/null || true)"
fi

if [[ -z "$PROFDATA" || ! -x "$PROFDATA" ]]; then
  echo "llvm-profdata not found; set LLVM_PROFDATA=/path/to/llvm-profdata" >&2
  exit 2
fi

TIMESTAMP="$(date -u +%Y%m%dT%H%M%SZ)"
RUN_DIR="$ARTIFACT_DIR/pgo-$TIMESTAMP"
RAW_DIR="$RUN_DIR/raw"
LOG_DIR="$RUN_DIR/logs"
PROFDATA_FILE="$RUN_DIR/game2048.profdata"
mkdir -p "$RAW_DIR" "$LOG_DIR"

GEN_FLAGS="-O3 -DNDEBUG -fprofile-instr-generate=$RAW_DIR/game2048-%p.profraw"
USE_FLAGS="-O3 -DNDEBUG -fprofile-instr-use=$PROFDATA_FILE -Wno-profile-instr-unprofiled -Wno-profile-instr-out-of-date"

cmake -S "$ROOT_DIR" -B "$BUILD_GEN_DIR" \
  -DGAME2048_BUILD_GUI=OFF \
  -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
  -DGAME2048_NATIVE_OPT="$GAME2048_NATIVE_OPT" \
  -DGAME2048_ENABLE_LTO="$GAME2048_ENABLE_LTO" \
  -DCMAKE_CXX_FLAGS_RELEASE="$GEN_FLAGS" \
  -DCMAKE_EXE_LINKER_FLAGS_RELEASE="-fprofile-instr-generate=$RAW_DIR/game2048-%p.profraw"
cmake --build "$BUILD_GEN_DIR" --target game2048_cli -j

GEN_CLI="$BUILD_GEN_DIR/game2048_cli"
for ((run = 1; run <= MICRO_REPEATS; ++run)); do
  "$GEN_CLI" microbench --profile "$MICRO_PROFILE" --games "$MICRO_GAMES" | tee "$LOG_DIR/microbench-$run.log"
done

if [[ "$RUN_PARITY" == "1" ]]; then
  "$GEN_CLI" parity --profile "$PARITY_PROFILE" | tee "$LOG_DIR/parity.log"
fi

shopt -s nullglob
raw_profiles=("$RAW_DIR"/*.profraw)
shopt -u nullglob
if [[ "${#raw_profiles[@]}" -eq 0 ]]; then
  echo "no .profraw files were generated in $RAW_DIR" >&2
  exit 3
fi

"$PROFDATA" merge -output="$PROFDATA_FILE" "${raw_profiles[@]}"

cmake -S "$ROOT_DIR" -B "$BUILD_USE_DIR" \
  -DGAME2048_BUILD_GUI=OFF \
  -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
  -DGAME2048_NATIVE_OPT="$GAME2048_NATIVE_OPT" \
  -DGAME2048_ENABLE_LTO="$GAME2048_ENABLE_LTO" \
  -DCMAKE_CXX_FLAGS_RELEASE="$USE_FLAGS"
cmake --build "$BUILD_USE_DIR" --target game2048_cli game2048_cli_tests -j
ctest --test-dir "$BUILD_USE_DIR" --output-on-failure

USE_CLI="$BUILD_USE_DIR/game2048_cli"
if [[ "$RUN_USE_BENCH" == "1" ]]; then
  "$USE_CLI" microbench --profile "$MICRO_PROFILE" --games "$MICRO_GAMES" | tee "$LOG_DIR/use-microbench.log"
fi

if [[ "$RUN_USE_PARITY" == "1" ]]; then
  "$USE_CLI" parity --profile "$PARITY_PROFILE" | tee "$LOG_DIR/use-parity.log"
fi

echo "pgo_cli=$BUILD_USE_DIR/game2048_cli"
echo "profdata=$PROFDATA_FILE"
echo "logs=$LOG_DIR"
