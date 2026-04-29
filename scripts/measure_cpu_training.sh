#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"$ROOT_DIR/build-server-measure"}"
ARTIFACT_DIR="${ARTIFACT_DIR:-"$ROOT_DIR/artifacts/cpu-measurements"}"
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
GAME2048_NATIVE_OPT="${GAME2048_NATIVE_OPT:-ON}"
GAME2048_ENABLE_LTO="${GAME2048_ENABLE_LTO:-OFF}"
RUN_TESTS="${RUN_TESTS:-1}"
RUN_PARITY="${RUN_PARITY:-1}"
MICRO_PROFILE="${MICRO_PROFILE:-"$ROOT_DIR/profiles/smoke.toml"}"
MICRO_GAMES="${MICRO_GAMES:-200}"
MICRO_REPEATS="${MICRO_REPEATS:-3}"
PARITY_PROFILE="${PARITY_PROFILE:-"$ROOT_DIR/profiles/tdl_parity_smoke.toml"}"
PARITY_REPEATS="${PARITY_REPEATS:-1}"

absolute_path() {
  case "$1" in
    /*) printf '%s\n' "$1" ;;
    *) printf '%s\n' "$ROOT_DIR/$1" ;;
  esac
}

BUILD_DIR="$(absolute_path "$BUILD_DIR")"
ARTIFACT_DIR="$(absolute_path "$ARTIFACT_DIR")"
MICRO_PROFILE="$(absolute_path "$MICRO_PROFILE")"
PARITY_PROFILE="$(absolute_path "$PARITY_PROFILE")"

mkdir -p "$ARTIFACT_DIR"

TIMESTAMP="$(date -u +%Y%m%dT%H%M%SZ)"
SUMMARY="$ARTIFACT_DIR/cpu-training-$TIMESTAMP.csv"
SUMMARY_STATS="$ARTIFACT_DIR/cpu-training-$TIMESTAMP-summary.txt"
LOG_DIR="$ARTIFACT_DIR/cpu-training-$TIMESTAMP-logs"
mkdir -p "$LOG_DIR"

echo "timestamp,kind,profile,games,elapsed_ms,games_per_sec,moves_per_sec,train_games_per_sec,train_moves_per_sec,log" > "$SUMMARY"

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
  ' >> "$SUMMARY_STATS"
}

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
  -DGAME2048_BUILD_GUI=OFF \
  -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
  -DGAME2048_NATIVE_OPT="$GAME2048_NATIVE_OPT" \
  -DGAME2048_ENABLE_LTO="$GAME2048_ENABLE_LTO"
cmake --build "$BUILD_DIR" --target game2048_cli game2048_cli_tests -j

if [[ "$RUN_TESTS" == "1" ]]; then
  ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

CLI="$BUILD_DIR/game2048_cli"
MICRO_ELAPSED_VALUES="$LOG_DIR/microbench-elapsed-ms.values"
MICRO_GAMES_PER_SEC_VALUES="$LOG_DIR/microbench-games-per-sec.values"
MICRO_MOVES_PER_SEC_VALUES="$LOG_DIR/microbench-moves-per-sec.values"
PARITY_TRAIN_GAMES_PER_SEC_VALUES="$LOG_DIR/parity-train-games-per-sec.values"
PARITY_TRAIN_MOVES_PER_SEC_VALUES="$LOG_DIR/parity-train-moves-per-sec.values"
: > "$MICRO_ELAPSED_VALUES"
: > "$MICRO_GAMES_PER_SEC_VALUES"
: > "$MICRO_MOVES_PER_SEC_VALUES"
: > "$PARITY_TRAIN_GAMES_PER_SEC_VALUES"
: > "$PARITY_TRAIN_MOVES_PER_SEC_VALUES"

for ((run = 1; run <= MICRO_REPEATS; ++run)); do
  log="$LOG_DIR/microbench-$run.log"
  "$CLI" microbench --profile "$MICRO_PROFILE" --games "$MICRO_GAMES" | tee "$log"
  line="$(tail -n 1 "$log")"
  elapsed="$(sed -n 's/.*elapsed_ms=\([^ ]*\).*/\1/p' <<<"$line")"
  games_per_sec="$(sed -n 's/.*games_per_sec=\([^ ]*\).*/\1/p' <<<"$line")"
  moves_per_sec="$(sed -n 's/.*moves_per_sec=\([^ ]*\).*/\1/p' <<<"$line")"
  echo "$elapsed" >> "$MICRO_ELAPSED_VALUES"
  echo "$games_per_sec" >> "$MICRO_GAMES_PER_SEC_VALUES"
  echo "$moves_per_sec" >> "$MICRO_MOVES_PER_SEC_VALUES"
  echo "$TIMESTAMP,microbench,$MICRO_PROFILE,$MICRO_GAMES,$elapsed,$games_per_sec,$moves_per_sec,,,$log" >> "$SUMMARY"
done

if [[ "$RUN_PARITY" == "1" ]]; then
  for ((run = 1; run <= PARITY_REPEATS; ++run)); do
    log="$LOG_DIR/parity-$run.log"
    "$CLI" parity --profile "$PARITY_PROFILE" | tee "$log"
    line="$(grep 'train_games_per_sec=' "$log" | tail -n 1)"
    train_games_per_sec="$(sed -n 's/.*train_games_per_sec=\([^ ]*\).*/\1/p' <<<"$line")"
    train_moves_per_sec="$(sed -n 's/.*train_moves_per_sec=\([^ ]*\).*/\1/p' <<<"$line")"
    echo "$train_games_per_sec" >> "$PARITY_TRAIN_GAMES_PER_SEC_VALUES"
    echo "$train_moves_per_sec" >> "$PARITY_TRAIN_MOVES_PER_SEC_VALUES"
    echo "$TIMESTAMP,parity,$PARITY_PROFILE,,,,,$train_games_per_sec,$train_moves_per_sec,$log" >> "$SUMMARY"
  done
fi

write_metric_summary "microbench.elapsed_ms" "$MICRO_ELAPSED_VALUES"
write_metric_summary "microbench.games_per_sec" "$MICRO_GAMES_PER_SEC_VALUES"
write_metric_summary "microbench.moves_per_sec" "$MICRO_MOVES_PER_SEC_VALUES"
write_metric_summary "parity.train_games_per_sec" "$PARITY_TRAIN_GAMES_PER_SEC_VALUES"
write_metric_summary "parity.train_moves_per_sec" "$PARITY_TRAIN_MOVES_PER_SEC_VALUES"

echo "summary=$SUMMARY"
echo "summary_stats=$SUMMARY_STATS"
echo "logs=$LOG_DIR"
