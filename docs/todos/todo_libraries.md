# TODO — Standard Library

**Last updated:** 2026-11-10

Live roadmap for the Vyne standard library. This file replaces the earlier
draft, which predated the native array ABI, the region-policy work, and the
first batch of external libraries. It records what ships, what was learned,
what the next libraries are, and the long tail.

> **Status snapshot (2026-11-10).** The top-10 numerical set is now complete
> except `vnum`: `vstat`, `vtermplot`, `vcomplex`, `vinterp`, `vcsv`,
> `vsignal`, `vcontrol`, `voptim`, `vsparse`, and `vdecomp` ship. `vimage`,
> `vconv`, `vi2c`, and `vspi` also exist. `vchem` is partial
> (`vchem_common` + `vgraph`). The "remaining top-10 library" section is now
> just `vnum`.

Companion documents:

- `todo.md` — Paper 1 pipeline (regions, scratch, peak bounds).
- `todo_2.md` — post-Paper-1 feature axes (shape/range/unique/effect types,
  device regions, region policies).
- `todo_optimization.md` — codegen peepholes and structural performance work.
- `todo_rulesets.md` — per-file safety rulesets.
- `todo_drug_discovery.md` — the `vchem` pipeline.
- `milestone.md` (repo root) — the overall scientific/numerical roadmap.

The through-line is unchanged: **a good Vyne library has a natural per-call
boundary, does arithmetic on native types, and gives the caller a compile-time
peak bound.** If a candidate fails any of the three, it is a fight with the
language, not a use of it.

---

## What changed in this revision

The compiler is no longer frozen behind Paper 1 for library work. The native
array ABI, native struct ABI, region policies, autodiff, and the library
batches have landed. The standard library now has a broad external surface in
the tree: every top-10 item except `vnum`, plus `vsignal`, `vcontrol`,
`vimage`, `vconv`, `vi2c`, and `vspi`. The three libraries added in this
revision - `voptim`, `vsparse`, and `vdecomp` - are the numerical stack's
killing features; see the section below.

The top-10 scientific/numerical set was defined as:

| # | Library | State | PR |
| --- | --- | --- | --- |
| 1 | `vstat` — statistics | **Shipped** | #104 |
| 2 | `vtermplot` — terminal plotting | **Shipped** | #105 |
| 3 | `vcomplex` — complex numbers | **Shipped** | #107 |
| 4 | `vinterp` — 1-D/2-D interpolation | **Shipped** | #108 |
| 5 | `vcsv` — CSV/TSV tables | **Shipped** | #109 |
| 6 | `voptim` — optimizers | **Shipped** | this PR |
| 7 | `vsignal` — signal processing | **Shipped** | #121 |
| 8 | `vsparse` — sparse matrices | **Shipped** | this PR |
| 9 | `vdecomp` — decompositions/solvers | **Shipped** | this PR |
| 10 | `vnum` — numerical methods | **Next** | — |

