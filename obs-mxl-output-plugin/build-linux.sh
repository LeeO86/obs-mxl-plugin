#!/bin/bash

# Linux-specific build script for OBS MXL Output Plugin
# This script helps set up the Linux build environment

set -e

echo "Setting up Linux build environment for OBS MXL Output Plugin..."

# Check for required packages
check_package() {
    if ! dpkg -l | grep -q "^ii  $1"; then
        echo "Missing package: $1"
        return 1
    fi
    return 0
}

# Check for OBS headers in common locations
check_obs_headers() {
    local obs_header_paths=(
        "/usr/include/obs"
        "/usr/local/include/obs"
        "/usr/include/obs-studio"
        "/usr/local/include/obs-studio"
        "/usr/include/obs-studio/libobs"
        "/usr/local/include/obs-studio/libobs"
    )
    
    for path in "${obs_header_paths[@]}"; do
        if [ -f "$path/obs.h" ] || [ -f "$path/libobs/obs.h" ]; then
            echo "Found OBS headers at: $path"
            export OBS_INCLUDE_PATH="$path"
            return 0
        fi
    done
    return 1
}

# Check for OBS libraries
check_obs_libraries() {
    if ldconfig -p | grep -q "libobs\.so" || [ -f "/usr/lib/libobs.so" ] || [ -f "/usr/local/lib/libobs.so" ] || [ -f "/usr/lib/x86_64-linux-gnu/libobs.so" ]; then
        echo "Found OBS libraries"
        return 0
    fi
    return 1
}

# Check for MXL SDK
check_mxl_sdk() {
    if [ -n "$MXL_SDK_PREFIX" ]; then
        if [ -f "$MXL_SDK_PREFIX/lib/cmake/mxl/mxlConfig.cmake" ] || \
           [ -f "$MXL_SDK_PREFIX/include/mxl/mxl.h" ] || \
           [ -f "$MXL_SDK_PREFIX/include/mxl.h" ]; then
            echo "Found MXL SDK at: $MXL_SDK_PREFIX"
            return 0
        fi
    fi

    local mxl_paths=(
	"$HOME/mxl-sdk"
        "/usr"
        "$HOME/mxl-sdk/usr/local"
        "/usr/local"
        "/opt/mxl-sdk"
        "$HOME/mxl/usr/local"
    )
    
    for path in "${mxl_paths[@]}"; do
        if [ -f "$path/lib/cmake/mxl/mxlConfig.cmake" ] || [ -f "$path/include/mxl/mxl.h" ] || [ -f "$path/include/mxl.h" ]; then
            echo "Found MXL SDK at: $path"
            export MXL_SDK_PREFIX="$path"
            return 0
        fi
    done
    return 1
}

echo "Checking for required packages and dependencies..."
MISSING_PACKAGES=0

# Check for build tools
echo "Checking build tools..."
check_package "build-essential" || { echo "Install with: sudo apt install build-essential"; MISSING_PACKAGES=1; }
check_package "cmake" || { echo "Install with: sudo apt install cmake"; MISSING_PACKAGES=1; }
check_package "pkg-config" || { echo "Install with: sudo apt install pkg-config"; MISSING_PACKAGES=1; }

# Check for GTK development headers (required for native dialog)
echo "Checking for GTK development headers..."
check_package "libgtk-3-dev" || { echo "Install with: sudo apt install libgtk-3-dev"; MISSING_PACKAGES=1; }

# Check for OBS Studio installation and headers
echo "Checking for OBS Studio..."
if ! command -v obs &> /dev/null; then
    echo "OBS Studio not found. Install with:"
    echo "  sudo add-apt-repository ppa:obsproject/obs-studio"
    echo "  sudo apt update"
    echo "  sudo apt install obs-studio"
    MISSING_PACKAGES=1
fi

echo "Checking for OBS development headers..."
if ! check_obs_headers; then
    echo "OBS development headers not found."
    echo "You may need to:"
    echo "1. Build OBS from source, or"
    echo "2. Install from a PPA that includes dev headers:"
    echo "   sudo add-apt-repository ppa:obsproject/obs-studio"
    echo "   sudo apt update"
    echo "   sudo apt install obs-studio"
    echo "3. Or manually install headers from OBS source"
    MISSING_PACKAGES=1
fi

echo "Checking for OBS libraries..."
if ! check_obs_libraries; then
    echo "OBS libraries not found."
    echo "Install OBS Studio first: sudo apt install obs-studio"
    MISSING_PACKAGES=1
fi

echo "Checking for MXL SDK..."
if ! check_mxl_sdk; then
    echo "MXL SDK not found in common locations:"
    echo "  - /usr"
    echo "  - $HOME/mxl-sdk/usr/local"
    echo "  - /usr/local"
    echo "  - /opt/mxl-sdk"
    echo "  - $HOME/mxl/usr/local"
    echo ""
    echo "Please ensure MXL SDK is built and installed, or set MXL_SDK_PREFIX manually:"
    echo "  export MXL_SDK_PREFIX=/path/to/mxl-sdk"
    MISSING_PACKAGES=1
fi

# Check for patchelf (optional but recommended)
if ! command -v patchelf &> /dev/null; then
    echo "patchelf not found (optional but recommended)"
    echo "Install with: sudo apt install patchelf"
fi

if [ $MISSING_PACKAGES -eq 1 ]; then
    echo ""
    echo "Please install missing packages and run this script again."
    exit 1
fi

echo "All required packages found!"

# Set environment variables for Linux build
if [ -n "$MXL_SDK_PREFIX" ]; then
    export CMAKE_PREFIX_PATH="$MXL_SDK_PREFIX:$CMAKE_PREFIX_PATH"
    export PKG_CONFIG_PATH="$MXL_SDK_PREFIX/lib/pkgconfig:$PKG_CONFIG_PATH"
    export LD_LIBRARY_PATH="$MXL_SDK_PREFIX/lib:$LD_LIBRARY_PATH"
fi

# Clean and create build directory
echo "Preparing build directory..."
rm -rf build
mkdir -p build
cd build

# Configure with CMake, passing MXL SDK path if found
echo "Configuring with CMake..."
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release"
if [ -n "$MXL_SDK_PREFIX" ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DMXL_SDK_PREFIX=$MXL_SDK_PREFIX"
fi
if [ -n "$OBS_INCLUDE_PATH" ]; then
    CMAKE_ARGS="$CMAKE_ARGS -DOBS_SOURCE_DIR=$OBS_INCLUDE_PATH"
fi

cmake .. $CMAKE_ARGS

# Build
echo "Building..."
make -j$(nproc)

# Install
echo "Installing..."
make install

cd ..

echo ""
echo "Linux build complete!"
echo ""
echo "Plugin installed to: $HOME/.config/obs-studio/plugins/obs-mxl-output-plugin/"
echo ""
echo "If you encounter library loading issues, you may need to:"
echo "1. Add MXL SDK to your library path:"
if [ -n "$MXL_SDK_PREFIX" ]; then
    echo "   export LD_LIBRARY_PATH=\"$MXL_SDK_PREFIX/lib:\$LD_LIBRARY_PATH\""
else
    echo "   export LD_LIBRARY_PATH=\"/path/to/mxl-sdk/lib:\$LD_LIBRARY_PATH\""
fi
echo "2. Or create a symlink in a system library directory"
echo ""
echo "Restart OBS Studio to load the plugin."
