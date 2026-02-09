#!/bin/bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MXL_DIR="$ROOT_DIR/third_party/mxl"

MXL_PRESET="${MXL_PRESET:-Linux-GCC-Release}"
MXL_SDK_PREFIX="${MXL_SDK_PREFIX:-$ROOT_DIR/.mxl-sdk/usr/local}"
VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Missing required command: $1"
        exit 1
    fi
}

require_cmd git
require_cmd cmake

echo "==> Using MXL preset: $MXL_PRESET"
echo "==> MXL SDK prefix: $MXL_SDK_PREFIX"

mkdir -p "$MXL_SDK_PREFIX"

echo "==> Syncing submodules..."
git submodule update --init --recursive

if [ ! -d "$MXL_DIR" ]; then
    echo "MXL submodule not found at: $MXL_DIR"
    exit 1
fi

VCPKG_TOOLCHAIN="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
USE_PRESET=true
if [ ! -f "$VCPKG_TOOLCHAIN" ]; then
    echo "Warning: vcpkg toolchain not found at $VCPKG_TOOLCHAIN"
    USE_PRESET=false
fi
if ! command -v ninja >/dev/null 2>&1; then
    echo "Warning: ninja not found in PATH"
    USE_PRESET=false
fi

echo "==> Building MXL..."
if [ "$USE_PRESET" = true ]; then
    pushd "$MXL_DIR" >/dev/null
    cmake --preset "$MXL_PRESET" \
        -DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN" \
        -DCMAKE_INSTALL_PREFIX="$MXL_SDK_PREFIX" \
        -DBUILD_TESTS=OFF \
        -DBUILD_DOCS=OFF
    MXL_BUILD_DIR="$MXL_DIR/build/$MXL_PRESET"
    cmake --build "$MXL_BUILD_DIR"
    cmake --install "$MXL_BUILD_DIR" --prefix "$MXL_SDK_PREFIX"
    popd >/dev/null
else
    echo "==> Falling back to non-preset build (no vcpkg and/or ninja)."
    GENERATOR="${CMAKE_GENERATOR:-}"
    if [ -z "$GENERATOR" ]; then
        if command -v ninja >/dev/null 2>&1; then
            GENERATOR="Ninja"
        else
            GENERATOR="Unix Makefiles"
        fi
    fi
    MXL_BUILD_DIR="$MXL_DIR/build/fallback-release"
    cmake -S "$MXL_DIR" -B "$MXL_BUILD_DIR" -G "$GENERATOR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$MXL_SDK_PREFIX" \
        -DBUILD_TESTS=OFF \
        -DBUILD_DOCS=OFF
    cmake --build "$MXL_BUILD_DIR"
    cmake --install "$MXL_BUILD_DIR" --prefix "$MXL_SDK_PREFIX"
fi

echo "==> Building input plugin..."
MXL_SDK_PREFIX="$MXL_SDK_PREFIX" "$ROOT_DIR/obs-mxl-input-plugin/build-linux.sh"

echo "==> Building output plugin..."
MXL_SDK_PREFIX="$MXL_SDK_PREFIX" "$ROOT_DIR/obs-mxl-output-plugin/build-linux.sh"

echo "==> Build complete."
