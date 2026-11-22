#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="${1:-$ROOT/build-macos}"
ARCH="$(uname -m)"
CMAKE_ARGS=(
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_OSX_ARCHITECTURES="$ARCH"
  -DBUILD_TESTING=ON
)
if [[ "${DETTATO_ENABLE_AAX:-0}" == "1" ]]; then
  CMAKE_ARGS+=(-DDETTATO_ENABLE_AAX=ON -DAAX_SDK_PATH="${AAX_SDK_PATH:?Set AAX_SDK_PATH}")
fi

cmake -S "$ROOT/plugin" -B "$BUILD_DIR" -G Ninja "${CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR" --parallel 2
ctest --test-dir "$BUILD_DIR" -C Release --output-on-failure
bash "$ROOT/plugin/scripts/build_engine_mac.sh" "$BUILD_DIR"
bash "$ROOT/plugin/scripts/package_mac.sh" "$BUILD_DIR" "$ARCH"
