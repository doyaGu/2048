#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-"$ROOT_DIR/build-server"}"
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
GAME2048_NATIVE_OPT="${GAME2048_NATIVE_OPT:-OFF}"
USER_PROFILE_SET="${PROFILE+x}"
PROFILE="${PROFILE:-"$ROOT_DIR/profiles/ntuple_server.toml"}"
if [[ "${SMOKE:-0}" == "1" ]]; then
  if [[ -z "$USER_PROFILE_SET" ]]; then
    PROFILE="$ROOT_DIR/profiles/smoke.toml"
  fi
fi

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
  -DGAME2048_BUILD_GUI=OFF \
  -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
  -DGAME2048_NATIVE_OPT="$GAME2048_NATIVE_OPT"
cmake --build "$BUILD_DIR" --target game2048_cli

"$BUILD_DIR/game2048_cli" train --profile "$PROFILE"
