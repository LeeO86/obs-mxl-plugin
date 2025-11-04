#!/bin/bash

# Build script for OBS MXL Plugin

set -e

# Detect platform
if [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macOS"
    INSTALL_DIR="$HOME/Library/Application Support/obs-studio/plugins"
    NCPU_CMD="sysctl -n hw.ncpu"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="Linux"
    INSTALL_DIR="$HOME/.config/obs-studio/plugins"
    NCPU_CMD="nproc"
else
    echo "Unsupported platform: $OSTYPE"
    exit 1
fi

# Configuration
BUILD_TYPE=${BUILD_TYPE:-Release}
BUILD_DIR="build"

echo "Building OBS MXL Plugin for $PLATFORM..."
echo "Build type: $BUILD_TYPE"
echo "Install directory: $INSTALL_DIR"

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Platform-specific CMake configuration
if [[ "$PLATFORM" == "macOS" ]]; then
    cmake .. \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_PREFIX_PATH="../.." \
        -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
else
    cmake .. \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_PREFIX_PATH="../.."
fi

# Build
cmake --build . --config "$BUILD_TYPE" -j$($NCPU_CMD)

# Install
echo "Installing plugin to: $INSTALL_DIR"
cmake --install . --config "$BUILD_TYPE"

# Platform-specific post-install steps
if [[ "$PLATFORM" == "macOS" ]]; then
    # Fix library paths for MXL SDK on macOS
    echo "Fixing MXL library paths..."
    PLUGIN_PATH="$INSTALL_DIR/obs-mxl-plugin.plugin/Contents/MacOS/obs-mxl-plugin.so"
    MXL_SDK_PATH="$HOME/mxl-sdk/usr/local/lib/libmxl.0.dylib"

    if [ -f "$PLUGIN_PATH" ]; then
        install_name_tool -change @rpath/libmxl.0.dylib "$MXL_SDK_PATH" "$PLUGIN_PATH"
        echo "Fixed MXL library path to: $MXL_SDK_PATH"
    else
        echo "Warning: Plugin not found at $PLUGIN_PATH"
    fi
elif [[ "$PLATFORM" == "Linux" ]]; then
    # Set up rpath for MXL SDK on Linux
    echo "Setting up library paths for Linux..."
    PLUGIN_PATH="$INSTALL_DIR/obs-mxl-plugin/bin/64bit/obs-mxl-plugin.so"
    
    if [ -f "$PLUGIN_PATH" ]; then
        # Add MXL SDK lib directory to rpath if needed
        if [ -d "$HOME/mxl-sdk/usr/local/lib" ]; then
            patchelf --set-rpath "\$ORIGIN:$HOME/mxl-sdk/usr/local/lib" "$PLUGIN_PATH" 2>/dev/null || echo "Note: patchelf not available, manual LD_LIBRARY_PATH setup may be needed"
        fi
        echo "Plugin installed successfully"
    else
        echo "Warning: Plugin not found at $PLUGIN_PATH"
    fi
fi

echo "Build complete!"
echo ""
echo "To use the plugin:"
echo "1. Make sure MXL domain is set up (typically on tmpfs)"
if [[ "$PLATFORM" == "Linux" ]]; then
    echo "   sudo mkdir -p /tmp/mxl_domain"
    echo "   sudo mount -t tmpfs -o size=1G tmpfs /tmp/mxl_domain"
else
    echo "   mkdir -p /tmp/mxl_domain"
    echo "   sudo mount_tmpfs -s 1g /tmp/mxl_domain"
fi
echo "2. Start OBS Studio"
echo "3. Add a new source -> MXL Flow Source"
echo "4. Configure the domain path and flow ID"
