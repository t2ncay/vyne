---
id: libs
title: Library Overview
group: Standard Library
icon: fa-cubes
order: 1
description: The module map, import conventions, kernel/wrapper pattern, and every library in the repository.
---

# Library Overview

Vyne has two library layers:

- **Native modules** implemented in C++ for the desktop runtime or `static inline` C for AVR. They are available through `use native <name>`.
- **External libraries** written in Vyne itself under `vyne/modules/external/`. They are imported through `use external "<lib>/<facade>.vy"`.

The external layer is where most numerical work lives. Every library follows the same kernel/wrapper structure: hot loops over typed arrays live in `Kernels.vy`, typed interfaces live in `Types.vy`, validation and allocation live in `Ops.vy` or the leaf modules, and a facade re-exports the surface.

## Native modules

| Module | Purpose |
| :--- | :--- |
| [`vcore`](page:vcore) | Time, sleep, platform, input, string builder, process/memory info, realtime telemetry. |
| [`vmath`](page:vmath) | Trig, exp/log, rounding, sigmoid/relu, clamp, PCG32 random, constants. |
| [`vmem`](page:vmem) | Arena checkpoints, peak/live accounting, commit/stream stats, diagnostics. |
| [`vfs`](page:vfs) | Filesystem read/write, lines, bytes, walk, glob, mkdir, path operations. |
| [`arduino`](page:arduino) | AVR-only: 110 bindings for serial, GPIO, ADC, I2C/SPI, EEPROM, bits, interrupts, watchdog, realtime. |

```vyne
use native vmath;
use native vfs;
use native vmem;

out(vmath.sqrt(9.0));
out(vfs.read("data/input.txt"));
out(vmem.total_allocated());
```

## Numerical libraries

| Library | What it gives you |
| :--- | :--- |
| [`vlin`](page:vlin) | Dense matrices and vectors, arithmetic, products, reductions, activations, in-place training kernels. |
| [`vml`](page:vml) | Dense layers, Sequential, activations, losses, SGD, Adam. |
| [`vfft`](page:vfft) | Radix-2 FFT with cached twiddle factors, inverse, magnitude/phase. |
| [`vrand`](page:vrand) | Continuous/discrete distributions and sampling. |
| [`vstat`](page:vstat) | Descriptive statistics, correlation, regression, histograms, streaming moments, PSNR/SSIM. |
| [`vcomplex`](page:vcomplex) | Complex scalars, elementary functions, polar helpers, split-array kernels. |
| [`vinterp`](page:vinterp) | 1-D curves and 2-D grids: nearest, linear, cubic, spline, bicubic. |
| [`voptim`](page:voptim) | SGD, momentum, Nesterov, Adam, AdamW, RMSProp, Adagrad, schedules. |
| [`vsparse`](page:vsparse) | CSR, SpMV, residuals, conjugate gradient. |
| [`vdecomp`](page:vdecomp) | LU, QR, Cholesky, Jacobi eigen, singular values, direct solves. |

## DSP & control

| Library | What it gives you |
| :--- | :--- |
| [`vsignal`](page:vsignal) | Moving averages, exponential/FIR/median/biquad filters, windows, resampling, spectra. |
| [`vcontrol`](page:vcontrol) | PID, complementary filter, scalar and constant-velocity Kalman, state-space models. |

## Data & plotting

| Library | What it gives you |
| :--- | :--- |
| [`vcsv`](page:vcsv) | RFC 4180 CSV/TSV parse/write, typed columns, vfs-backed I/O. |
| [`vjson`](page:vjson) | JSON parse, serialize, validate, path access. |
| [`vtermplot`](page:vtermplot) | Line, scatter, bar, histogram, heatmap, sparkline in the terminal. |

## Image & vision

| Library | What it gives you |
| :--- | :--- |
| [`vimage`](page:vimage) | PNG/JPEG/BMP/TGA I/O, bilinear resize, crop, grayscale. |
| [`vconv`](page:vconv) | Conv2d, activations, pixel shuffle, backward kernels, weight bundles. |

## Embedded buses

