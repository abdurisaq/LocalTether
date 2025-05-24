#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(realpath "$SCRIPT_DIR/..")"
BUILD_DIR="$ROOT_DIR/build"
VCPKG_DIR="$ROOT_DIR/vcpkg"

CMAKE_ARGS=${@:-}


mkdir -p "$BUILD_DIR"
pushd "$BUILD_DIR"


cmake "$ROOT_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET="x64-linux" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \ 
  $CMAKE_ARGS


cmake --build . --config Debug

popd


EXE_PATH="$BUILD_DIR/LocalTether"
if [[ -f "$EXE_PATH" ]]; then
    echo "Running LocalTether..."
    chmod +x "$EXE_PATH"
    "$EXE_PATH"
else
    echo "Executable not found at $EXE_PATH"
    exit 1
fi

