# OBS MXL Plugins

A collection of OBS Studio plugins that enable integration with [MXL (Media Exchange Layer)](https://github.com/dmf-mxl/mxl) flows for real-time, uncompressed media sharing between applications.

## ⚠️ Alpha Software Notice

**This is alpha software.** While the plugins are functionally working, they have not been extensively tested. Use with caution and expect potential issues or instability.

## ⚠️ Current Limitations

- **Platform Support**: Currently only supports macOS and Linux. Windows support is under development.
- **Resolution Requirement**: Output plugin currently only works with **1920x1080** resolution.
- **Video-Only Flow Support**: Plugins currently support video-only flows. Audio support is under development.

## Future Development and Roadmap
- Combine input & output plugins into one
- Add audio flow support
- Support for any resolution & framerate
- Add Windows support

## Plugins

### [📥 MXL Input Plugin](./obs-mxl-input-plugin/)
Allows OBS Studio to consume video streams from MXL flows as sources.

**Key Features:**
- Real-time video capture from MXL flows
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
Each plugin has its own build system. See individual plugin READMEs for detailed build instructions.

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
