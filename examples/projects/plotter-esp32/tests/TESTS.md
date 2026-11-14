# Plotting coverage catalog

| Test | Coverage |
| --- | --- |
| `core.c` | Empty/single/constant/non-finite data; finite/reversed/degenerate/overflowing ranges; line/scatter/signed bars/heatmaps; clipping; palettes; responsive 32x32 through 1024x768 layout; long titles/legends; grid/style changes; rings and logical order; borrowed mutation; visible-window autoscale; hysteresis; narrow spikes; cache reuse; 500 redraws; 1000 lifecycle repetitions; stale handles; allocation-failure rollback |
| `adapters.c` | 128 pixel-equivalent RGB888 versus RGB565 band cases: portrait/landscape, four chart primitives, both themes, band heights 1/3/17/64, full and inset viewports; dense min/max-reduced lines; no rendering allocations; transparency capability rejection |
| `bridge.c` | Native types, RGB limits, string length, malformed typed-array metadata, constructor failures, pool exhaustion, Float64/Float32/Int64 copies, capacity errors, histogram bin edges/counts and repeated updates, image payload/ownership, grayscale/RGB/RGBA binding, invalid handles/types/viewports/scales, cleanup |
| `managed.c` | Managed defaults/palettes, automatic legend/user override, owned surface reuse/recreation/cleanup, allocation failure rollback, 16/8/4 KiB/direct fallback policy, direct PNG export, no export arena copy, automatic timestamps, millisecond wrap and exact fake-clock redraw throttling, allocation-free pushes |
| `friendly.vy` | Primary-import managed chart/save plus all five quick PNG helpers, directory creation, decoded 640x360 images and cleanup |
| `friendly_device.vy` | Automatic configured display initialization, all five quick display helpers, existing rotation preservation, transient/managed cleanup and physical redraw-rate bound |
| `friendly_live.vy` | Short-API live updates with instrumented native/arena/heap metrics |
| `api.vy` | Actual compiler/native integration, wrapping ring samples, copy and borrowed data, updates, render counters/storage stability, image conversion/overlay and destroyed-handle errors |
| `aliases.vy` | Native aliases resolve the same functions/headers and resource lifecycle |
| `legacy.vy` | Existing standalone text bar helper remains usable |
| `memory.vy` | 1001 append/render iterations with unchanged native allocation count, owned bytes and Vyne arena count, without a region rewind |
| `gallery.vy` | Five colorful chart types, themes, labels/grids/legend/colorbar and PNG export |
| `run.py` PNG checks | Signature/IHDR, chunk CRCs, zlib/filter decoding, dimensions and multiple actual pixel colors for every generated chart |
| `run.py` compile-negative cases | Wrong native arity (including constants), ESP32-only surfaces on host and image surfaces on ESP32 |
| `run.py` initialization | vplot library selection/version and a fresh generated quick-chart starter without display/surface setup; physical build/upload when COM5 tests are selected |
| `device.vy` | Real ESP32 scalar conversions/logical not, direct and buffered surfaces, rotated dimensions, all five chart types, inset viewport, unsupported transparency and native cleanup |
| Live `src/main.vy` capture | Advancing frames/samples, bounded ring count, stable native allocation count/bytes and Vyne arena, bounded free-heap drift, panic/error absence and operator visual confirmation |

ASan/UBSan runs cover the three C tests on Linux. The core is also compiled as
C++17 for Arduino compatibility. Hosted CI executes the host suite; physical
COM5 runs are separate hardware acceptance. Pixel parity verifies renderer math,
while physical appearance is confirmed on the actual display.

Scope is the supported vplot API/backends. SVG, window interaction, log scales,
Unicode font shaping and other future backends are not claimed as tested.
