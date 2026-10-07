# mac-kinect

Native macOS **Azure Kinect DK depth capture and RGB background keying**.

This is an experimental standalone project aimed at a future OBS depth-key
plugin. It captures ToF measurements via libusb, reconstructs depth with a hybrid
CPU/Metal pipeline,
projects that depth into the RGB camera's view, and demonstrates a live cutout
in a local browser. No VM or Microsoft depth-engine binary is used.

## Quick start

Requirements: macOS, Xcode Command Line Tools, Git, Python 3, and an Azure Kinect
DK connected at USB 3 speed. Tested on an Apple Silicon Mac.

```sh
brew install cmake pkgconf libusb ffmpeg
cmake -S . -B build-native -DCMAKE_BUILD_TYPE=Release
cmake --build build-native --parallel 4
ctest --test-dir build-native --output-on-failure
python3 scripts/run_demo.py --build build-native
```

Open **http://127.0.0.1:8765**. Allow camera access for your terminal if macOS
asks. Stop the launcher with Ctrl-C to release the cameras and stop the server.

The first configure downloads pinned dependencies. Subsequent builds reuse them.
An existing SDK-era `build/` directory is not compatible with this build; use
`build-native/` as shown above. Old captures in `build/` remain usable.

## Demo controls

Metal neighborhood filtering is enabled by default, with automatic CPU fallback.
Use `python3 scripts/run_demo.py --build build-native --backend cpu` for the
reference path. On the tested M1, two recorded-frame benchmarks went from
69–71 ms CPU decode to 14–15 ms hybrid decode (4.8–5.0× faster), with identical
uint16 depth and invalid-pixel masks on both recordings. This measures decoder
latency, not end-to-end RGB/OBS latency. Metal capture requests 30 fps; CPU mode
requests 15 fps. The browser/RGB preview is still independently rate-limited.

To compare a complete NFOV raw recording against the CPU reference:

```sh
build-native/bin/benchmark_depth path/to/depth.raw
```

The shader is embedded and compiled by Metal at startup, so it does not require
a separately installed Metal command-line compiler or a runtime shader file.

- **Cutoff:** remove RGB pixels beyond the selected nominal distance.
- **Soft transition:** feather the cutoff over a distance interval.
- **Keep unknown depth:** retain pixels whose depth is unavailable; this can
  reduce holes but also retain background.
- **Show mask on RGB:** inspect the projected foreground mask.
- **Depth and registered mask:** expand diagnostic views.

This is a distance key, not person segmentation. A nearby wall can remain in
the cutout. Missing depth, occlusions, and moving edges still need improvement.

## Accuracy and synchronization

**The depth conversion uses OpenK4A's built-in tables from another Kinect.** Its
per-device ToF calibration parser is unfinished. Values are nominal millimeters,
not physically validated measurements for your device.

The **camera intrinsics and extrinsics**, in contrast, are read from the connected
Kinect and used for depth-to-RGB projection. The current point-based projection
is approximate. RGB and depth are captured concurrently but not exposure-synced.
These limitations matter for edge quality and must be addressed before treating
this as a production OBS filter.

## Project layout

```text
src/depth/       Native capture, CPU/Metal decoder, RGB projection, offline decoder
demo/            Local HTTP server and live RGB/depth-key browser UI
scripts/         Demo process launcher
tests/           PNG/mask output checks
cmake/           Pinned SDK dependency adapter and macOS portability fixes
docs/            Stream protocol, architecture, and remaining OBS work
```

Only project-owned implementation lives here. Microsoft SDK and OpenK4A source
are fetched under the chosen build directory, with no source submodules required.
SDK viewers, recorders, examples, .NET bindings and packaging are not built.

## Prior art

- [Azure Kinect Sensor SDK](https://github.com/microsoft/Azure-Kinect-Sensor-SDK)
  and [djpiper28's macOS port](https://github.com/microsoft/Azure-Kinect-Sensor-SDK/pull/1977):
  USB transport and camera calibration/projection.
- [OpenK4A](https://github.com/LukeSchoen/OpenK4A): CPU companding, phase
  reconstruction, de-aliasing, and filtering. We supply a native macOS adapter.

See [architecture and stream protocol](docs/native-depth.md) and
[third-party notices](THIRD_PARTY.md). This repository is not the official
Microsoft SDK, and an OBS plugin has not yet been implemented.
