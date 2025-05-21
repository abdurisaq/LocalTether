#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(realpath "$SCRIPT_DIR/..")"
BUILD_DIR="$ROOT_DIR/build"
VCPKG_DIR="$ROOT_DIR/vcpkg"

# Optional: pass additional CMake options via CLI
CMAKE_ARGS=${@:-}

# Make sure build directory exists
mkdir -p "$BUILD_DIR"
pushd "$BUILD_DIR"

# Run CMake (configure)
cmake "$ROOT_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET="x64-linux" \
  -DCMAKE_BUILD_TYPE=Debug \
  $CMAKE_ARGS

# Build the project
cmake --build . --config Debug

popd

# Run the built executable
EXE_PATH="$BUILD_DIR/LocalTether"
if [[ -f "$EXE_PATH" ]]; then
    echo "Running LocalTether..."
    chmod +x "$EXE_PATH"
    "$EXE_PATH"
else
    echo "Executable not found at $EXE_PATH"
    exit 1
fi

