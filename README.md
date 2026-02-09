# OBS MXL Plugins

A collection of OBS Studio plugins that enable integration with [MXL (Media Exchange Layer)](https://github.com/dmf-mxl/mxl) flows for real-time, uncompressed media sharing between applications.

## ⚠️ Alpha Software Notice

**This is alpha software.** While the plugins are functionally working, they have not been extensively tested. Use with caution and expect potential issues or instability.

## ⚠️ Current Limitations

- **Platform Support**: Currently only supports macOS and Linux. Windows support is under development.
- **Resolution Requirement**: Output plugin currently only works with **1920x1080** resolution.

## Future Development and Roadmap
- Combine input & output plugins into one
- Add audio flow support for output
- Support for any resolution & framerate
- Add Windows support

## Plugins

### [📥 MXL Input Plugin](./obs-mxl-input-plugin/)
Allows OBS Studio to consume video streams from MXL flows as sources.

**Key Features:**
- Real-time video/audio capture from MXL flows
- Support for multiple video formats (RGBA, V210, I420, NV12)
- Configurable domain path and flow ID
- Automatic frame rate detection

### [📤 MXL Output Plugin](./obs-mxl-output-plugin/)
Enables OBS Studio to output video streams as MXL flows for consumption by other applications.

**Key Features:**
- Real-time streaming to MXL flows
- Automatic flow descriptor generation

## Development

### Building from Source
Each plugin has its own build system. You can still build them individually (see the plugin READMEs), or use the unified build scripts at the repo root.

#### MXL Submodule
This repo pins the MXL SDK as a submodule at `third_party/mxl`, so the exact SDK commit is tracked with this repo.
Current pinned commit: `96a535ee31e96c0f66ae3b5565a366127df08a3c`.

Initialize or update submodules:
```bash
git submodule update --init --recursive
```

To update the pinned MXL commit:
```bash
cd third_party/mxl
git fetch origin
git checkout <new-commit>
cd ../..
git add third_party/mxl
```

#### Unified Build (macOS/Linux)
These scripts build the MXL SDK into a repo-local prefix and then build both plugins:

```bash
./build-macos.sh   # macOS
./build-linux.sh   # Linux
```

Defaults and environment variables:
- `MXL_SDK_PREFIX`: install prefix for the MXL SDK (default: `./.mxl-sdk/usr/local`)
- `VCPKG_ROOT`: path to vcpkg (default: `~/vcpkg`, required by MXL presets)
- `OBS_SOURCE_DIR`: OBS source path (macOS only)
- `MXL_PRESET`: override MXL CMake preset (default: `Darwin-Clang-Release` or `Linux-GCC-Release`)

You can also drive the scripts via CMake:
```bash
cmake -S . -B build
cmake --build build --target build-all
```

### Contributing
1. Fork the repository
2. Create feature branches for each plugin
3. Test thoroughly on supported platforms
4. Submit pull requests

## License

These plugins are licensed under the [Apache 2.0 License](LICENSE).

## Support

For issues related to:
- **MXL SDK**: Check the main MXL repository
- **Plugin Integration**: Create issues in this repository
- **OBS Studio**: Check OBS Studio documentation and forums

## Related Projects

- [MXL SDK](https://github.com/dmf-mxl/mxl) - Core Media Exchange Layer framework
- [OBS Studio](https://obsproject.com/) - Open source streaming and recording software
