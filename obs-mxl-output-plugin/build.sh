#!/bin/bash

# MXL Output Plugin Build Script
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Building MXL Output Plugin...${NC}"

# Get the directory of this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Platform detection
if [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macOS"
    CMAKE_PRESET="Darwin-Clang-Release"
    INSTALL_DIR="$HOME/Library/Application Support/obs-studio/plugins"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="Linux"
    CMAKE_PRESET="Linux-GCC-Release"
    INSTALL_DIR="$HOME/.config/obs-studio/plugins"
else
    echo -e "${RED}Unsupported platform: $OSTYPE${NC}"
    exit 1
fi

echo -e "${YELLOW}Building for $PLATFORM${NC}"

# Create build directory
BUILD_DIR="build"
if [ -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}Cleaning existing build directory...${NC}"
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo -e "${YELLOW}Configuring with CMake...${NC}"
if [[ "$PLATFORM" == "macOS" ]]; then
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_OSX_ARCHITECTURES=arm64 \
        -DOBS_SOURCE_DIR="/Users/samisb/obs-studio" \
        -DMXL_SDK_PREFIX="/Users/samisb/mxl-sdk/usr/local"
else
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release
fi

# Build
echo -e "${YELLOW}Building...${NC}"
cmake --build . --config Release

# Install
echo -e "${YELLOW}Installing plugin...${NC}"
cmake --install .

# Platform-specific post-install steps
if [[ "$PLATFORM" == "macOS" ]]; then
    # Fix library paths for MXL SDK on macOS
    echo -e "${YELLOW}Fixing MXL library paths...${NC}"
    PLUGIN_PATH="$INSTALL_DIR/obs-mxl-output-plugin.plugin/Contents/MacOS/obs-mxl-output-plugin.so"
    MXL_SDK_PATH="$HOME/mxl-sdk/usr/local/lib/libmxl.0.dylib"

    if [ -f "$PLUGIN_PATH" ]; then
        # Fix MXL library path
        install_name_tool -change @rpath/libmxl.0.dylib "$MXL_SDK_PATH" "$PLUGIN_PATH"
        echo -e "${GREEN}Fixed MXL library path to: $MXL_SDK_PATH${NC}"
        echo -e "${GREEN}Qt-free plugin - no Qt dependencies to fix${NC}"
    else
        echo -e "${RED}Warning: Plugin not found at $PLUGIN_PATH${NC}"
    fi
    
    INSTALL_PATH="$INSTALL_DIR/obs-mxl-output-plugin.plugin"
elif [[ "$PLATFORM" == "Linux" ]]; then
    # Set up rpath for MXL SDK on Linux
    echo -e "${YELLOW}Setting up library paths for Linux...${NC}"
    PLUGIN_PATH="$INSTALL_DIR/obs-mxl-output-plugin/bin/64bit/obs-mxl-output-plugin.so"
    
    if [ -f "$PLUGIN_PATH" ]; then
        # Add MXL SDK lib directory to rpath if needed
        if [ -d "$HOME/mxl-sdk/usr/local/lib" ]; then
            patchelf --set-rpath "\$ORIGIN:$HOME/mxl-sdk/usr/local/lib" "$PLUGIN_PATH" 2>/dev/null || echo "Note: patchelf not available, manual LD_LIBRARY_PATH setup may be needed"
        fi
        echo -e "${GREEN}Plugin installed successfully${NC}"
    else
        echo -e "${RED}Warning: Plugin not found at $PLUGIN_PATH${NC}"
    fi
    
    INSTALL_PATH="$INSTALL_DIR/obs-mxl-output-plugin"
fi

echo -e "${GREEN}Build completed successfully!${NC}"
echo -e "${GREEN}Plugin installed to: $INSTALL_PATH${NC}"
echo -e "${YELLOW}Note: Restart OBS Studio to load the new plugin.${NC}"
