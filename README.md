# OBS MXL Plugins

A collection of OBS Studio plugins that enable integration with MXL (Media Exchange Layer) flows for real-time, low-latency media sharing between applications.

## ⚠️ Current Limitations

- **Platform Support**: Currently only supports macOS and Linux. Windows support coming soon.
- **Resolution Requirement**: Output plugin currently only works with **1920x1080** resolution.
- **Media Support**: Input plugin supports video-only flows. Audio support is under development.

## Plugins

### [📥 MXL Input Plugin](./obs-mxl-input-plugin/)
Allows OBS Studio to consume video streams from MXL flows as sources.

**Key Features:**
- Real-time video capture from MXL flows
- Support for multiple video formats (RGBA, V210, I420, NV12)
- Configurable domain path and flow ID
- Automatic frame rate detection

### [📤 MXL Output Plugin](./obs-mxl-output-plugin/)
Enables OBS Studio to output video and audio streams in MXL format for consumption by other applications.

**Key Features:**
- Real-time streaming to MXL flows
- Dual stream support (separate video and audio flows)
- Native configuration dialog
- Automatic flow descriptor generation

## Quick Start

### Prerequisites

- OBS Studio (version 28.0 or later)
- MXL SDK installed and configured
- **macOS**: macOS 10.15+
- **Linux**: Ubuntu 20.04+ or equivalent

### Installation

1. **Build MXL SDK** (if not already installed):
   ```bash
   cd /path/to/mxl
   mkdir build && cd build
   cmake .. --preset Darwin-Clang-Release  # macOS
   # or cmake .. -DCMAKE_BUILD_TYPE=Release  # Linux
   cmake --build . --target all
   cmake --install . --prefix ~/mxl-sdk
   ```

2. **Build Both Plugins**:
   ```bash
   # Input Plugin
   cd obs-mxl-input-plugin
   ./build.sh
   
   # Output Plugin
   cd ../obs-mxl-output-plugin
   ./build.sh
   ```

3. **Restart OBS Studio** to load the plugins

### Basic Workflow

1. **Set up MXL Domain**:
   ```bash
   mkdir -p /tmp/mxl_domain
   ```

2. **Configure Output** (Tools → MXL Output Settings):
   - Set MXL Domain Path: `/tmp/mxl_domain`
   - Enable MXL Output and Video Stream
   - Note the generated Video Flow ID

3. **Add Input Source**:
   - Add "MXL Flow Source" in OBS
   - Use the same domain path and flow ID from output

4. **Test the Connection**:
   - Start streaming/recording in OBS
   - The input source should display the output content

## Use Cases

### Local Loopback
Stream from one OBS instance to another on the same machine for complex scene compositions or processing pipelines.

### Multi-Application Workflows
Share OBS output with other MXL-enabled applications for real-time processing, analysis, or distribution.

### Development and Testing
Create test video sources and sinks for MXL application development.

## Architecture

Both plugins use the MXL SDK to create and manage flows:

```
OBS Studio
├── Input Plugin → MXL Flow Reader → Shared Memory
└── Output Plugin → MXL Flow Writer → Shared Memory
                                          ↓
                              Other MXL Applications
```

## Configuration

### MXL Domain Setup
For best performance, use a memory-backed filesystem:

**Linux:**
```bash
sudo mount -t tmpfs -o size=1G tmpfs /tmp/mxl_domain
```

**macOS:**
```bash
# /tmp is already memory-backed by default
mkdir -p /tmp/mxl_domain
```

### Flow Management
- Flow IDs are UUIDs that uniquely identify streams
- Auto-generated if not specified
- Use consistent IDs between input and output for connections

## Troubleshooting

### Plugin Not Loading
- Check OBS Studio logs for error messages
- Ensure MXL SDK is properly installed
- Verify plugins are in correct directories

### No Video/Connection Issues
- Verify MXL domain path exists and is accessible
- Check flow IDs match between input and output
- Ensure flows are actively producing frames
- Check OBS logs for MXL-related errors

### Performance Issues
- Use tmpfs/memory filesystem for MXL domain
- Monitor CPU and memory usage
- Verify resolution settings (1920x1080 for output)

## Development

### Building from Source
Each plugin has its own build system. See individual plugin READMEs for detailed build instructions.

### Contributing
1. Fork the repository
2. Create feature branches for each plugin
3. Test thoroughly on supported platforms
4. Submit pull requests

## License

These plugins are licensed under the Apache 2.0 License, consistent with the MXL SDK.

## Support

For issues related to:
- **MXL SDK**: Check the main MXL repository
- **Plugin Integration**: Create issues in this repository
- **OBS Studio**: Check OBS Studio documentation and forums

## Related Projects

- [MXL SDK](https://github.com/aws/mxl) - Core Media Exchange Layer framework
- [OBS Studio](https://obsproject.com/) - Open source streaming and recording software
