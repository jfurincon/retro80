#!/usr/bin/env bash
# Cross-compiles Retro80 as a Windows VST3 from Linux (needs mingw-w64, cmake, ninja).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
export PATH="$ROOT/scripts/shims:$PATH"
cmake -S "$ROOT" -B "$ROOT/build-win" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/mingw-w64.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DSMTG_RUN_VST_VALIDATOR=OFF -DSMTG_CREATE_PLUGIN_LINK=OFF -DSMTG_CREATE_MODULE_INFO=OFF
ninja -C "$ROOT/build-win" Retro80
echo "Built: $ROOT/build-win/VST3/Release/Retro80.vst3"