`vcontrol` (PID, complementary filter, Kalman1D/Kalman2, state-space) shipped
alongside `vsignal` as the first control-systems library (#121). `vimage` and
`vconv` also shipped (#112). `voptim`, `vsparse`, and `vdecomp` now ship as
well, leaving `vnum` as the only top-10 item still missing.

## Top 3 killing-feature libraries

These are the three libraries that turn the numerical stack from "useful
primitives" into a reason to choose the language. Each ships in
`vyne/modules/external/` with the standard layout (`Types`, leaf modules,
facade, `README.md`), a compiled test under `examples/libs/`, and a benchmark.

### 1. `voptim` - optimizers that make training loops real

**Why it kills.** A numerical language is judged by whether a training loop
can run without falling back to another runtime. `voptim` provides SGD,
momentum/nesterov, Adam, AdamW, RMSProp, and Adagrad over flat typed buffers,
plus constant/step/exponential/cosine/warmup-cosine schedules. State is built
once and every update is in place; the update kernels are native-ABI leaves.

**Status:** core MVP shipped. Files: `Types.vy`, `Kernels.vy`, `Schedules.vy`,
`Ops.vy`, facade, README. Test: `examples/libs/voptim_test.vy`. Benchmark:
`examples/libs/voptim_bench.vy` (100k parameters).

**Deferred:** optimizer protocol polymorphism, weight-decay variants beyond
AdamW, and integration examples against a full `vlin` training loop.

### 2. `vsparse` - sparse systems with a bounded working set

**Why it kills.** Sparse matrices are where PDEs, graphs, force fields, and
graph Laplacians live. `vsparse` ships CSR construction (`zeros`, `identity`,
`diagonal`, `from_dense`, `to_dense`, `transpose`, `copy`), native-ABI SpMV,
residual/Frobenius/dot helpers, and conjugate gradient. CG reuses one `Ap`
buffer and keeps peak memory at O(rows) independent of iteration count.

**Status:** core MVP shipped. Files: `Types.vy`, `Kernels.vy`,
`Constructors.vy`, `Ops.vy`, facade, README. Test:
`examples/libs/vsparse_test.vy`. Benchmark: `examples/libs/vsparse_bench.vy`
(10k tridiagonal SpMV, 1k CG).

**Deferred:** CSC, SpMM, ILU/preconditioners, and graph-Laplacian helpers.

### 3. `vdecomp` - LAPACK-shaped solves in pure Vyne

**Why it kills.** A scientific language needs factorizations, not just
matvecs. `vdecomp` ships partial-pivot LU (`solve_lu`, `determinant`,
`solve`, `inverse`), Householder QR (`qr`, `least_squares`), Cholesky
(`cholesky`, `solve_cholesky`, `solve_spd`), symmetric Jacobi eigen
(`eigen_symmetric`, `eigen_matrix`), and singular values via A^T A.

**Status:** core MVP shipped. Files: `Types.vy`, `Util.vy`, `LU.vy`, `QR.vy`,
`Cholesky.vy`, `Eigen.vy`, `Ops.vy`, facade, README. Test:
`examples/libs/vdecomp_test.vy`. Benchmark: `examples/libs/vdecomp_bench.vy`
(64x64 and 128x128 factorizations).

**Tradeoff:** the SVD surface is `singular_values` via A^T A, which squares the
condition number. It is documented as such; one-sided Jacobi is the follow-up
for ill-conditioned inputs. There is no LAPACK dependency, so the library stays
portable and runs under the same region/peak-bound rules as the rest of Vyne.

---

Two codegen/runtime fixes came out of this batch and are now load-bearing for
library work:

- **Float64 literal form.** Whole-valued doubles were emitted without a decimal
  point, so `32.0 / 7.0` lowered to integer division. `floatLit` now appends
  `.0` when needed and maps inf/nan to `INFINITY`/`NAN`.
- **`vcore.sb_append_char`.** The existing runtime byte append is now exposed.
  Parsers can scan with `String.byte_at` and append bytes without allocating a
  one-character string per input byte. The 274 KB / 10k-row CSV benchmark went
  from ~2270 ms to ~1 ms.

---

## Delivery workflow

The current process is one library per branch/PR.

- **One library per PR.** Stack the next branch on the previous one when its
  integration test needs the earlier libraries; otherwise branch from `master`.
- **Directory shape.** `<lib>/<lib>.vy` facade, leaf modules named by
  responsibility (`Types.vy`, `Kernels.vy`, `Ops.vy`, `Reductions.vy`,
  `Constructors.vy`, domain leaves), and `README.md`.
- **Tests.** `examples/test/<lib>_test.vy` with a `check(name, cond)` helper and
  a non-zero exit if any check fails.
- **Benchmarks.** `examples/benchmark/<lib>_bench.vy`, using `vcore.now_ns()`.
- **Cross-library tests.** Put integration tests on the last branch of a stack,
  e.g. `examples/test/libraries_integration_test.vy`, so they can import every
  library in the stack.
- **Verification per PR.** `xmake build` if the compiler/runtime changed, the
  library test, its benchmark, the affected integration test, and the safety
  suite (`examples/safety/run_safety.ps1`, currently 10/10).
- **Commit shape.** Separate commits for runtime prerequisites, the library,
  tests/benchmarks, and docs. Headers and bodies explain the why and the
  verification, not just the file list.

### Codegen constraints every library author hits

These are real constraints in the current emitter. Design around them; do not
fight them inside a library PR.

1. **Typed declarations use `:=`**, not `=`: `x :: Int64 := 0;`. Reassignment is
   plain `x = ...;`.
2. **Function definitions end with `}`**, no trailing semicolon.
   **`through ... -> loop/mode { ... };` ends with `;`.** **`while` and `if`
   blocks do not take a trailing `;`.**
3. **`out` is a builtin.** Never use it as a variable, parameter, or local
   name. The same applies to other reserved keywords.
4. **Function parameters are not reassignable.** Copy to a local first:
   `x :: Float64 := fx; if x < 0.0 { x = 0.0; }`.
5. **A native array parameter is a bare pointer.** Inside a native-variant
   function you cannot call `.size()` on it, and you cannot pass it to another
   function that takes an array parameter. Pass `n` explicitly, or take a boxed
   `Array` in a convenience entry point and copy into typed locals.
6. **Prefer a struct with data + length.** A `Series { data :: Array<Float64>,
   n :: Int64 }` keeps the pointer and length together and lets wrappers call
   `k_*` kernels with unboxed arrays. This is the safest zero-copy pattern.
7. **Same-named functions with different signatures collide** at C name
   mangling. Use distinct names (`nearest` vs `nearest2d`).
8. **Native struct construction from raw array parameters fails.** Build the
   struct through a boxed helper or from typed locals.
9. **Boxed `Array` parameters are the convenience escape hatch.** They can hold
   any element type, but entry points that take them copy once. Document the
   copy and offer a direct constructor for zero-copy use.
10. **Recursion with array parameters should be iterative or explicit-stack.**
    Native-to-native array calls are rejected; an explicit stack of indices or a
    single inlined loop avoids the gate.
11. **Use the `vcore` string builder for O(n) string work.** `sb_create`,
    `sb_append`, `sb_append_char`, `sb_build`. Plain `s = s + x` in a loop is
    O(n²).
12. **File I/O is `vfs.read` / `vfs.write` / `vfs.exists` / `vfs.remove`.**
    Desktop only; do not put `vfs` into an AVR-target library.

---

## The scoring test

| Question | If no, then... |
| --- | --- |
| Does it have a natural per-call boundary? | Region model doesn't help |
| Does it do arithmetic on `Int64` / `Float64`? | Native boxing buys you nothing |
| Does the caller benefit from a peak bound? | The language's pitch is irrelevant |

Three yeses → strong library. Two yeses → usable but not a demo. One yes →
don't. Every remaining entry is scored against this test.

---

## What ships today

### Built-in native modules

These are pulled in with `use native`, not `use external`.

| Module | Contents | Notes |
| --- | --- | --- |
| `vcore` | Time, platform, process info, string builder, `chr`, JSON helpers | `sb_append_char` added for byte parsers |
| `vmath` | Scalar math, PCG32 RNG, `_f64` unboxed variants | Trig, exp/log, rounding, min/max/clamp |
| `vmem` | Arena checkpoint/rewind, peak/live inspection, stream introspection | Region policies are in the runtime |
| `vfs` | File read/write, lines, bytes, walk, glob, path ops | Desktop only |
| `vimage` | PNG/JPEG decode/encode, resize, crop, grayscale | Vendored stb; header-only |
| `vconv` | Forward conv2d, activations, pixel shuffle, weight bundles | CPU; backward pass is a follow-up |
| `arduino` | AVR-only serial/GPIO/ADC/I2C/SPI/EEPROM/watchdog runtime | 110 bindings on the Uno R3 |

### External libraries (`vyne/modules/external/`)

| Library | State | Manual | One-line summary |
| --- | --- | --- | --- |
| `vlin` | **Done** | `vlin/README.md` | Dense matrix/vector algebra over `Array<Float64>`. |
| `vml` | **Done** | `vml/README.md` | Dense neural-network primitives on `vlin`. |
| `vfft` | **Done** | `vfft/README.md` | Radix-2 Cooley-Tukey FFT with plan caching. |
| `vrand` | **Done** | `vrand/README.md` | Continuous/discrete distributions and sampling. |
| `vjson` | **Done** | `vjson/README.md` | Recursive-descent JSON parser and serializer. |
| `vbio` | **Done** | `vbio/README.md` | Nucleotide/protein sequence primitives. |
| `vstat` | **Done** | `vstat/README.md` | Descriptive stats, correlation, regression, histograms, KDE, Welford. |
| `vtermplot` | **Done** | `vtermplot/README.md` | Terminal line/scatter/bar/histogram/heatmap/sparkline plots. |
| `vcomplex` | **Done** | `vcomplex/README.md` | Scalar/array complex arithmetic, functions, polar helpers. |
| `vinterp` | **Done** | `vinterp/README.md` | 1-D nearest/linear/Lagrange/spline and 2-D nearest/bilinear/bicubic. |
| `vcsv` | **Done** | `vcsv/README.md` | RFC 4180 CSV/TSV parser, writer, typed columns. |
| `vsignal` | **Done** | `vsignal/README.md` | Windows, FIR/IIR, resampling, spectral helpers, DSP kernels. |
| `vcontrol` | **Done** | `vcontrol/README.md` | PID, complementary filters, Kalman1D/2, state-space helpers. |
| `voptim` | **Done** | `voptim/README.md` | SGD, momentum/nesterov, Adam/AdamW, RMSProp, Adagrad, schedules. |
| `vtensor` | **Done** | `vtensor/README.md` | Region-local device tensor metadata over @device handles. |
| `vnn` | **Done** | `vnn/README.md` | Explicit @device_fn neural-network blocks over vtensor. |
| `vsparse` | **Done** | `vsparse/README.md` | CSR, SpMV, residual, Frobenius, conjugate gradient. |
| `vdecomp` | **Done** | `vdecomp/README.md` | LU, QR/least squares, Cholesky, Jacobi eigen, singular values. |
| `vi2c`, `vspi` | **Done** | header comments | Typed MCU I2C/SPI wrappers over the Arduino bindings. |
| `vimage` | **Done** | `vimage/README.md` | PNG/JPEG/BMP/TGA decode and encode over vendored stb. |
| `vconv` | **Done** | `vconv/README.md` | Forward conv2d, activations, pixel shuffle, weight bundles. |
| `vchem` | **Partial** | `vchem/` | `vchem_common` (elements/constants) + `vgraph` (CSR, BFS, DFS, Dijkstra). |
| `vcolors` | **Done** | header comments | ANSI color helpers. |
| `vconvert` | **Done** | header comments | Unit conversions (temperature, distance). |
| `vstring` | **Done** | header comments | Legacy string helpers. |
| `vplot` | **Toy** | — | Single-file bar chart. Superseded by `vtermplot`; do not extend. |

`vchem`'s full pipeline (`vmol`, `vsmiles`, `vdesc`, `vfp`, `vforce`, `vconf`,
`vdock`) is tracked in `todo_drug_discovery.md`.

---

## Condensed manuals of the shipped libraries

### `vlin`

Row-major flat `Matrix` plus a 2-D `Vector`. Split into `Kernels` (`k_*` loops
over typed arrays, native-ABI eligible) and `Ops`/`Reductions`/`Constructors`
(matrix-level wrappers). Two calling conventions: `vlin.multiply(a, b)` and
`a.transposed()`. Weight initializers, in-place variants, activations,
elementwise ops, reductions, and `sgd_update_inplace`.

**Load-bearing detail:** the `data :: Array<Float64>` annotation is what
unboxes the field into `VyneArray_f64` at every read. Changing it to plain
`Array` compiles but makes element access much slower.

### `vml`

A thin composition layer over `vlin`: `Dense`, `Sequential`, `SGD`, forward
pass, activation derivatives, one SGD step. Backward pass is manual. Native
`vml` bindings also carry Adam-family optimizers; `voptim` now provides the
first-class optimizer surface (SGD, momentum, Adam/AdamW, RMSProp, Adagrad,
schedules) over flat typed buffers.

### `vfft`

Iterative radix-2 FFT, complex input, power-of-two lengths. Plan caching for
twiddles and bit reversal. `forward`, `inverse`, `next_pow2`, `log2_exact`,
`pow2`, `make_plan`, `ensure_plan`, `fft_kernel`, `bit_reverse`.

**Plan-cache hazard:** the plan is arena-allocated, but the invalidation
sentinel is a plain global. Warm the plan at top-level scope before a region
loop; a plan first built inside a rewound region dangles. `rfft`/`irfft`,
batched transforms, and region-aware invalidation are still open.

### `vrand`

Continuous and discrete distributions, sampling utilities, and RNG access on top
of PCG32. Seedable; deterministic. The process-global RNG stream is still a
limitation for independent simulations (`todo_2.md` has the state-carrying RNG
direction).

### `vjson`

Recursive-descent parser and string-builder serializer. All seven JSON value
types, nested containers, escapes, surrogate pairs, `\uXXXX` decoding.
`parse`, `serialize`, `validate`, `depth`, type predicates. **Lexer hazard:**
literal `{` in a source string must be escaped as `\{`; `}` only when it closes
an interpolation. Key order is hash-order, not insertion-order.

### `vbio`

Nucleotide/protein sequence primitives: complement, reverse-complement,
transcription, codon translation, reverse translation, GC content, melting
temperature, Hamming distance, codon composition. Sequences are `String`s; the
`DNA`/`RNA`/`Protein` interfaces carry a label. Codon bias defaults to E. coli
K-12.

### `vstat`

`Series { data :: Array<Float64>, n :: Int64 }` carries a typed array and its
length for zero-copy kernel calls. Surface: sum/mean/min/max/range, variance
and stddev (population/sample), median/quantile/percentile/IQR/MAD/mode,
weighted mean, summary; z-score, min-max normalize, absolute, cumulative sum,
difference, average rank; covariance, Pearson, Spearman, autocorrelation,
cross-correlation, R²; linear/predict/residuals, multiple linear, ridge,
predict-row; histogram/histogram_range, density, CDF, empirical CDF, quantiles,
Gaussian KDE; Welford and bivariate Welford state. `from_array` copies a boxed
array; the direct constructor is zero-copy.

### `vtermplot`

Matplotlib-flavoured terminal plotting. A `Plot` owns a flat width×height grid
of marker codes; `SeriesData` carries typed x/y arrays. Surface: line, scatter,
bars, histogram, heatmap, sparkline, axes/labels, Unicode/ASCII themes, optional
ANSI color, and `save` via `vfs`. One-call helpers: `plot_line`, `plot_scatter`,
`plot_bars`, `plot_hist`, `plot_heatmap`, `plot_line_series`,
`bars_series_plot`. Sparklines use the `vcore` string builder.

### `vcomplex`

Scalar `Complex { re, im }`, split `ComplexArray { re[], im[], n }`, and
`Polar { r, theta }`. Scalar ops, exp/log/sqrt/pow/sin/cos/tan, polar
conversion/rotation, and element-wise array ops (`a_add`, `a_sub`, `a_mul`,
`a_scale`, `a_conj`, `a_abs`, `a_abs_sq`, `a_arg`, `a_sum`, `a_dot`,
`a_from_polar`). Functions use the principal branch.

### `vinterp`

1-D over a sorted `Curve { xs, ys, n }`: nearest, linear with linear
extrapolation, clamped linear, Lagrange polynomial, natural cubic spline,
linear/spline resampling. 2-D over a row-major `Grid { data, rows, cols }`:
nearest, bilinear, Catmull-Rom bicubic, both at fractional grid indices and at
physical coordinates. `curve`/`grid` boxed constructors copy and validate; the
direct constructors are zero-copy. `Curve.xs` must be strictly ascending.

### `vsignal`

DSP library over typed arrays: rectangular/Hann/Hamming/Blackman/Bartlett
windows, moving-average/exponential/FIR/biquad filters, RBJ-style IIR design,
resampling, and spectral helpers on `vfft`. Public surface uses interfaces, so
the MCU linkage waits on the AVR struct runtime even though the kernels are
plain array loops. Tests: `examples/libs/vsignal_test.vy`; benchmark:
`examples/libs/vsignal_bench.vy`.

### `vcontrol`

Control-systems library: PID (anti-windup, derivative filtering, bumpless
transfer), complementary filter, Kalman1D, constant-velocity Kalman2, and
two-state state-space helpers. Depends on `vsignal`/`vlin` where useful.
Desktop-complete; same MCU interface blocker as `vsignal`. Tests:
`examples/libs/vcontrol_test.vy`; benchmark: `examples/libs/vcontrol_bench.vy`.

### `vcsv`

RFC 4180-style CSV/TSV. `Table { headers :: Array, cells :: Array, n_rows,
n_cols }` with flat row-major cells. Quoted fields, doubled quotes, embedded
delimiters/newlines, CRLF, optional headers, custom single-byte delimiter.
Access: cell/header/row/column, typed `column_f64`/`column_i64`, `shape`,
`select_columns`, `column_names`. Serializer quotes only when needed and uses
the `vcore` string builder; file I/O via `vfs`. The parser scans with
`String.byte_at` and `vcore.sb_append_char`.

---

## The remaining top-10 library

`voptim`, `vsparse`, and `vdecomp` have shipped, so the queue is now just
**`vnum`**. The earlier briefs below are retained for context; the first three
are marked with their shipped status.

### L1.6 — `voptim` — Optimizers for training loops [SHIPPED]

**Status (2026-11-10):** shipped under `vyne/modules/external/voptim` with
`Types.vy`, `Kernels.vy`, `Schedules.vy`, `Ops.vy`, facade, README, test, and
benchmark. The brief below is the original design; the implemented surface
covers the update rules and schedules.

**Why next.** It is the shortest path to a measurable ML improvement: the
classifier and `vml` training loops get Adam/AdamW and schedules, and per-step
updates can be proven allocation-free.

**What lands.**

- Update rules: `sgd`, `sgd_momentum`, `nesterov`, `rmsprop`, `adagrad`,
  `adam`, `adamw`.
- Learning-rate schedules: `constant`, `step`, `exponential`, `cosine`,
  `warmup_cosine`.
- In-place kernels over `Array<Float64>` (`k_sgd_update`, `k_adam_update`, …)
  and matrix wrappers over `vlin.Types.Matrix`.
- State interfaces: `SGDState`, `MomentumState`, `RMSPropState`, `AdamState`
  (`m`, `v`, step count), with session-scoped allocation.
- A small `Optimizer` protocol so a training loop can keep one state value and
  call `step(params, grads)`.

**Design constraints.**

- Updates happen in place; `state` and parameter buffers are reused every step.
- Per-parameter state lives in a session region; per-step code allocates
  nothing.
- The kernels take data + length (`Series`/matrix fields); wrappers build the
  state once.

**Depends on.** `vlin`.

**Effort.** 1 week.

**Validation.** Train XOR and the `ml_seq` classifier; measure epochs to target
versus plain SGD; assert zero arena growth per step; benchmark 1M-parameter
updates.

**File shape.** `Types.vy`, `Kernels.vy`, `Schedules.vy`, `Ops.vy`,
`Optimizers.vy`, `voptim.vy`, `README.md`.

---

### L1.7 — `vsignal` — Signal processing [SHIPPED #121]

**Status (2026-11-10):** shipped under `vyne/modules/external/vsignal` with
`Types`, `Constructors`, `Kernels`, `Iir`, `Fir`, `Windows`, `Resample`,
`Spectral`, facade, README, test, and benchmark. The design below is retained
as the original brief; `vcontrol` ships alongside it. The remaining DSP work is
`vfft`'s `rfft` path and MCU linkage, not the library skeleton.

**Why next.** It builds directly on `vfft` and turns the DSP story into a real
library: windows, convolution, filters, spectra, and allocation-free block
processing. It absorbs most of the old `vfilter` plan.

**What lands.**

- Windows: rectangular, Hann, Hamming, Blackman, Bartlett.
- Convolution: direct, FIR, moving average, exponential moving average.
- Filters: first-order low/high, biquad/SOS with RBJ Audio EQ Cookbook design,
  cascade apply, magnitude and group-delay analysis. A `vfilter` submodule can
  host the real-time SOS layer if it wants its own name.
- Spectral: power spectrum, one-sided magnitude, spectrogram over `vfft`
  (with the plan warmed outside any region), and a real-input helper.
- Signal stats: RMS, peak, peak-to-peak, zero crossings, energy.
- Resampling: decimate, linear resample, frame/window extraction.

**Design constraints.**

- Typed arrays throughout; filter state is a small struct reused across blocks.
- Spectrogram/window allocations belong in a caller region; document the plan
  caching hazard.
- Do not call `vfft.forward` with a raw native array parameter; pass typed
  containers from a wrapper.

**Depends on.** `vfft`, `vmath`; optionally `vcomplex`.

**Effort.** 2 weeks.

**Validation.** Sine FFT peak at the expected bin; filter magnitude response
against the cookbook; convolution against a direct reference; spectrogram on a
chirp; an allocation-free block callback.

**File shape.** `Types.vy`, `Windows.vy`, `Kernels.vy`, `Convolution.vy`,
`Filters.vy`, `Spectral.vy`, `Stats.vy`, `vsignal.vy`, `README.md`.

---

### L1.8 — `vsparse` — Sparse matrices and iterative solvers [SHIPPED]

**Status (2026-11-10):** shipped under
`vyne/modules/external/vsparse` with `Types.vy`, `Kernels.vy`,
`Constructors.vy`, `Ops.vy`, facade, README, test, and benchmark. CG lives in
`Ops.vy` rather than a separate `Solvers.vy`. CSC/SpMM/preconditioners remain
follow-ups.

**Why next.** Sparse matvec is a natural Vyne kernel: typed index/value arrays,
a per-call peak bound, and no boxing. It unlocks force-field Hessians, graph
Laplacians, and PDE stencils.

**What lands.**

- CSR and CSC types over `Array<Int64>` indices and `Array<Float64>` values.
- Constructors: from COO, from dense, identity, transpose, diagonal.
- Operations: SpMV, SpMM, scalar multiply, add, Frobenius norm, residual.
- Iterative solvers: Jacobi, Gauss–Seidel, conjugate gradient (SPD).
- Graph helpers: Laplacian/adjacency construction from edge lists.

**Design constraints.**

- Kernels are leaves over typed arrays; wrappers validate shape and allocate.
- `vsparse` is a general-purpose library; keep it at the top level, not under
  `vchem/`, even though chemistry is the first consumer.
- Solver iterations reuse workspace arrays; document the region/scratch
  pattern.

**Depends on.** `vlin` for dense fallback/RHS.

**Effort.** 1 week.

**Validation.** SpMV/SpMM against a dense oracle on random sparse matrices;
conjugate gradient against a dense SPD solve; graph Laplacian sanity checks.

**File shape.** `Types.vy`, `Constructors.vy`, `Kernels.vy`, `Ops.vy`,
`Solvers.vy`, `vsparse.vy`, `README.md`.

---

### L1.9 — `vdecomp` — Matrix decompositions and direct solvers [SHIPPED]

**Status (2026-11-10):** shipped under
`vyne/modules/external/vdecomp` with `Types.vy`, `Util.vy`, `LU.vy`,
`QR.vy`, `Cholesky.vy`, `Eigen.vy`, `Ops.vy`, facade, README, test, and
benchmark. SVD is exposed as `singular_values` via A^T A; one-sided Jacobi is
still a follow-up.

**Why next.** It is the LAPACK-shaped layer the scientific stack lacks: LU, QR,
Cholesky, eigen, SVD, and the solves built on them. It is also the natural
backend for `vstat`'s regressions, `vconf`, and force-field minimization.

**What lands.**

- LU with partial pivoting: `lu`, `solve`, `inverse`, `determinant`.
- QR by Householder: `qr`, `least_squares`, orthogonal residual checks.
- Cholesky for SPD matrices; triangular solves.
- Symmetric eigen via Jacobi rotations; power iteration for the dominant
  eigenpair.
- SVD via the symmetric eigen of `AᵀA` (documented conditioning limits), or a
  dedicated one-sided Jacobi if it proves cleaner.
- Small helpers: matrix norm, condition estimate, pivoting diagnostics.

**Design constraints.**

- Pure Vyne over `vlin.Types.Matrix`/flat `Array<Float64>`; no LAPACK dependency.
- Each factorization returns a result struct with the factors and pivot/status
  information. Factorizations that allocate workspace should document it.
- Inline the triangular/tridiagonal loops where the native-call gate would
  otherwise reject a helper, or use `Series`-style struct calls.
- Native `vblas`/`vlapack` bindings remain a separate future item; `vdecomp` is
  the portable pure-Vyne layer.

**Depends on.** `vlin`.

**Effort.** 2–3 weeks.

**Validation.** Residual norms and orthogonality on known matrices; LU/QR/
Cholesky/SVD compared with NumPy/LAPACK on random matrices; least squares on a
rank-deficient system; eigenpairs checked by `A v ≈ λ v`.

**File shape.** `Types.vy`, `LU.vy`, `QR.vy`, `Cholesky.vy`, `Eigen.vy`,
`Solve.vy`, `vdecomp.vy`, `README.md`.

---

### L1.10 — `vnum` — Numerical methods

**Why last of the top-10.** It composes everything above: roots, quadrature,
ODE integration, interpolation, and fitting. It is also the library most
sensitive to the callback design.

**What lands.**

- Root finding: bisection, Newton, secant, Brent.
- Integration: trapezoidal, Simpson, Gauss–Legendre (nodes/weights), adaptive
  quadrature.
- ODE: Euler, midpoint, RK4, adaptive RK45.
- Interpolation/fitting wrappers: reuse `vinterp` for 1-D interpolation;
  polynomial least-squares fit; linear/ridge from `vstat` where appropriate.
- Result structs carrying iterations, residual, and convergence status.

**Callback protocol (load-bearing).**

Function values are broken in the compiled backend, so **do not pass a function
as a value**. Instead, define an interface with the method the solver needs and
use generic method dispatch:

```vyne
interface Fn {
    eval(x :: Float64) -> Float64;
}

interface Dfn {
    eval(x :: Float64) -> Float64;
    deriv(x :: Float64) -> Float64;
}

fn bisection<F>(f :: F, lo :: Float64, hi :: Float64, tol :: Float64) -> Float64 {
    # f.eval(x) is monomorphized for the concrete interface.
}

fn newton<F>(f :: F, x0 :: Float64, tol :: Float64) -> Float64 {
    # f.eval(x) and f.deriv(x)
}
```

For vector ODEs, use an interface with `rhs(t, y :: Array<Float64>) ->
Array<Float64>` and copy into typed locals inside the wrapper; avoid
native-to-native array calls.

**Depends on.** `vinterp` (optional), `vlin`.

**Effort.** 2 weeks.

**Validation.** Known roots (`sqrt(2)`, a cubic), `∫₀¹ x²` by each method,
convergence orders, ODE against analytic solutions, and a callback example in
the README.

**File shape.** `Types.vy`, `Callbacks.vy`, `Roots.vy`, `Integration.vy`,
`ODE.vy`, `Fitting.vy`, `vnum.vy`, `README.md`.

---

## Tier 2 — real, but not on the critical path

Ordered by how often the next project will need them.

| Library | What it does | Effort | Notes |
| --- | --- | --- | --- |
| `vunits` | Length/mass/temperature/pressure/time conversions; dimensional helpers | 2 days | Pairs with scientific input parsing; `vconvert` is the legacy toy |
| `vgeom` | 2-D/3-D vectors, intersections, point-in-polygon, hulls, AABB | 1 week | Pairs with graphics; overlaps `vlin.Vector` |
| `vtime` | ISO 8601 parse/format, arithmetic, comparisons | 3 days | Desktop, `vcore.now` |
| `vduration` | `"1h30m"`, `"500ms"` parse/format/arithmetic | 1 day | Use with `vtime` |
| `vcalendar` | Day-of-week, week-number, leap years, month names | 2 days | Use with `vtime` |
| `vuuid` | UUID v4/v7 generation | 1 day | `vrand` for randomness |
| `vconfig` | Layered INI/JSON/env config lookup | 4 days | Builds on `vjson`, `vini` |
| `vini` | INI parser/serializer | 2 days | Step toward `vtoml` |
| `vtoml` | TOML parser | 1 week | After `vini` and `vcsv` |
| `vlog` | Structured logging, levels, JSON output | 3 days | Builds on `vjson` |
| `vsort` | Merge/radix/counting/bucket sorts over typed arrays | 4 days | Kernel-native |
| `vsearch` | Binary/lower/upper/exponential/interpolation search | 2 days | Kernel-native |
| `vbitset` | Fixed-size bitset, popcount, and/or/xor | 2 days | Uses `Array<Int64>` |
| `vquat` | Quaternion arithmetic, rotations | 3 days | Pairs with `vcomplex` |
| `vfilter` | Real-time SOS/IIR layer (if `vsignal` does not absorb it) | 2 weeks | Optional split from `vsignal` |

`vfilter` is deliberately duplicated here and inside `vsignal`: ship the signal
library first; if the real-time SOS API wants its own name and docs, promote it
to a separate `vfilter` module rather than growing `vsignal` indefinitely.

---

## Tier 3 — longer projects or ecosystem polish

| Library | What it does | Effort |
| --- | --- | --- |
| `vbigint` | Arbitrary-precision integers over `Array<Int64>` | 1.5 weeks |
| `vpath` | Grid A*, Dijkstra, BFS, DFS, flow fields | 1 week |
| `vparse` | Expression-language parser (CSV/INI/TOML are separate) | 3–5 days |
| `vregex` | NFA regex engine | 1.5 weeks |
| `vmarkdown` | Markdown subset to HTML | 1 week |
| `vbase64` / `vhex` | Encoding helpers | 1 day each |
| `vsemver` | Semantic-version parse/compare/ranges | 2 days |
| `vheap` / `vtreap` / `vtrie` / `vunion` / `vbloom` | Data structures | 1–3 days each |
| `vgraph` | General graph algorithms (separate from `vchem/vgraph`) | 1 week |
| `vimage` | PPM/PNG loading and pixel access | 1 week |
| `vaudio` | WAV read/write, sample access, resampling | 4 days |
| `vcolor` | RGB/HSL/HSV/Lab conversions | 2 days |
| `vtest` / `vbench` | Test and benchmark harnesses | 2–3 days each |
| `vprocess` | Subprocess spawn/capture/wait | 3 days |

### Native bindings, kept separate

- `vblas` — OpenBLAS/MKL wrappers (`dgemm`, `daxpy`, `dscal`, `dgemv`, …).
  Driver `-lopenblas` behind `--blas`; the existing BLAS dispatch already covers
  `vlin.multiply`.
- `vlapack` — LAPACK workspace-query bindings, for the cases `vdecomp` should
  not implement in pure Vyne.

These are interop libraries, not pure-Vyne scientific libraries. Keep them out
of `vdecomp`/`vsparse` so those stay portable.

---

## Tier 4 — defer until Papers 2 and 3

Nothing here is fully doable with today's feature set. The compiler now has
`@differentiable` / `grad`, but the tensor/shape/linear-type layer these
libraries need does not exist.

- `vtensor` — autodiff tensors. Needs linear types and shape types.
- `vode` / `vpde` — differential-equation solvers at scale. Needs shape types.
- `vsim` — 3-D physics at scale. Needs shape and linear types.
- `v3d` — `Vec3`/`Vec4`/`Mat4` as distinct shape types.
- `vautodiff` — reverse-mode AD over `vtensor`; needs the tensor and effect
  work. The compiler-level `@differentiable` / `grad` surface exists, but a
  tensor library with shape/linear-type guarantees does not.

---

## Never

- `vcrypto` — bind libsodium; do not roll your own.
- `vnet` — sockets need syscalls; bind the C API.
- `vxml` — no.
- `vgui` / `vweb` — state-and-callback UI languages, not arithmetic.

---

## Ordering

```
Done:  vstat, vtermplot, vcomplex, vinterp, vcsv,
       vsignal, vcontrol, vimage, vconv,
       voptim, vsparse, vdecomp
  |
vnum        (2 weeks)    - roots, quadrature, ODE, fitting
  |
Tier 2 in the order the next project needs
  |
Tier 3 and native bindings, one at a time
```

Reasoning: `voptim`, `vsparse`, and `vdecomp` now complete the core numerical
stack: training, sparse systems, and dense factorizations. `vnum` is the last
top-10 item and the natural next step because fitting and ODE work reuse the
solvers above.

---
## What each library contributes to the story

| Library | State | Audience | Claim |
| --- | --- | --- | --- |
| `vlin` | Done | Numeric | "The numerical core, with native-ABI kernels" |
| `vml` | Done | ML | "Training loops work, and the numbers are real" |
| `vfft` | Done | Numeric/scientific | "Real numeric code, no deps, competitive speed" |
| `vrand` | Done | Everything | "We have distributions, not just a PRNG" |
| `vjson` | Done | Tooling | "Recursive descent without a GC" |
| `vbio` | Done | Bioinformatics | "Credible non-numeric reach" |
| `vstat` | Done | Data/analytics | "The boring statistics you actually need" |
| `vtermplot` | Done | Scientific/data | "Matplotlib in the terminal" |
| `vcomplex` | Done | Numeric/signals | "Complex math over split arrays" |
| `vinterp` | Done | Scientific | "Sampled data becomes a function" |
| `vcsv` | Done | Data | "Real data gets in without a scripting layer" |
| `voptim` | Done | ML | "Training loops get faster, and allocations stay flat" |
| `vsignal` | Done | DSP/audio | "Allocation-free block processing, provably" |
| `vcontrol` | Done | Robotics/control | "PID, Kalman, and state-space without a runtime" |
| `vsparse` | Done | Scientific | "Sparse systems with a per-call peak bound" |
| `vdecomp` | Done | Scientific | "LAPACK-shaped solves in pure Vyne" |
| `vnum` | Next | Scientific | "Roots, integrals, and ODEs belong here" |
| `vtensor` | Tier 4 | ML | "Autodiff as a first-class primitive" |

---

## Don't do this

- **Don't start a library before its dependencies.** The dependency graph is a
  DAG; walking it out of order creates rework.
- **Don't break the one-library-per-PR workflow.** Each library should ship
  with a README, a test, and a benchmark.
- **Don't use function values in compiled code.** They do not lower correctly;
  use the generic interface-method callback pattern in `vnum` and
  `vsignal`.
- **Don't name two functions the same with different signatures.** C name
  mangling collides; use `nearest` vs `nearest2d`.
- **Don't use `out` as a variable or parameter.** It is a builtin.
- **Don't reassign a function parameter.** Copy to a local first.
- **Don't call `.size()` on a native array parameter**, and do not pass a native
  array parameter to another array-parameter function. Use a `Series`-style
  struct or a boxed convenience entry point.
- **Don't build `vtensor`/autodiff before shape and linear types.**
- **Don't ship `vblas`/`vlapack` before the native array ABI and region-backed
  buffers are stable** — they are stable now, so this is about scheduling, not
  a blocker.
- **Don't try to match FFTW's last 30%.** 0.7–0.9× FFTW is the honest number.
- **Don't add every distribution to `vrand`.** Gaussian, exponential, and
  categorical cover most uses.
- **Don't write two libraries at once.** Serialize; each teaches something about
  the language.
- **Don't chase key-order preservation in `vjson`.** It is valid JSON without it.
- **Don't extend the legacy `vplot.vy`.** Use `vtermplot`.
- **Don't roll your own crypto.** Bind libsodium or don't ship it.

---

## What "done" looks like

When `examples/benchmark/` and `examples/test/` can demonstrate:

- A matmul at native-C speed (`vlin`, checks on).
- An FFT at 0.8× FFTW (`vfft`; `rfft` still open).
- A 4-band EQ with an allocation-free callback (`vsignal`/`vfilter`).
- A classifier trained with Adam in fewer epochs (`voptim`).
- A JSON payload round-trip (`vjson`).
- A CSV → stats → interpolation → complex → terminal-plot pipeline
  (`vcsv` + `vstat` + `vinterp` + `vcomplex` + `vtermplot`; the integration test
  already does this).
- A sparse system solved by conjugate gradient (`vsparse`).
- LU/QR/Cholesky/eigen solves with residual checks (`vdecomp`).
- Roots, quadrature, and an ODE integration with convergence checks (`vnum`).
- All of the above with provable peak scratch bounds.

...the library layer has both a numeric story and a memory-model story, backed
by measurements a reviewer can read. That is what turns the compiler's
guarantees from _interesting_ into _useful_.

---

## Cross-references and PR log

- Top-10 batch:
  - #104 `vstat`
  - #105 `vtermplot`
  - #107 `vcomplex`
  - #108 `vinterp`
  - #109 `vcsv` + `examples/test/libraries_integration_test.vy`
  - #112 `vimage` + `vconv` (ESPCN slice)
  - #121 `vsignal` + `vcontrol`
  - this PR `voptim`, `vsparse`, `vdecomp`
- Tests: `examples/libs/{vsignal,vcontrol,voptim,vsparse,vdecomp}_{test,bench}.vy`,
  `examples/test/{vstat,vtermplot,vcomplex,vinterp,vcsv}_test.vy`,
  `examples/test/libraries_integration_test.vy`.
- Benchmarks: `examples/benchmark/{vstat,vtermplot,vcomplex,vinterp,vcsv}_bench.vy`,
  `examples/benchmark/{vsignal,vcontrol}_bench.vy` (where present).
- Paper/feature roadmaps: `todo.md`, `todo_2.md`, `todo_optimization.md`,
  `todo_rulesets.md`, `todo_drug_discovery.md`, and `milestone.md`.
