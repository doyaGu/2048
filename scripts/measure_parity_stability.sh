#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"$ROOT_DIR/build-server-lto-stability"}"
ARTIFACT_DIR="${ARTIFACT_DIR:-"$ROOT_DIR/artifacts/parity-stability"}"
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
GAME2048_NATIVE_OPT="${GAME2048_NATIVE_OPT:-ON}"
GAME2048_ENABLE_LTO="${GAME2048_ENABLE_LTO:-ON}"
BUILD="${BUILD:-1}"
RUN_TESTS="${RUN_TESTS:-1}"
PROFILE="${PROFILE:-"$ROOT_DIR/profiles/tdl_parity_smoke.toml"}"
REPEATS="${REPEATS:-5}"
WARMUPS="${WARMUPS:-1}"
CLI="${CLI:-}"

absolute_path() {
  case "$1" in
    /*) printf '%s\n' "$1" ;;
    *) printf '%s\n' "$ROOT_DIR/$1" ;;
  esac
}

BUILD_DIR="$(absolute_path "$BUILD_DIR")"
ARTIFACT_DIR="$(absolute_path "$ARTIFACT_DIR")"
PROFILE="$(absolute_path "$PROFILE")"
if [[ -n "$CLI" ]]; then
  CLI="$(absolute_path "$CLI")"
else
  CLI="$BUILD_DIR/game2048_cli"
fi

mkdir -p "$ARTIFACT_DIR"

TIMESTAMP="$(date -u +%Y%m%dT%H%M%SZ)"
CSV="$ARTIFACT_DIR/parity-stability-$TIMESTAMP.csv"
SUMMARY="$ARTIFACT_DIR/parity-stability-$TIMESTAMP-summary.txt"
LOG_DIR="$ARTIFACT_DIR/parity-stability-$TIMESTAMP-logs"
mkdir -p "$LOG_DIR"

if [[ "$BUILD" == "1" ]]; then
  cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
    -DGAME2048_BUILD_GUI=OFF \
    -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
    -DGAME2048_NATIVE_OPT="$GAME2048_NATIVE_OPT" \
    -DGAME2048_ENABLE_LTO="$GAME2048_ENABLE_LTO"
  cmake --build "$BUILD_DIR" --target game2048_cli game2048_cli_tests -j
  if [[ "$RUN_TESTS" == "1" ]]; then
    ctest --test-dir "$BUILD_DIR" --output-on-failure
  fi
fi

if [[ ! -x "$CLI" ]]; then
  echo "CLI is not executable: $CLI" >&2
  exit 2
fi

echo "timestamp,run,kind,train_games_per_sec,train_moves_per_sec,real_sec,user_sec,sys_sec,instructions_retired,peak_memory_bytes,log" > "$CSV"

TRAIN_GAMES_VALUES="$LOG_DIR/train-games-per-sec.values"
TRAIN_MOVES_VALUES="$LOG_DIR/train-moves-per-sec.values"
REAL_VALUES="$LOG_DIR/real-sec.values"
USER_VALUES="$LOG_DIR/user-sec.values"
SYS_VALUES="$LOG_DIR/sys-sec.values"
INSTRUCTIONS_VALUES="$LOG_DIR/instructions-retired.values"
PEAK_MEMORY_VALUES="$LOG_DIR/peak-memory-bytes.values"
: > "$TRAIN_GAMES_VALUES"
: > "$TRAIN_MOVES_VALUES"
: > "$REAL_VALUES"
: > "$USER_VALUES"
: > "$SYS_VALUES"
: > "$INSTRUCTIONS_VALUES"
: > "$PEAK_MEMORY_VALUES"

write_metric_summary() {
  local label="$1"
  local values_file="$2"
  if [[ ! -s "$values_file" ]]; then
    return
  fi
  sort -n "$values_file" | awk -v label="$label" '
    { values[NR] = $1 }
    END {
      if (NR == 0) {
        exit
      }
      mid = int((NR + 1) / 2)
      if (NR % 2 == 1) {
        median = values[mid]
      } else {
        median = (values[mid] + values[mid + 1]) / 2
      }
      printf "%s count=%d min=%s median=%s max=%s\n", label, NR, values[1], median, values[NR]
    }
  ' >> "$SUMMARY"
}

run_once() {
  local run="$1"
  local kind="$2"
  local log="$LOG_DIR/$kind-$run.log"
  /usr/bin/time -l "$CLI" parity --profile "$PROFILE" > "$log" 2>&1

  local train_line train_games train_moves real_sec user_sec sys_sec instructions peak_memory
  train_line="$(grep 'train_games_per_sec=' "$log" | tail -n 1)"
  train_games="$(sed -n 's/.*train_games_per_sec=\([^ ]*\).*/\1/p' <<<"$train_line")"
  train_moves="$(sed -n 's/.*train_moves_per_sec=\([^ ]*\).*/\1/p' <<<"$train_line")"
  real_sec="$(awk '/ real / && / user / && / sys/ { print $1; exit }' "$log")"
  user_sec="$(awk '/ real / && / user / && / sys/ { print $3; exit }' "$log")"
  sys_sec="$(awk '/ real / && / user / && / sys/ { print $5; exit }' "$log")"
  instructions="$(awk '/instructions retired/ { print $1; exit }' "$log")"
  peak_memory="$(awk '/peak memory footprint/ { print $1; found = 1; exit } END { if (!found) exit 1 }' "$log" 2>/dev/null || true)"
  if [[ -z "$peak_memory" ]]; then
    peak_memory="$(awk '/maximum resident set size/ { print $1; exit }' "$log")"
  fi

  if [[ "$kind" == "measure" ]]; then
    echo "$train_games" >> "$TRAIN_GAMES_VALUES"
    echo "$train_moves" >> "$TRAIN_MOVES_VALUES"
    echo "$real_sec" >> "$REAL_VALUES"
    echo "$user_sec" >> "$USER_VALUES"
    echo "$sys_sec" >> "$SYS_VALUES"
    if [[ -n "$instructions" ]]; then
      echo "$instructions" >> "$INSTRUCTIONS_VALUES"
    fi
    if [[ -n "$peak_memory" ]]; then
      echo "$peak_memory" >> "$PEAK_MEMORY_VALUES"
    fi
  fi

  echo "$TIMESTAMP,$run,$kind,$train_games,$train_moves,$real_sec,$user_sec,$sys_sec,$instructions,$peak_memory,$log" >> "$CSV"
  echo "$kind run=$run train_moves_per_sec=$train_moves real_sec=$real_sec instructions_retired=${instructions:-NA}"
}

for ((run = 1; run <= WARMUPS; ++run)); do
  run_once "$run" "warmup"
done

for ((run = 1; run <= REPEATS; ++run)); do
  run_once "$run" "measure"
done

write_metric_summary "parity.train_games_per_sec" "$TRAIN_GAMES_VALUES"
write_metric_summary "parity.train_moves_per_sec" "$TRAIN_MOVES_VALUES"
write_metric_summary "time.real_sec" "$REAL_VALUES"
write_metric_summary "time.user_sec" "$USER_VALUES"
write_metric_summary "time.sys_sec" "$SYS_VALUES"
write_metric_summary "time.instructions_retired" "$INSTRUCTIONS_VALUES"
write_metric_summary "time.peak_memory_bytes" "$PEAK_MEMORY_VALUES"

echo "csv=$CSV"
echo "summary=$SUMMARY"
echo "logs=$LOG_DIR"
