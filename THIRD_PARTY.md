# Third-party code and attribution

The capture and decode pipeline builds on substantial prior work; it is not a
from-scratch depth reconstruction algorithm.

## Microsoft Azure Kinect Sensor SDK

Fetched from `https://github.com/djpiper28/Azure-Kinect-Sensor-SDK.git`, commit
`59047d3ca78fddde8b3844a50aa3fbb22d7e1f6f` (unfinished macOS port).
Microsoft's code is MIT licensed. Its dependencies retain their own licenses.
See the fetched SDK's `LICENSE` and `extern/` dependency licenses.

`cmake/Dependencies.cmake` contains our reproducible macOS fixes: correct native
`pthread_once_t` storage/initialization, extended locale declarations, and enabling
the UVC color backend on Darwin. `cmake/sdk/` limits the build to the SDK libraries
needed by the capture/projection pipeline and supplies the system libusb target.

## OpenK4A

Fetched from `https://github.com/LukeSchoen/OpenK4A.git`, commit
`f192ef34b3df2845d944c7065ce6d1ec4b7bdb7a`, MIT licensed. See its `LICENSE`.
The notice is also retained in `LICENSES/OpenK4A.txt` for our derived shader.
We compile its unmodified frame and CPU depth-model implementations, with a small
platform adapter and extracted portable declarations. `filter.metal` is a port
of its `depth_pixel` and `atan2_turns` algorithms under the same MIT license.
`accelerated.c` composes its private stages with our Metal filter. Its builtin calibration is
from another device; our demo explicitly reports that limitation.

Dependency source and license files are retained in `<build>/_deps/`. Preserve
the applicable notices when distributing binaries. The project's existing MIT
license retains the Microsoft attribution from its SDK ancestry.
