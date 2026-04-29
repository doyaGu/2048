#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TDL2048_DIR="${TDL2048_DIR:-"/Users/touyou/workspace/TDL2048"}"
ARTIFACT_DIR="${ARTIFACT_DIR:-"$ROOT_DIR/artifacts/tdl2048-baseline"}"
CXX="${CXX:-"/opt/homebrew/bin/g++-15"}"
STD="${STD:-c++20}"
OUTPUT="${OUTPUT:-2048-tdl-baseline}"
NETWORK="${NETWORK:-8x6patt=320000/norm}"
TRAIN_K="${TRAIN_K:-20}"
SEED="${SEED:-700000}"
REPEATS="${REPEATS:-3}"

absolute_path() {
  case "$1" in
    /*) printf '%s\n' "$1" ;;
    *) printf '%s\n' "$ROOT_DIR/$1" ;;
  esac
}

TDL2048_DIR="$(absolute_path "$TDL2048_DIR")"
ARTIFACT_DIR="$(absolute_path "$ARTIFACT_DIR")"

if [[ ! -d "$TDL2048_DIR" ]]; then
  echo "TDL2048_DIR not found: $TDL2048_DIR" >&2
  exit 2
fi

if [[ ! -x "$CXX" ]]; then
  echo "CXX not executable: $CXX" >&2
  exit 3
fi

TIMESTAMP="$(date -u +%Y%m%dT%H%M%SZ)"
RUN_DIR="$ARTIFACT_DIR/tdl2048-$TIMESTAMP"
WORK_DIR="$RUN_DIR/source"
LOG_DIR="$RUN_DIR/logs"
SUMMARY="$RUN_DIR/summary.csv"
SUMMARY_STATS="$RUN_DIR/summary.txt"
mkdir -p "$WORK_DIR" "$LOG_DIR"

tar -C "$TDL2048_DIR" --exclude .git --exclude '*.w' --exclude '*.x' -cf - . | tar -C "$WORK_DIR" -xf -

# GCC on Apple arm64 sees size_t as unsigned long, which is ambiguous for TDL2048+'s
# uint32_t/uint64_t log2 overload set. Patch only the disposable benchmark copy.
perl -0pi -e 's/math::log2\(w\.size\(\) \?: 1\)/math::log2(static_cast<u64>(w.size() ?: 1))/g;
              s/math::log2\(weight::wghts\(\)\.front\(\)\.size\(\)\)/math::log2(static_cast<u64>(weight::wghts().front().size()))/g;
              s/math::log2\(w\.size\(\)\)/math::log2(static_cast<u64>(w.size()))/g' "$WORK_DIR/2048.cpp"

make -C "$WORK_DIR" CXX="$CXX" STD="$STD" OUTPUT="$OUTPUT"

echo "timestamp,kind,network,train_k,ops,log" > "$SUMMARY"
ops_values="$LOG_DIR/ops.values"
: > "$ops_values"

for ((run = 1; run <= REPEATS; ++run)); do
  log="$LOG_DIR/train-$run.log"
  "$WORK_DIR/$OUTPUT" -n "$NETWORK" -t "$TRAIN_K" -s "$SEED" -% none -p 1 | tee "$log"
  line="$(grep -E '^[0-9]+/[0-9]+ ' "$log" | tail -n 1)"
  ops="$(sed -n 's/.* \([0-9][0-9.]*\)ops.*/\1/p' <<<"$line")"
  echo "$ops" >> "$ops_values"
  echo "$TIMESTAMP,train,$NETWORK,$TRAIN_K,$ops,$log" >> "$SUMMARY"
done

sort -n "$ops_values" | awk '
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
    printf "tdl2048.train_ops count=%d min=%s median=%s max=%s\n", NR, values[1], median, values[NR]
  }
' > "$SUMMARY_STATS"

echo "summary=$SUMMARY"
echo "summary_stats=$SUMMARY_STATS"
echo "logs=$LOG_DIR"
