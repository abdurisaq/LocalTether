#!/usr/bin/env bash

set -euo pipefail

unset DLLTOOL RC WINDRES

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(realpath "$SCRIPT_DIR/..")"
BUILD_DIR="$ROOT_DIR/build"
VCPKG_DIR="$ROOT_DIR/vcpkg"
EXTERNAL_DIR="$ROOT_DIR/external"
IMGUI_DIR="$EXTERNAL_DIR/imgui"
IMGUI_BACKENDS_DIR="$EXTERNAL_DIR/imgui_backends"

# Detect the package manager
detect_package_manager() {
    if command -v apt-get &>/dev/null; then
        echo "apt"
    elif command -v pacman &>/dev/null; then
        echo "pacman"
    elif command -v dnf &>/dev/null; then
        echo "dnf"
    else
        echo "unknown"
    fi
}

# Install packages based on detected package manager
install_package() {
    local pkg=$1
    local manager
    manager=$(detect_package_manager)
    case $manager in
        apt) sudo apt update && sudo apt install -y "$pkg" ;;
        pacman) sudo pacman -Sy --noconfirm "$pkg" ;;
        dnf) sudo dnf install -y "$pkg" ;;
        *) echo "Unsupported package manager. Install '$pkg' manually." >&2; exit 1 ;;
    esac
}

# Install essential packages
install_packages() {
    echo "Installing required packages..."
    install_package cmake
    install_package git
    install_package curl
    install_package gcc
    install_package gcc-c++
    install_package make
    install_package perl
    install_package ninja-build
    install_package pkg-config
    install_package libtool libtool-ltdl-devel
    install_package autoconf
    install_package automake
    install_package polkit-devel
}

install_packages

# Clone vcpkg if it doesn't exist
if [[ ! -d "$VCPKG_DIR" ]]; then
    echo "Cloning vcpkg..."
    git clone https://github.com/microsoft/vcpkg.git "$VCPKG_DIR"
    pushd "$VCPKG_DIR"
    ./bootstrap-vcpkg.sh
    popd
fi

export VCPKG_ROOT="$VCPKG_DIR"
export PATH="$VCPKG_ROOT:$PATH"

# Help vcpkg detect the compiler
export CC=/usr/bin/gcc
export CXX=/usr/bin/g++

# Clone or update ImGui docking branch
if [[ ! -d "$IMGUI_DIR" ]]; then
    echo "Cloning Dear ImGui (docking)..."
    git clone --branch docking https://github.com/ocornut/imgui.git "$IMGUI_DIR"
else
    echo "Updating Dear ImGui..."
    pushd "$IMGUI_DIR"
    git fetch
    git checkout docking
    git pull
    popd
fi
# Install libraries with static linking flags
"$VCPKG_DIR/vcpkg" install boost-asio sdl2[core] openssl glad --triplet x64-linux

# Create build directory and configure CMake
mkdir -p "$BUILD_DIR"
pushd "$BUILD_DIR"

cmake "$ROOT_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET="x64-linux" \
  -DVCPKG_LIBRARY_LINKAGE=static \
  -DCMAKE_BUILD_TYPE=Release

# Build the project
cmake --build . --config Release
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

echo "--- Done ---"

