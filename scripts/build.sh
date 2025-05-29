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

install_build_essentials() {
    case $(detect_package_manager) in
        apt)
            sudo apt update
            sudo apt install -y build-essential cmake git curl ninja-build pkg-config libtool autoconf automake libudev-dev
            ;;
        pacman)
            sudo pacman -Sy --noconfirm base-devel cmake git curl ninja pkgconf libtool autoconf automake systemd
            ;;
        dnf)
            sudo dnf install -y gcc gcc-c++ make cmake git curl ninja-build pkgconf libtool autoconf automake systemd-devel
            ;;
        *)
            echo "Unsupported package manager" >&2
            exit 1
            ;;
    esac
}


install_gui_deps() {
    case $(detect_package_manager) in
        apt)
            sudo apt update
            sudo apt install -y libsdl2-dev libx11-dev libxext-dev libegl1-mesa-dev libgl1-mesa-dev libxkbcommon-dev libwayland-dev wayland-protocols libpolkit-gobject-1-dev
            ;;
        pacman)
            sudo pacman -Sy --noconfirm sdl2 libx11 libxext mesa libxkbcommon wayland wayland-protocols polkit glib2-devel
            ;;
        dnf)
            sudo dnf install -y SDL2-devel libX11-devel libXext-devel mesa-libEGL-devel mesa-libGL-devel libxkbcommon-devel wayland-devel wayland-protocols-devel polkit-devel polkit-libs glib2-devel
            ;;
    esac
}

install_build_essentials
install_gui_deps

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
"$VCPKG_DIR/vcpkg" install asio  openssl glad libevdev cereal --triplet x64-linux

# Create build directory and configure CMake
mkdir -p "$BUILD_DIR"
pushd "$BUILD_DIR"

cmake "$ROOT_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_DIR/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET="x64-linux" \
  -DVCPKG_LIBRARY_LINKAGE=static \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_BUILD_TYPE=Release

# Build the project
cmake --build . --config Release
popd

# Run the built executable
# EXE_PATH="$BUILD_DIR/LocalTether"
# if [[ -f "$EXE_PATH" ]]; then
#     echo "Running LocalTether..."
#     chmod +x "$EXE_PATH"
#     "$EXE_PATH"
# else
#     echo "Executable not found at $EXE_PATH"
#     exit 1
# fi

echo "--- Done ---"

