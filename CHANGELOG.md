# Changelog

## Input plugin 0.0.2-alpha (CMake project 1.0.1)

### Fixes

- **An interlaced video flow is refused instead of crashing OBS.** MXL keeps an interlaced flow (`interlace_mode` `interlaced_tff` or `interlaced_bff`) as one field per grain: `frame_height / 2` lines at twice the declared rate. The input read each 540-line grain of a 1080i flow as 1080 lines, about 2.7 MB past the end of the grain's memory mapping (most likely SIGSEGV/SIGBUS). The input now reads `interlace_mode` from `flow_def.json` and accepts only `progressive` (or a definition without it). An interlaced flow is not read: the source shows nothing, the OBS log has `MXL Source: Refusing flow <id>: interlace_mode is <mode>; only progressive video flows are supported`, the source's properties show the reason in red, and the flow list marks such flows `[<mode>: not supported]`.
- The v210 to RGBA conversion converts only the whole lines the grain holds (`grainSize / line stride`), so a grain smaller than the declared frame is never read past its end (one warning per source in the log).
- **The v210 line length is MXL's**: `(width + 47) / 48 * 128` bytes (each line padded to 48 pixels). The conversion used `(width + 5) / 6 * 16` bytes, which is shorter for widths that are not a multiple of 48 (1280: 3424 instead of 3456 bytes), so every line started a little early and the picture was sheared. 1920 and 3840 were not affected.
- The reason shown in the properties is guarded by a lock: libobs runs the update of a video source (where the flow is opened) on the video thread, while the properties dialog reads it on the UI thread.
