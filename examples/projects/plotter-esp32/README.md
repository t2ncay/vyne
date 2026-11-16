# Universal vplot live dashboard and plotting acceptance

This project is the main test/demo workspace for **vplot**. Its chart core is
shared by ESP32 displays and raster/vimage output. The HW-950 demo attaches to
the verified configured display and never reconfigures its wiring or orientation.

## Your first ESP32 chart

```vyne
use external "vplot/vplot.vy";
xs := [0,1,2,3];
ys := [22.0,23.4,21.8,24.1];
vplot.show_line(xs,ys,"Temperature");
```

Selecting `--libraries vplot` when initializing an HW-950 project now generates
a short starter like this. Configured display initialization, colors, ranges,
buffer selection and temporary cleanup are automatic.

## Live ESP32 Wi-Fi packet-rate dashboard

```sh
cd examples/projects/plotter-esp32
vynec hardware setup
vynec hardware show
vynec build
vynec run --port COM5
```

`src/main.vy` captures observed **2.4 GHz Wi-Fi frame events on fixed channel 6**
and plots management (blue), data (orange), and control (pink) rates. Edit
`CHANNEL` at the top to select another channel permitted by the driver's country
configuration; capture never widens that configuration. No network credentials
or association are needed. Counts include retries and driver-delivered aggregates;
they are not unique packets, IP traffic, all-channel activity, or network throughput.

Three 512-point rings share the same timestamps and ten-second scrolling axes.
The main loop requests a sample every 500 ms, divides each counter by the **actual
64-bit microsecond interval**, appends all three series, and redraws once. TFT
rendering adds time to that interval; this is not a hard realtime deadline.
There is no channel hopping or packet-payload storage. `SNIFF_READY` and periodic
`SNIFF_STATUS` serial messages report the channel, counters, elapsed time, chart
progress, native storage and heap/arena measurements. Send **Q** to stop capture
and explicitly release the chart/surface; reset the board to start again.

Capture owns Wi-Fi while active. Start rejects a radio already in use, invalid
channels, or driver setup failures; do not call connect/scan/mode/HTTP helpers
during capture. The receive callback only updates synchronized, saturating counters;
it never calls Vyne, draws, allocates, or retains driver buffer pointers. Sampling
atomically copies/resets pending counters. Rendering is single-threaded in the
application. Stop disables capture, detaches the callback, and turns off the
owned radio without erasing credentials or changing display/touch/NVS settings.
Wi-Fi initialization and chart creation consume heap explicitly; the steady-state
counter/append/render path reuses existing storage. A saturated interval is
reported as an error rather than plotted as an exact rate.

The original short sine/cosine demo is preserved as `examples/synthetic_live.vy`:

```sh
vynec run examples/synthetic_live.vy --port COM5
```

An instrumented synthetic version lives in `tests/friendly_live.vy`; its serial
`VPLOT_STATUS` reports frames/samples and memory counters. The original
explicit-surface dashboard remains available as `examples/advanced_live.vy`.

The original initialized touch demo is preserved:

```sh
vynec run examples/touch.vy --port COM5
```

That demo accepts serial **C** for calibration. Plotting does not change NVS
calibration or the measured ST7796/HSPI/GPIO27/Y-correction preset.

## Host chart gallery

Use the desktop flag-based command rather than the project's ESP32 target:

```sh
vynec --run -o build/gallery tests/gallery.vy
```

Create `build/plots/` first, or use the test runner which creates output folders.
The gallery exports `line.png`, `scatter.png`, `bars.png`, `histogram.png` and
`heatmap.png` under `build/plots/`. `tests/api.vy` exercises vimage overlays and
conversion; `tests/memory.vy` verifies the steady-state native and Vyne allocation
counts remain unchanged without relying on region rewinds.

## Comprehensive plotting tests

Run from the repository root:

