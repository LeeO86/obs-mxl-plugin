#!/bin/bash

# Linux-specific build script for OBS MXL Plugin
# This script helps set up the Linux build environment

set -e

echo "Setting up Linux build environment for OBS MXL Plugin..."

# Check for required packages
check_package() {
    if ! dpkg -l | grep -q "^ii  $1 "; then
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
    )
    
    for path in "${obs_header_paths[@]}"; do
        if [ -f "$path/obs.h" ] || [ -f "$path/libobs/obs.h" ]; then
            echo "Found OBS headers at: $path"
            return 0
        fi
    done
    return 1
}

# Check for OBS libraries
check_obs_libraries() {
    if ldconfig -p | grep -q "libobs\.so" || [ -f "/usr/lib/libobs.so" ] || [ -f "/usr/local/lib/libobs.so" ]; then
        echo "Found OBS libraries"
        return 0
    fi
    return 1
}

echo "Checking for required packages and dependencies..."
MISSING_PACKAGES=0

# Check for build tools
echo "Checking build tools..."
check_package "build-essential" || { echo "Install with: sudo apt install build-essential"; MISSING_PACKAGES=1; }
check_package "cmake" || { echo "Install with: sudo apt install cmake"; MISSING_PACKAGES=1; }
check_package "pkg-config" || { echo "Install with: sudo apt install pkg-config"; MISSING_PACKAGES=1; }

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
export CMAKE_PREFIX_PATH="$HOME/mxl-sdk/usr/local:$CMAKE_PREFIX_PATH"
export PKG_CONFIG_PATH="$HOME/mxl-sdk/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH"

# Run the main build script
echo "Running build script..."
./build.sh

echo ""
echo "Linux build complete!"
echo ""
echo "Plugin installed to: $HOME/.config/obs-studio/plugins/obs-mxl-plugin/"
echo ""
echo "If you encounter library loading issues, you may need to:"
echo "1. Add MXL SDK to your library path:"
echo "   export LD_LIBRARY_PATH=\"$HOME/mxl-sdk/usr/local/lib:\$LD_LIBRARY_PATH\""
echo "2. Or create a symlink in a system library directory"
