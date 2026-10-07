# Native depth pipeline

## Architecture

1. `src/depth/live.c` opens the depth MCU through the SDK's native libusb
   transport and requests NFOV unbinned capture at 30 fps with Metal or 15 fps
   with the CPU backend.
2. A callback validates frame length and publishes into one pending raw-frame
   slot. When decoding falls behind, a new frame replaces the pending one.
3. OpenK4A's CPU code expands the measurements and projects phase vectors. A
   Metal compute shader runs the 5×5 neighborhood filter and phase extraction.
   CPU code then de-aliases the frequencies and converts radial distance to Z.
4. SDK CPU calibration functions project samples into the 1280×720 RGB view
   using intrinsics/extrinsics read from the connected device. A 3×3 footprint
   fills upsampling gaps; nearest depth wins overlapping projections.
5. Completed frames are published by atomic rename. The demo reads complete
   frames and applies the selected distance cutoff as RGB alpha.
6. FFmpeg captures RGB separately through macOS AVFoundation at 1280×720,
   publishing JPEGs at 10 fps. Python serves the preview on localhost only.

Use `python3 scripts/run_demo.py --build build-native` to run all three processes.
Logs and current frames live in `build-native/live/`. Ctrl-C shuts down the
launcher and releases the camera. The source does not require administrator
access for the tested capture path.

## HTTP endpoints

- `/`: RGB, cutout and diagnostic browser views
- `/rgb.jpg`: latest RGB frame
- `/frame.bin`: latest 640×576 depth frame
- `/aligned.bin`: latest 1280×720 depth projected into RGB coordinates
- `/depth.png?near=500&far=4000`: depth-camera grayscale PNG
- `/mask.png?cutoff=1500`: depth-camera binary mask PNG (not RGB-aligned)

Endpoints reject frames older than two seconds. `X-Frame-Time` is file publication
time in Unix seconds, **not exposure time**. The browser also rejects RGB/aligned
depth publications more than 500 ms apart. That guards gross stalls but is not
camera synchronization. Transparent preview pixels reveal the checkerboard.

## Binary frame protocol

Both `.bin` outputs start with five uint64 values, little endian:

| Offset | Value |
|---|---|
| 0 | Magic `0x315448504544344b` |
| 8 | Received sequence number |
| 16 | Host monotonic receipt time, nanoseconds |
| 24 | Sensor exposure ticks, 90 kHz |
| 32 | CPU decode duration, nanoseconds |

Row-major uint16 little-endian nominal millimeter depths follow the 40-byte
header. Zero denotes invalid/unknown depth. Dimensions are fixed by the endpoint;
there is no per-frame dimension field. The aligned frame retains depth-camera Z
values, located in RGB pixel coordinates. This is a prototype interface, not yet
the intended OBS transport/API.

## Offline decode

```sh
build-native/bin/decode_depth path/to/depth.raw output-mm.bin
```

Input is a complete 5,310,760-byte NFOV raw frame. Output is a 640×576 uint16
millimeter array without the live-frame header. Existing SDK-era captures can
still be decoded this way. The initial phase-image experiments have been removed;
they are superseded by the OpenK4A CPU decoder.

## Metal backend

`accelerated.c` includes the pinned upstream model implementation to access its
private stages without duplicating the opaque model layout. `filter.metal` ports
the scalar neighborhood filter and phase-angle approximation. `metal_filter.mm`
owns persistent shared buffers, a command queue and a compute pipeline; shader
compilation happens once with fast math disabled. A synchronous GPU completion
wait protects buffer reuse and readback. One decoder instance is used by one
capture worker; its buffers are not safe for concurrent calls.

Only the filter/phase-extraction stage is GPU accelerated in this version.
Unpacking, temperature/calibration preparation, phase projection, final distance
reconstruction, depth-to-RGB reprojection and the demo transport remain CPU work.
Output is copied from shared GPU buffers back into the existing pipeline.

If GPU initialization or a command fails, decoding falls back to the scalar CPU
filter and logs the failure. `KINECT_DEPTH_BACKEND=cpu` explicitly disables Metal;
the launcher exposes this as `--backend cpu`. `decode_depth` remains CPU-only as
an offline reference. The frame header's decode duration measures the full hybrid
decode (including submission/wait/copies), not only GPU kernel execution.

`benchmark_depth` checks a supplied raw recording after warmup over ten iterations,
comparing final uint16 outputs and invalid masks against the CPU path. It fails
if more than 36 pixels differ in validity or by more than 2 mm, and reports all
differences. Two local captures matched exactly on the M1, with approximately
4.8–5.0× decoder speedup. This does not establish calibration accuracy.
CTest also exercises the filter against an independent scalar calculation on
small grids, borders, dim samples, and entirely invalid neighborhoods; it skips
that GPU test when no Metal device is available.

## Calibration distinction

The connected device's **camera geometry** is applied and saved as
`camera-calibration.json`. Its **ToF conversion calibration** is not yet applied:
OpenK4A's `block_expand()` returns false and the demo uses builtin tables from
another unit. Thus the output is reconstructed depth, but its distance accuracy
on this Kinect remains unverified. An `OPENK4A_DEPTH_TABLES` file can supply
tables using upstream's supported format.

Black gaps at boundaries can result from rejected measurements. Unknown pixels
must not automatically be interpreted as distant background. Retaining them
reduces holes but can also retain the room. The soft-transition control feathers
in distance, not spatially across image edges.

## Verification and remaining OBS work

The standalone build was checked against a saved raw capture and produced the
same depth array as the prior prototype. CTest checks PNG encoding, distance
endpoints and invalid-pixel mask behavior. Live hardware testing checks advancing
sensor timestamps, changing depth values and the RGB cutout in a browser.

Before a production OBS source/filter:

- Validate known distances and per-device ToF calibration.
- Pair color/depth exposure timestamps with bounded skew.
- Improve projection rasterization and occlusion handling at silhouettes.
- Add explicit invalid-depth policy, spatial/temporal edge treatment, and a
  low-latency native frame interface.
- Integrate the result as an OBS plugin rather than a browser demo.
