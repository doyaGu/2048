#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TDL_DIR="${TDL_DIR:-"$(cd "$ROOT_DIR/.." && pwd)/TDL2048"}"
ARTIFACT_DIR="${ARTIFACT_DIR:-"$ROOT_DIR/artifacts/tdl2048-reference"}"
GAMES="${GAMES:-10000}"
EVAL_GAMES="${EVAL_GAMES:-1000}"
SEED="${SEED:-700000}"
THREADS="${THREADS:-1}"
NETWORK="${NETWORK:-8x6patt=5000}"
ALPHA="${ALPHA:-0.1}"

mkdir -p "$ARTIFACT_DIR"

if [[ ! -d "$TDL_DIR" ]]; then
  echo "TDL2048 directory not found: $TDL_DIR" >&2
  exit 2
fi

BIN="$ARTIFACT_DIR/tdl2048"
WEIGHTS="$ARTIFACT_DIR/8x6-reference.w"
LOG="$ARTIFACT_DIR/reference.log"
CSV="$ARTIFACT_DIR/summary.csv"

make -C "$TDL_DIR" native OUTPUT="$BIN" >/dev/null

{
  echo "timestamp,phase,games,eval_games,seed,network,alpha,threads"
  echo "$(date -u +%FT%TZ),stage0-reference,$GAMES,$EVAL_GAMES,$SEED,$NETWORK,$ALPHA,$THREADS"
} > "$CSV"

"$BIN" \
  -n "$NETWORK" \
  -a "$ALPHA" \
  -s "$SEED" \
  -p "$THREADS" \
  -t "$GAMES" \
  -e "$EVAL_GAMES" \
  -o "$WEIGHTS" "$LOG"

echo "weights=$WEIGHTS"
echo "log=$LOG"
echo "summary=$CSV"
