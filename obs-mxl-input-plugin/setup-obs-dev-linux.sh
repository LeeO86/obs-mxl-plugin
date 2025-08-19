#!/bin/bash

# Script to set up OBS Studio development environment on Linux

set -e

echo "Setting up OBS Studio development environment..."

# Check if we're on Ubuntu/Debian
if ! command -v apt &> /dev/null; then
    echo "This script is designed for Ubuntu/Debian systems with apt package manager"
    exit 1
fi

# Method 1: Try to install from PPA (most reliable)
echo "Attempting to install OBS Studio from official PPA..."
if ! grep -q "obsproject/obs-studio" /etc/apt/sources.list.d/*.list 2>/dev/null; then
    echo "Adding OBS Studio PPA..."
    sudo add-apt-repository -y ppa:obsproject/obs-studio
    sudo apt update
fi

# Install OBS Studio
sudo apt install -y obs-studio

# Method 2: If headers still not available, build minimal headers from source
if [ ! -f "/usr/include/obs/obs.h" ] && [ ! -f "/usr/local/include/obs/obs.h" ]; then
    echo "OBS headers not found in system paths. Setting up from source..."
    
    # Create temporary directory
    TEMP_DIR=$(mktemp -d)
    cd "$TEMP_DIR"
    
    echo "Downloading OBS Studio source for headers..."
    git clone --depth 1 --branch 30.2.3 https://github.com/obsproject/obs-studio.git
    
    cd obs-studio
    
    # Create a minimal build just to get headers
    mkdir build && cd build
    
    echo "Configuring OBS build (headers only)..."
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_UI=OFF \
        -DENABLE_SCRIPTING=OFF \
        -DBUILD_TESTS=OFF \
        -DCMAKE_INSTALL_PREFIX=/usr/local
    
    echo "Installing OBS headers..."
    sudo make install/fast
    
    # Clean up
    cd /
    rm -rf "$TEMP_DIR"
    
    echo "OBS headers installed to /usr/local/include/"
fi

# Verify installation
echo "Verifying OBS installation..."
if command -v obs &> /dev/null; then
    echo "✓ OBS Studio executable found"
else
    echo "✗ OBS Studio executable not found"
fi

# Check for headers
OBS_HEADER_FOUND=false
for path in "/usr/include/obs" "/usr/local/include/obs" "/usr/include/obs-studio" "/usr/local/include/obs-studio"; do
    if [ -f "$path/obs.h" ] || [ -f "$path/libobs/obs.h" ]; then
        echo "✓ OBS headers found at: $path"
        OBS_HEADER_FOUND=true
        break
    fi
done

if [ "$OBS_HEADER_FOUND" = false ]; then
    echo "✗ OBS headers not found"
    echo "You may need to build OBS from source or check your installation"
else
    echo "✓ OBS development environment ready!"
fi

# Check for libraries
if ldconfig -p | grep -q "libobs\.so"; then
    echo "✓ OBS libraries found"
else
    echo "✗ OBS libraries not found"
fi

echo ""
echo "Setup complete! You can now run ./build-linux.sh or ./build.sh"