```sh
python examples/projects/plotter-esp32/tests/run.py
python examples/projects/plotter-esp32/tests/run.py --sanitize --core-only
python examples/projects/plotter-esp32/tests/run.py --esp32 --port COM5 --seconds 60
# New capture-specific host and physical lifecycle/dashboard checks:
python examples/projects/plotter-esp32/tests/run_sniff.py
python examples/projects/plotter-esp32/tests/run_sniff.py --sanitize --core-only
python examples/projects/plotter-esp32/tests/run_sniff.py --esp32 --port COM5 --seconds 60
```

`--sanitize` requires an ASan/UBSan-capable C toolchain (used on Linux CI).
`--core-only` requires C/C++ compilers but no Vyne compiler. Normal runs build
and execute the native and Vyne tests, decode all PNGs with CRC/filter checks,
and test argument-count/backend diagnostics. `--esp32` additionally uploads the
device regression, verifies direct/buffered rendering, all chart kinds, rotation,
viewport/capability checks and cleanup, then leaves the live dashboard running.
Close other COM5 monitors while uploading/capturing.
The capture-specific device runner first checks invalid/busy-radio rejection and
three start/stop cycles, then uploads `src/main.vy`, verifies real traffic and
stable storage, sends Q to check cleanup, and resets into the dashboard. A quiet
channel with no observed traffic cannot satisfy its physical receive assertion.
The ordinary host suite now includes the injected sniffer-driver tests and native
arity/alias/optional-feature checks; no radio hardware is inferred from host tests.

All tests and test sources are in this folder; outputs remain in `build/`.
`tests/TESTS.md` lists the exact coverage. Set `VYNEC` to test another compiler,
including an installed distribution. `xmake test-vplot` runs the host suite.

## Public API and ownership

See [vplot API](../../../vyne/modules/external/vplot/README.md). Plots/surfaces
are generation-checked Int64 handles. Plots own series; destroy plots and
surfaces explicitly. Copied data is owned; borrowed Float64 arrays and image
pixels must remain alive and unresized. Mutations/render return Bool and
`vplot.last_error()` explains failures. Native buffers are independent of Vyne
region rewinds, with no append/render allocations after setup.

The baseline font, scaling, clipping, palettes and chart layout are shared
across outputs. New C/C++ surface adapters can implement pixel/span and frame
hooks without adding hardware-specific chart algorithms.

## Verified results

- Source-tree and installed-distribution host suites passed.
- Native core/bridge/adapter tests passed ASan/UBSan on Ubuntu/WSL.
- C and C++ core test executables each completed 147523 checks; band tests
  completed 128 pixel-equivalent scenarios.
- Five decoded 400x300 PNGs passed integrity/color checks.
- Physical COM5 device regression passed direct/buffered rendering, all five
  chart types, rotation, capability rejection and cleanup.
- Final installed-compiler 120-second live capture observed 751 completed
  frames and a wrapped 512-sample ring. Owned native storage stayed at 33712
  bytes with 8 setup allocations, the Vyne arena stayed at 0, and free heap
  ranged from 225828 to 225916 bytes.
- The owner confirmed correct live traces, colors, labels and orientation.

These are measured results on the classic ESP32 HW-950 / ESP32 core 3.3.12 /
TFT_eSPI 2.5.43. The refresh interval is a request, not a fixed-frame-rate guarantee.

### Managed-API follow-up

The new quick/managed suite also passed on Windows, Ubuntu ASan/UBSan, and the
installed distribution. The six-line generated source (comment, import, two
arrays, one chart call) was built/uploaded on COM5, and the owner confirmed its
appearance. Device checks confirmed quick-helper cleanup and preservation of an
explicit portrait rotation. A 60-second managed live capture observed 385
frames, stable 33760 native bytes / 8 setup allocations / 0 arena bytes and
constant 225892-byte free heap. Clock wrapping, exact throttling, ring wrapping
and buffer fallback are additionally exercised with a deterministic test clock.

### Fixed-channel Wi-Fi capture (2026-11-16)

