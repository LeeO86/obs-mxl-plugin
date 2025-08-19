# OBS MXL Plugin

An OBS Studio plugin that allows you to use MXL (Media Exchange Layer) flows as video sources.

## ⚠️ Current Limitations

- **Platform Support**: Currently only supports macOS and Linux. Windows support coming soon.
- **Media Support**: Currently supports video-only flows. Audio is not yet supported.
- **Resolution Testing**: Only tested with 1920x1080 video resolutions.

## Overview

This plugin enables OBS Studio to consume video streams from MXL flows, allowing for low-latency, high-performance video sharing between applications using shared memory.

![MXL Flow Source in OBS Studio](assets/screenshot_mxl_input.png)

## Features

- Real-time video capture from MXL flows
- Support for multiple video formats (RGBA, V210, I420, NV12)
- Configurable domain path and flow ID
- Automatic frame rate detection
- Thread-safe frame processing

## Prerequisites

- OBS Studio (version 28.0 or later)
- MXL SDK installed and configured
- **macOS**: macOS 10.15+ 
- **Linux**: Ubuntu 20.04+ or equivalent (with obs-studio-dev package)

## Building

### 1. Build and Install MXL SDK

**macOS:**
```bash
cd /path/to/mxl
mkdir build && cd build
cmake .. --preset Darwin-Clang-Release
cmake --build . --target all
cmake --install . --prefix ~/mxl-sdk
```

**Linux:**
```bash
cd /path/to/mxl
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target all
cmake --install . --prefix ~/mxl-sdk
```

### 2. Build the Plugin

**macOS:**
```bash
cd obs-mxl-plugin
./build.sh
```

**Linux:**
```bash
cd obs-mxl-plugin

# Option 1: Use the setup script (recommended)
./setup-obs-dev-linux.sh

# Option 2: Manual installation
# Install basic dependencies
sudo apt update
sudo apt install build-essential cmake pkg-config git

# Install OBS Studio from PPA
sudo add-apt-repository ppa:obsproject/obs-studio
sudo apt update
sudo apt install obs-studio

# Then build the plugin
./build.sh
# Or use the Linux helper script:
./build-linux.sh
```

## Installation

The build script automatically installs the plugin to:

**macOS:**
```
~/Library/Application Support/obs-studio/plugins/obs-mxl-plugin.plugin/
```

**Linux:**
```
~/.config/obs-studio/plugins/obs-mxl-plugin/
```

## Usage

1. **Set up MXL Domain**: Ensure your MXL domain directory exists and contains active flows.

2. **Add Source in OBS**:
   - Open OBS Studio
   - Click the "+" in Sources
   - Select "MXL Flow Source"
   - Give it a name and click OK

3. **Configure Source**:
   - **MXL Domain Path**: Path to your MXL domain directory (e.g., `/tmp/mxl_domain`)
   - **Flow ID**: UUID of the MXL flow you want to capture

4. **Test with MXL Tools**:
   ```bash
   # List available flows
   ./mxl-info -d /tmp/mxl_domain -l
   
   # Create a test video source
   ./mxl-gst-videotestsrc -d /tmp/mxl_domain -f flow_config.json
   ```

## Configuration

### MXL Domain Setup

The MXL domain should typically be on a tmpfs filesystem for best performance:

```bash
# Create tmpfs mount (Linux)
sudo mkdir -p /tmp/mxl_domain
sudo mount -t tmpfs -o size=1G tmpfs /tmp/mxl_domain

# macOS uses memory-backed filesystem by default in /tmp
mkdir -p /tmp/mxl_domain
```

### Flow Configuration

MXL flows are configured using NMOS Flow JSON format. Example:

```json
{
  "description": "Test Video Flow",
  "id": "550e8400-e29b-41d4-a716-446655440000",
  "format": "urn:x-nmos:format:video",
  "label": "Test Video",
  "media_type": "video/raw",
  "grain_rate": {
    "numerator": 30,
    "denominator": 1
  },
  "frame_width": 1920,
  "frame_height": 1080
}
```

## Supported Video Formats

- **video/raw**: RGBA format
- **video/x-raw**: RGBA format  
- **video/v210**: 10-bit YUV 4:2:2
- **video/yuv420p**: I420 format
- **video/nv12**: NV12 format

## Troubleshooting

### Plugin Not Loading
- Check OBS Studio logs for error messages
- Ensure MXL SDK is properly installed
- Verify plugin is in the correct directory

### No Video Appearing
- Verify MXL domain path is correct
- Check that the flow ID exists using `mxl-info`
- Ensure the flow is actively producing frames
- Check OBS Studio logs for MXL-related errors

### Performance Issues
- Ensure MXL domain is on tmpfs/memory filesystem
- Check system resources (CPU, memory)
- Verify flow grain rate matches expected frame rate

## Development

### Code Structure
- `src/obs-mxl-source.cpp`: Plugin registration and entry point
- `src/mxl-source.cpp`: Main source implementation
- `src/mxl-source.h`: Header definitions

### Key Components
- **mxl_source_data**: Main data structure holding MXL and OBS state
- **capture_loop()**: Background thread for reading MXL grains
- **process_grain()**: Frame processing and format conversion
- **OBS callbacks**: Integration with OBS Studio source API

### Extending the Plugin
- Add support for audio flows
- Implement additional video format conversions
- Add flow discovery/browsing UI
- Implement flow statistics and monitoring

## License

This plugin follows the same license as the MXL SDK (Apache 2.0).

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

## Support

For issues related to:
- **MXL SDK**: Check the main MXL repository
- **OBS Integration**: Create an issue in this repository
- **OBS Studio**: Check OBS Studio documentation and forums