| Library | What it gives you |
| :--- | :--- |
| [`vi2c`](page:vi2c) | Typed, range-checked I2C master helpers. |
| [`vspi`](page:vspi) | Typed, range-checked SPI master helpers. |

## Domain libraries

| Library | What it gives you |
| :--- | :--- |
| [`vbio`](page:vbio) | DNA/RNA/protein types, codon table, translation, composition, melting temperature. |
| [`vchem`](page:vchem) | Periodic-table constants and element properties, CSR graph construction, BFS/DFS/Dijkstra. |

## Helpers

| File | What it gives you |
| :--- | :--- |
| [`vcolors.vy`](page:vhelpers) | ANSI color and terminal styling constants. |
| [`vconvert.vy`](page:vhelpers) | Temperature, distance, and unit conversions. |
| [`vplot.vy`](page:vhelpers) | A minimal bar chart helper. |
| [`vplot/vplot.vy`](page:vplot) | Universal colorful charts, bounded live series, ESP32 and image surfaces. |
| [`vstring.vy`](page:vhelpers) | Small string helpers (`upper`, `lower`, and similar). |

## Import conventions

```vyne
use native vmath;                      # native module
use external "vlin/vlin.vy";           # external library facade
use external "voptim/voptim.vy";
use "model.vy";                        # project-local source file
```

A facade imports its leaves and ends with `deploy`:

```vyne title="vyne/modules/external/vsignal/vsignal.vy"
use "Types.vy";
use "Constructors.vy";
use "Kernels.vy";
use "Iir.vy";
use "Fir.vy";
use "Windows.vy";
use "Resample.vy";

ruleset { dynamic_casting };
module vsignal;

deploy vsignal;
```

## Kernel/wrapper pattern

```
Kernels.vy    # unboxed loops over Array<Float64>; native-array ABI
Types.vy      # interfaces: Matrix, Series, Dense, filter/controller state
Ops.vy / ...  # validation, allocation, dispatch
<lib>.vy      # facade: imports leaves, declares module, deploys
```

Kernels have the shape `(Array<Float64>, Int64) -> Float64` or `(Array<Float64>, Array<Float64>, Int64) -> Int64`. The transpiler recognizes the shape and emits a direct C function. Wrappers handle shape validation, allocation, and dispatch.

- `vstat` and `vinterp` keep a typed buffer and its length together in a `Series` / `Curve`.
- `vsignal` and `vcontrol` keep streaming state in an `Array<Float64>` field, so a step updates shared state instead of returning a new struct.
- `voptim` keeps parameters and moments in typed arrays and exposes native update kernels plus pure schedules.
- `vsparse` stores CSR values/indices/row pointers in typed arrays; `cg` reuses one `Ap` buffer so peak memory is O(rows) independent of iteration count.
- `vdecomp` takes flat row-major arrays plus dimensions in raw routines and adds `Matrix` wrappers for solves and factorisations.

## BLAS dispatch

`vlin.multiply` and `vlin.multiply_trans_b` lower to `cblas_dgemm` when the compiler is invoked with `--blas`. The dispatch table lives in `compiler/codegen/blas_dispatch.h`; the runtime bridge is `runtime/detail/blas_bridge.h`. Source is unchanged between modes; only the matched call lowering differs. `--blas` is desktop-only.

## Tests and benchmarks

Each library ships tests and at least one benchmark:

- `examples/test/<lib>_test.vy` (older libraries) or `examples/libs/<lib>_test.vy` (newer libraries).
- `examples/benchmark/<lib>_bench.vy` or `examples/libs/<lib>_bench.vy`.

Run them with `vynec --compile <file>` followed by the produced executable, or `vynec run <file>` for interpreter-friendly tests.

## Legacy modules

`vglib`, `vaudio`, `vserv`, `vnet`, `vcv`, and `vurage` are no longer maintained. The language's purpose has shifted to scientific numerical computing with a first-class embedded target. The code may still exist for archival purposes, but it is not part of the supported surface, is not tested in CI, and is not documented here. Pin to a pre-0.4 tag if you depend on them.

## See also

- [Groups & Modules](page:mods)
- [Projects & Tooling](page:projects)
- [SRCNN Showcase](page:srcnn)