Verified on the same classic ESP32 HW-950 / COM5, Arduino ESP32 core 3.3.12,
TFT_eSPI 2.5.43, arduino-cli 1.5.1, using the source-tree compiler rebuilt with
`xmake`. Windows host tests passed with GCC/G++ 15.2.0; the real driver logic with
injected hardware outcomes passed ASan/UBSan on Ubuntu/WSL G++ 15.2.0. Tests cover
classification, null/MISC/error/short receive events, setup failure rollback,
country/ownership checks, stopped/zero-duration sampling, counter saturation,
unsigned clock wrapping, 100,000 contended events with exact conservation, and
no callback/snapshot allocations. The existing host plotting/image/golden/safety
regressions also passed. The portable concurrency test uses a host mutex; actual
ESP32 task/critical-section operation is additionally exercised by live capture.

The 60-second device run produced 23 status rows, advancing from 5 rendered
frames / 4 samples to 93 frames / 92 samples. Management, data, and control counts
were nonzero in actual channel-6 traffic. Examples of raw interval measurements:

| Elapsed us | Management | Data | Control | Frames | Samples | Native bytes | Allocations | Free heap | Arena bytes |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 626000 | 18 | 1 | 8 | 5 | 4 | 42128 | 10 | 76248 | 32 |
| 633000 | 17 | 4 | 9 | 41 | 40 | 42128 | 10 | 76248 | 32 |
| 636000 | 16 | 1 | 5 | 93 | 92 | 42128 | 10 | 76248 | 32 |

These are selected sampling windows, not all frames in the entire capture. Rates
use the per-row interval (first row: approximately 28.75 / 1.60 / 12.78 frames/s).
The chart's labeled units and colors are configured in source; serial verification
does not establish visual readability or perfect reception of all over-the-air
frames. User visual confirmation remains pending. Intervals in the final status
series ranged from **620000 to 636000 us**; no promised 2 Hz frame rate is claimed.

Native chart storage stayed **42128 bytes / 10 setup allocations**, the Vyne arena
stayed **32 bytes** (one setup title concatenation), and free heap stayed **76248
bytes** throughout the reported windows. Startup free heap was **173484 bytes**;
after Wi-Fi initialization it was **121884 bytes**, an observed reduction of
**51600 bytes**, before chart creation. Q reported **native_bytes=0** and free
heap **154196 bytes**. Heap did not return to its pre-initialization value; the
remaining **19288-byte difference** is visible, not called a memory gain or proven
leak. A separate radio-only lifecycle firmware reported **221556 free bytes / 0
arena bytes** after each of three start/stop cycles, demonstrating warmed-cycle
stability for that binary. Its static footprint differs from the chart firmware,
so their absolute heap figures must not be compared as a speed/memory improvement.

An initial lifecycle test's strict `delay(1000)` lower-bound assertion failed:
Arduino delay is RTOS-tick-based and can wake slightly short of a wall-clock
second. The test now permits a 10 ms tick-alignment tolerance; acquisition/rates
always use the measured microsecond interval rather than assuming the requested
delay elapsed exactly. Failure messages are drained before termination so an
ESP32 reset cannot hide the test's diagnostic.

The initial successfully built dashboard used **935432 bytes** of program storage.
The final built/uploaded source, including diagnostic draining before error exits,
uses **935532 bytes** of program storage and **112480 bytes** of static globals
(limits 1310720 / 327680). These linker figures are not live heap usage. The
60-second evidence preceded that error-path-only addition; exact generated
artifacts and local logs stay under ignored
`build/sniff-tests/` and `build/firmware/`. No throughput, memory reduction, MCU
stack bound, lossless capture, or other ESP32-board verification is claimed.
An additional eight-second reset/capture verified the final uploaded firmware's
`SNIFF_READY` and advancing status without errors. That boot reported **76088**
free heap bytes during plotting (160 bytes below the preceding capture), with
the same **42128 native bytes / 10 allocations / 32 arena bytes**. These are
observations across boots, not a guarantee of identical driver heap consumption.

Deterministic regions, explicit promotion/ownership and no-GC language/runtime
remain intact. The callback introduces no Vyne evaluation or aliasing changes and
owns no arena references. The feature is opt-in (`VYNE_USE_WIFI_SNIFF`); ordinary
Wi-Fi sketches and non-ESP32 targets do not link capture machinery. This feature
does not resolve the optimization roadmap's pending C03/C17/R20 or other findings.
