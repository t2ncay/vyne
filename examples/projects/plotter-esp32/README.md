# Universal vplot live dashboard and plotting acceptance

This project is the main test/demo workspace for **vplot**. Its chart core is
shared by ESP32 displays and raster/vimage output. The HW-950 demo attaches to
the verified configured display and never reconfigures its wiring or orientation.

## Live ESP32 demo

```sh
cd examples/projects/plotter-esp32
vynec hardware setup
vynec hardware show
vynec build
vynec run --port COM5
```

Two colored synthetic signals use 512-sample rings, a ten-second scrolling
window, stabilized autoscaling, major/minor grids and a 16 KiB strip buffer.
The application schedules sampling separately from a requested 100 ms refresh
interval; actual acquisition/refresh timing depends on rendering and device load.
Serial `VPLOT_STATUS` reports frames, samples, native memory/allocation count,
ESP32 free heap and Vyne arena usage. Add `--monitor` to observe it manually.

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
```

`--sanitize` requires an ASan/UBSan-capable C toolchain (used on Linux CI).
`--core-only` requires C/C++ compilers but no Vyne compiler. Normal runs build
and execute the native and Vyne tests, decode all PNGs with CRC/filter checks,
and test argument-count/backend diagnostics. `--esp32` additionally uploads the
device regression, verifies direct/buffered rendering, all chart kinds, rotation,
viewport/capability checks and cleanup, then leaves the live dashboard running.
Close other COM5 monitors while uploading/capturing.

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
