# OBS MXL Output Plugin

This plugin allows OBS Studio to output video and audio streams in MXL (Media Exchange Layer) format, enabling real-time media exchange between applications using the MXL framework.

## ⚠️ Important Notes

- **Resolution Requirement**: Currently only works with **1920x1080** resolution. In OBS, go to Settings → Video → Output (Scaled) Resolution and set to 1920x1080.
- **Platform Support**: Currently working on **macOS** and **Linux** only. Windows support is under development.

## Screenshot

![MXL Output Settings Dialog](assets/screenshot_mxl_output.png)

## Features

- **Real-time Output**: Stream OBS video and audio output directly to MXL flows
- **Dual Stream Support**: Separate video and audio flows with independent configuration
- **Native Configuration Dialog**: Clean, platform-native settings interface
- **Format Support**: Supports various video and audio formats from OBS
- **Flow Management**: Automatic creation of MXL flow descriptors and writers
- **Cross-platform**: Works on macOS and Linux
- **Independent Configuration**: File-based config system independent of OBS settings

## Requirements

- OBS Studio (version 28.0 or later)
- MXL SDK installed and configured
- CMake 3.20 or later
- C++17 compatible compiler

## Building

### Prerequisites

1. **Install MXL SDK**: Build and install the MXL SDK to a known location
2. **OBS Studio**: Either install OBS Studio or build from source

### macOS

```bash
# Set paths (adjust as needed)
export OBS_SOURCE_DIR="/path/to/obs-studio"
export MXL_SDK_PREFIX="/path/to/mxl-sdk/usr/local"

# Build
./build.sh
```

### Linux

```bash
# Install OBS development packages
sudo apt install obs-studio-dev  # Ubuntu/Debian
# or build OBS from source

# Build
./build_linux.sh
```

### Manual Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
cmake --install .
```

## Installation

The build script automatically installs the plugin to the appropriate OBS plugin directory:

- **macOS**: `~/Library/Application Support/obs-studio/plugins/obs-mxl-output-plugin.plugin`
- **Linux**: `~/.config/obs-studio/plugins/obs-mxl-output-plugin`

After installation, restart OBS Studio to load the plugin.

## Usage

### Configuration

1. **Open Settings**: In OBS, go to Tools → MXL Output Settings
2. **Configure Settings** in the native dialog:
   - **Current Status**: Shows real-time output status at the top
   - **MXL Domain Path**: Path to your MXL domain directory
   - **Enable MXL Output**: Master toggle for the output
   - **Enable Video Stream**: Toggle video stream output
   - **Enable Audio Stream**: Toggle audio stream output
   - **Video Flow ID**: UUID for the video flow (auto-generated if empty)
   - **Audio Flow ID**: UUID for the audio flow (auto-generated if empty)

3. **Apply Changes**: Click OK to save and immediately apply settings

### Key Features

- **Real-time Status**: Current output status displayed in the settings dialog
- **Immediate Application**: Settings changes take effect immediately without OBS restart
- **Auto-restart**: Output automatically restarts when stream configuration changes
- **Persistent Settings**: Configuration saved to dedicated config file

## Configuration

### MXL Domain Path

The domain path is where MXL flows are stored. This should be a directory accessible to both the output plugin and any consuming applications.

Example: `/home/user/mxl_domain` or `/Users/user/mxl_domain`

### Flow IDs

Flow IDs are UUIDs that uniquely identify each MXL flow. If left empty, the plugin will auto-generate them. You can specify custom UUIDs if you need predictable flow identifiers.

Example: `5fbec3b1-1b0f-417d-9059-8b94a47197ed`

## Flow Descriptors

The plugin automatically creates NMOS-style flow descriptor JSON files for each flow:

### Video Flow Descriptor
```json
{
  "description": "MXL Video Output Flow",
  "id": "video-flow-uuid",
  "format": "urn:x-nmos:format:video",
  "media_type": "video/x-raw",
  "grain_rate": {
    "numerator": 30,
    "denominator": 1
  },
  "frame_width": 1920,
  "frame_height": 1080,
  "colorspace": "BT709"
}
```

### Audio Flow Descriptor
```json
{
  "description": "MXL Audio Output Flow",
  "id": "audio-flow-uuid",
  "format": "urn:x-nmos:format:audio",
  "media_type": "audio/x-raw",
  "sample_rate": {
    "numerator": 48000,
    "denominator": 1
  },
  "channels": 2
}
```

## Consuming MXL Flows

Once the output plugin is running, other applications can consume the MXL flows using the MXL SDK:

```cpp
// Example consumer code
mxlInstance instance = mxlCreateInstance("/path/to/mxl_domain", "");
mxlFlowReader reader;
mxlCreateFlowReader(instance, "video-flow-uuid", "", &reader);

// Read grains...
GrainInfo grainInfo;
uint8_t* payload;
mxlFlowReaderReadGrain(reader, &grainInfo, &payload);
```

## Configuration File

Settings are stored in a dedicated configuration file:
- **macOS**: `~/Library/Application Support/obs-studio/plugin_config/mxl-output-config.ini`
- **Linux**: `~/.config/obs-studio/plugin_config/mxl-output-config.ini`

This file persists settings independently of OBS configuration and can be manually edited if needed.

## Troubleshooting

### Plugin Not Loading

1. Check OBS log for error messages
2. Verify MXL SDK is properly installed
3. Ensure plugin is in the correct directory
4. Check file permissions

### Settings Not Persisting

1. Check that the plugin config directory is writable
2. Verify the config file path in the logs
3. Ensure sufficient disk space

### Flow Creation Errors

1. Verify MXL domain path exists and is writable
2. Check disk space
3. Ensure no conflicting flow IDs

### Performance Issues

1. Monitor CPU usage - MXL operations are CPU intensive
2. Consider reducing video resolution/framerate
3. Check available memory

## Logging

The plugin uses OBS's logging system with clean, focused output:

```
=== LOADING MXL OUTPUT PLUGIN v1.0.0 ===
MXL Output: Output type registered successfully
MXL Output: Settings updated - Output: enabled, Video: enabled, Audio: disabled
MXL Output: Starting output
MXL Output: Output started successfully - Video: enabled, Audio: disabled
```

Enable OBS debug logging to see detailed MXL operations.

## Integration with MXL Input Plugin

This output plugin works seamlessly with the MXL input plugin, enabling:

- **Local Loopback**: Output from one OBS instance, input to another
- **Network Distribution**: Multiple consumers of the same MXL flows
- **Processing Pipelines**: Chain multiple MXL-enabled applications

## Architecture

### Clean, Modern Design
- **Native Configuration**: Platform-native dialog interface
- **File-based Config**: Independent configuration system
- **No Deprecated APIs**: Uses modern OBS plugin patterns
- **Minimal Dependencies**: Clean, focused codebase

### Key Components
- **Core Output Engine**: Handles OBS integration and MXL flow creation
- **Native Dialog System**: Cross-platform configuration interface
- **Configuration Manager**: File-based settings persistence
- **Flow Management**: Automatic MXL flow descriptor generation

## License

This plugin is licensed under the Apache 2.0 License, consistent with the MXL project.

## Contributing

Contributions are welcome! Please follow the MXL project's contribution guidelines.

## Support

For issues and questions:
1. Check the MXL project documentation
2. Review OBS plugin development guides
3. File issues in the MXL project repository
