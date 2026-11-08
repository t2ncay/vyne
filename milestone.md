# Vyne — Scientific & Numerical Programming Roadmap

> Derived from a full scan of the compiler, runtime, standard libraries, examples,
> and the six planning documents in `docs/todos/` (`todo.md`, `todo_2.md`,
> `todo_optimization.md`, `todo_libraries.md`, `todo_rulesets.md`,
> `todo_drug_discovery.md`). This document defines what Vyne can realistically
> achieve as a **scientific and numerical programming language**, and orders that
> work into capability milestones. No code is changed by this document.

---

## 0. Executive summary

Vyne is a C-flavored, statically-analyzable numerical language with a **single
AST and three backends**: a tree-walking interpreter, a C transpiler for the
host, and an AVR transpiler for the Arduino Uno R3. Its differentiating claim is
not syntax — it is **deterministic, region-scoped memory with no GC**, combined
with a **native (unboxed) ABI for numerical kernels** and a **kernel/wrapper
library pattern** that keeps arithmetic off the boxed value path.

In the scientific / numerical field, the achievable trajectory is:

1. **Parity with hand-written C** on dense numerical kernels, with bounds and
   safety checks still enabled (M1).
2. **A credible numerical standard library** — linear algebra, statistics,
   root-finding, ODE/integration, sparse solvers, DSP — each validated against a
   reference implementation (M2–M3).
3. **Compile-time guarantees** that mainstream numerical languages cannot give:
   shape correctness, in-bounds access, allocation-free kernels, in-place
   mutation, and provable peak memory (M4).
4. **A flagship domain pipeline** that demonstrates the whole stack end-to-end
   on a real scientific workload — computational chemistry and virtual screening
   is already planned and partially started (M5).
5. **Optional accelerator interop** (device arenas, `@device` regions, cuBLAS)
   once the CPU path is proven (M6).

The honest constraint from the planning docs: the compiler is effectively
**frozen until Paper 1 (regions + scratch + compile-time peak bounds) is
submitted**. Milestone M0 closes that gate; M1 onward is the numerical build-out.

---

## 1. What Vyne is — language semantics and goal

### 1.1 Main goal

> A numerical programming language where **lifetime is a lexical fact**, memory
> is deterministic (arena, no GC), and the same source runs on a workstation and
> an 8-bit microcontroller. Performance comes from a native ABI and a
> kernel/wrapper library pattern, not from a garbage collector or a JIT.

Design pillars observed in the codebase:

| Pillar | Mechanism | Where it lives |
| --- | --- | --- |
| One AST, three backends | Interpreter, C transpiler, AVR transpiler | `vyne/compiler/`, `vyne/runtime/`, `arduino/` |
| Deterministic memory | Bump arena + checkpoints + commit arena | `runtime/detail/`, `codegen/regions.cpp` |
| Lexical lifetime | `region` blocks; `scratch` stack arrays | parser, `codegen/regions.cpp` |
| Unboxed numerics | Native ABI for primitives, typed arrays, structs | `codegen/native_dispatch.cpp`, `ctype.h` |
| Numerical libraries | Kernel (`k_*`) + wrapper split | `vyne/modules/external/*` |
| Diagnostics | Stable `VNE-###` codes, source-annotated | `runtime/diagnostics.h` |

### 1.2 Core semantics (as implemented)

**Values and types**

| Type | Notes |
| --- | --- |
| `Int64` | 64-bit signed; hex literals; **scientific notation literals supported** (`1.5e-3`) |
| `Float64` | IEEE-754 double (32-bit `double` on AVR) |
| `String` | UTF-8, immutable, interned |
| `Bool`, `Null` | — |
| `Array` | heterogeneous, boxed (`VyneValue`) |
| `Array<T>` | typed (`Array<Float64>`, `Array<Int64>`); native ABI, typed struct containers |
| `Map` | string keys |
| `Struct` / `interface` | typed fields + methods; native C struct when C-eligible |
| `Function`, `Module`, `Reference` | internal function values, module handles, `^^` refs |

**Bindings and declarations**

```vyne
x = 42;                  # inferred (dynamic binding by default)
y :: Int64 = 42;         # explicit
const PI :: Float64 = 3.141592653589793;
```

**Control flow**

- `if / else if / else`, `while`, `return`, `break`, `continue`, `defer`.
- `through i :: lo..hi -> mode { ... };` with modes `loop`, `collect`,
  `filter`, `every`, `unique`.
- `..` inclusive range, `s[a:b]` slice, ternary `?:`, null-coalesce `??` / `??=`,
  pipeline `|>`, membership `in`.

**Functions and abstraction**

- `fn name(params) -> T { ... }`, first-class functions.
- Generics `<T>` **monomorphized** per distinct type tuple.
- `interface` = struct layout + constructor + methods.
- `group` = namespace for functions/constants (no type).
- `enum` with implicit increments.
- `try / catch / finally / throw` — `longjmp` on compiled backends.

**Operators**

Arithmetic `+ - * / %`, floor division `//`, power `**`; comparison; logical
`&& || !`; bitwise `& | ^ ~ << >>`; reference `^^`; address-of `$`
(interpreter only); range/slice/ternary/coalesce/pipeline/membership as above.

**Regions and memory (the load-bearing feature)**

```vyne
region train_step {
    scratch grad :: Float64[64, 16];   # lowered to a C stack array
    ...
};                                     # checkpoint/rewind at braces
```

Implemented region policies (from `codegen/regions.cpp`):

| Policy | Status | Meaning |
| --- | --- | --- |
| default bump region | **Done** | checkpoint on entry, rewind on exit |
| `@pool<T, N>` | **Done** | fixed-slot allocator, O(1) release |
| `@speculative` + `commit_if` | **Done** | rollback/commit on predicate |
| `@bounded<N>` | **Done** | runtime-checked memory ceiling |
| `@zeroed` | **Done** | zero-fill every allocation |
| `@deterministic` | **Done** | `@zeroed` + non-determinism scan |
| `@ring<N>`, `@persistent`, `@device`, `@streamed` | **Planned** | see `todo_2.md` |

Escape analysis (`VNE-070`) rejects a region-local pointer escaping into a
shallower variable. `scratch` is codegen-only; the interpreter rejects it.

**Rulesets** — `ruleset { type_check = "strict", memory_limit = ..., warnings = ... }`
exists as a language-policy construct. **Per-file safety waivers**
(`ruleset { safety { scratch_bounds = off } }`) are specified in
`todo_rulesets.md` but **not yet implemented**.

**Modules / imports** — `use native <mod>`, `use external "<file>"`, `module`,
`deploy`. The linker builds a topological import order; one `CompileUnit` per
file.

### 1.3 Memory model

- **Main arena**: bump allocation, block-chained, `mmap`/`VirtualAlloc` on host,
  static 1 KB window on AVR.
- **Commit arena**: survives rewinds; `region.commit(x)` deep-clones into it.
- **Checkpoints**: `vmem.checkpoint()` / `vmem.rewind(h)`; fixed 16-deep on AVR.
- **Interning**: identifier/map keys are `uint32_t` IDs, never string compares.
- **Typed arrays**: `VyneArray_f64` / `VyneArray_i64` box in O(1) (pointer wrap);
  `A.data[i]` lowers to a direct load.

---

## 2. Baseline — what exists today (v0.4.0)

### 2.1 Shipping surface

**Built-in native modules:** `vcore`, `vmath`, `vmem`, `vfs` (desktop), and
`arduino` (AVR, 60+ bindings).

**External Vyne libraries:**

| Library | State | Notes |
| --- | --- | --- |
| `vlin` | **Shipped** | Dense matrix/vector: kernels, constructors, ops, reductions, activations, elementwise, `sgd_update_inplace` |
| `vml` | **Shipped** | Dense NN primitives on `vlin`; native optimizer bindings (Adam family) |
| `vfft` | **Shipped** | Complex radix-2 FFT, plan caching; power-of-two only; no `rfft` |
| `vrand` | **Shipped** | Continuous/discrete distributions + sampling |
| `vjson` | **Shipped** | Parse / serialize / validate |
| `vbio` | **Shipped** | Codons, complement, GC, melting temperature |
| `vchem` | **Partial** | `vchem_common` (Elements, Constants) + `vgraph` (CSR, BFS, DFS, Dijkstra) |
| `vcolors`, `vconvert`, `vplot`, `vstring` | Single-file | Utility |

### 2.2 Landed from the planning docs

- `todo.md` Phase 0 (compiler bugs) and Phase 1 (safety: scratch bounds,
  region depth, escape checks) — **done**.
- `todo_2.md` **F0 native array ABI** — **done**; struct ABI landed in v0.4.0.
- **F1 OpenBLAS dispatch** (`--blas`, `cblas_dgemm`) — done.
- **F15 bitwise operators**, **F16 scientific-notation literals** — done.
- Some **region policies** (`@pool`, `@speculative`, `@bounded`, `@zeroed`,
  `@deterministic`) — done.
- `todo_drug_discovery.md` compiler prerequisites **C0, C1, C3, C4, C5** —
  landed; **C2** substantially complete with residual non-flattenable struct ABI.

### 2.3 Known gaps (from scan + README limitations)

- Emitter output is temp-heavy; measured **~3.9% wall-clock gap** to hand-written
  C, and bounds checks cost **~45%** on the 1024² matmul when enabled.
- No `Float32` / `BFloat16` / complex / quaternion / units / shape types /
  range types / uniqueness / effect typing.
- No per-file safety rulesets.
- `vlin.relu_prime` / generic-array fallback / top-level native empty-array
  declaration are residual codegen gaps.
- Single-process RNG (no state-carrying generator).
- No integer-overflow checking; `int64()` saturates.
- AVR target is Uno R3 only; no Wire/SPI/SoftwareSerial; no `out()`.
- Root `tests/` tree referenced by `xmake test` is **absent from this working
  copy**; verification currently relies on `examples/`, `examples/safety/`, and
  `vyne/runtime/tests/`.

---

## 3. Capability gap analysis — scientific & numerical programming

The table scores Vyne against what numerical practitioners expect. "Source"
points to the planning document that owns the work.

| Capability area | Today | Needed for scientific/numerical use | Source |
| --- | --- | --- | --- |
| Dense linear algebra | `vlin` matmul/ops, BLAS dispatch | LAPACK-class factorizations, solves, eigen | `todo_libraries.md` L3.2 |
| Sparse linear algebra | none | CSR/CSC matvec, iterative solvers | L3.4 / vsparse |
| Statistics | none | descriptive, correlation, regression, streaming | L2.1 `vstat` |
| Numerical methods | none | roots, quadrature, ODE, interpolation | L2.3 `vnum` |
| DSP / signals | `vfft` complex only | real FFT, FIR/IIR, windows, spectrogram | L1.2 `vfilter`, `vsignal` |
| Randomness | `vrand` distributions | state-carrying RNG streams | README limitation |
| Precision types | `Float64` only | `Float32`, `BFloat16`, promotion rules | `todo_2.md` F13 |
| Performance | ~4% behind C, checks cost 45% | checks-free when provable, register-tight code | `todo_optimization.md` |
| Memory guarantees | region-scoped, peak measured | peak *provable*, shape-checked | `todo_2.md` F6/F8/F9 |
| Aliasing / mutation | copy/box semantics | linear/unique in-place tensors | F7 |
| Units / dimensionality | none | dimensional analysis | F10 |
| Tooling / tests | LSP, examples | reproducible harness, CI across backends | `todo.md`, `todo_rulesets.md` |
| Domain pipelines | bio (done), chem (partial) | ingestion → descriptors → physics → docking | `todo_drug_discovery.md` |
| Accelerators | none | device arenas + cuBLAS interop | F2–F5 |

**The core insight** from the docs, worth preserving: numerical credibility is
not won by adding syntax. It is won by (a) closing the performance gap to C,
(b) making the library surface real and validated, and (c) making the memory /
shape / effect guarantees *checkable* instead of *measured*.

---

## 4. Milestones

Each milestone has a goal, scope, deliverables, acceptance criteria,
dependencies, and an effort band. Effort is calendar effort for a small team,
sourced from the todo estimates where available.

---

### M0 — Baseline integrity and the Paper 1 gate

**Goal.** Freeze a trustworthy, reproducible numerical baseline before adding
features. Close the Paper 1 work so the compiler is no longer "frozen",
establish the test harness, and remove residual codegen correctness defects.

**Scope**
- Finish `todo.md` remaining items: §1 reframing (layered lifetime/storage
  story), §7 related-work paragraph, §5.7 typo; decide Experiment 3 / NumPy
  reference (recommendation: defer).
- Land `todo_optimization.md` **Tier 0** (`O0.1`, `O0.2`) and **Tier 1**
  peepholes (`O1.1`–`O1.6`).
- Close `todo_drug_discovery.md` C2 residuals: top-level native empty-array
  declaration, struct-typed native variant only if a profile demands it.
- Restore/replace the root `tests/` harness; wire `xmake test` to a present
  suite; add regression cases for struct-of-arrays + `String` + `Map`.
- Reconcile docs drift (README still lists `relu_prime`/codegen gaps that
  `todo_drug_discovery.md` marks landed; regions reference page predates
  `@bounded`/`@zeroed`/`@deterministic`).

**Deliverables**
- Reproducible benchmark suite (matmul 1024², ML classifier, recursion
  capability, safety suite 10/10).
- Emitter peephole pass merged; emitted C diff reviewed.
- Green CI on Linux/macOS/Windows + AVR host-side compile.

**Acceptance criteria**
- Matmul checksum, peak RSS, and wall clock reproduce within run variance.
- Safety suite: **10/10**.
- `xmake test` runs a present suite; no missing-target failures.
- Paper 1 is submittable (the documented gate for starting M1+).

**Dependencies.** None. **Effort:** 1–2 weeks (mostly the paper edits + peepholes;
the peepholes are ~1 day per `todo_optimization.md`).

---

### M1 — Numerical performance parity with C

**Goal.** Make Vyne-generated numerical kernels match hand-written C **with
checks on**, and eliminate the bounds-check tax statically for well-typed code.

**Scope**
- `todo_optimization.md` Tier 2 runtime items: 32-byte typed-array alignment
  (`O2.1`), `vyne_binop_slow` branch order (`O2.3`), `place_all` fast path.
- **Range-typed induction variables** (`F6`, `O3.1`): `k :: Int64<0..N>` with
  interval propagation, so `scratchFlatIndex` emits **no `VNE-072`** when an
  access is provably in bounds.
- Minimal **`Tensor` CType unification** (`O3.4`): one `CType::Kind::Tensor
  { element, shape }` covering typed arrays and scratch, so element-wise tensor
  arithmetic lowers to bare C loops instead of `vyne_binop`.
- Inline single-use temps (`O1.4`) and loop-bound inlining behind stable tests.

**Deliverables**
- Range-refinement pass + tests; `scratchFlatIndex` check elision.
- `Kind::Tensor` in `ctype.h` with lowering paths for element-wise ops.
- A before/after report: checks-on vs checks-off vs hand-written C.

**Acceptance criteria**
- 1024² matmul, **checks enabled**, within measurement noise of hand-written C
  (same checksum and peak RSS; wall clock inside run-to-run variance).
- Bounds checks statically eliminated for all indices derived from `through`
  bounds in the benchmark kernels.
- No regression in safety suite.

**Dependencies.** M0 (frozen baseline, test harness). **Effort:** 3–5 weeks
(range refinement 2–3 weeks; Tensor unification is the long pole, months for the
full version — ship the minimal element-wise form in M1).

---

### M2 — Core numerical & scientific standard library

**Goal.** Ship the "boring but essential" numerical surface, each library
validated against a reference implementation. Apply the repo's three-question
library test: natural per-call boundary, arithmetic on native types, caller
benefits from a peak bound.

**Scope (in priority order)**
1. **`voptim`** — SGD-momentum, RMSProp, Adam, AdamW, LR schedules. (Adam
   exists in native `vml`; promote to a first-class `vlin`/`voptim` surface.)
2. **`vstat`** — mean/median/mode, variance/stddev/MAD/IQR, Pearson/Spearman,
   covariance/autocorrelation, linear/ridge regression, Welford streaming.
3. **`vnum`** — bisection/Newton/secant/Brent, trapezoid/Simpson/Gauss-Legendre/
   adaptive quadrature, Euler/midpoint/RK4/RK45, linear/polynomial/cubic-spline
   interpolation, least-squares fitting. Monomorphized callbacks, not function
   pointers.
4. **`vsparse`** — CSR/CSC, SpMV/SpMM, Jacobi/Gauss-Seidel/conjugate-gradient.
5. **`vlapack`** — LU, QR, Cholesky, SVD, eigenvalues (workspace-query pattern is
   a natural region fit).
6. **`vsort` / `vsearch` / `vbitset`** — native-loop algorithms over typed arrays.
7. **`vcomplex` / `vquat`** — pairs/quads of `Float64`.

**Deliverables.** Each library: `.vy` leaves + facade + `README.md` + a test file
+ at least one example. Numerical validation scripts against NumPy/LAPACK where
possible.

**Acceptance criteria**
- Every library passes its reference-tolerance test (e.g. stats within the
  documented tolerances; linear algebra vs LAPACK; sparse vs dense oracle).
- All kernels are native-ABI eligible and allocate only at the wrapper boundary.
- At least one training loop uses `voptim` and improves epochs-to-target.

**Dependencies.** M1 (tight kernels). `vnum`/`vsparse`/`vlapack` depend on
`vlin`. **Effort:** 6–10 weeks (serialized; the docs warn against writing two
libraries at once).

---

### M3 — Signal processing, DSP & applied numerics

**Goal.** Turn the FFT/ML story into a real numerical-systems story: filters,
signals, interpolation, and unit-aware computation.

**Scope**
- **`vfft` completion** — `rfft`/`irfft`, batched transforms, **region-aware plan
  invalidation** (fix the dangling-plan hazard documented in `todo_libraries.md`).
- **`vfilter`** — SOS biquads (Butterworth/Chebyshev I/II/Linkwitz-Riley),
  RBJ cookbook design, Direct Form II Transposed apply, cascade apply, magnitude
  and group-delay analysis. Target the allocation-free callback pattern:
  `region callback { scratch in/out ... }`.
- **`vsignal`** — windowing, convolution, FIR/IIR application, spectrogram.
- **`vinterp`** — 1-D/2-D nearest/linear/cubic/bilinear.
- **`vunits`** — dimensional conversions.
- **`vgeom`** — vectors, intersections, hulls (pairs with graphics/DSP demos).

**Deliverables**
- `vfilter` + `vfft` real path with a 4-band EQ demo that is provably
  allocation-free inside the callback region.
- FFT benchmark within the honest 0.7–0.9× FFTW band.
- Spectrogram / convolution demos on real data.

**Acceptance criteria**
- 4-band EQ runs in a `region` callback with **zero arena allocations** measured
  across N blocks.
- `rfft` validated against a reference FFT; batched transforms match per-call.
- Plan caching survives region rewinds or is rebuilt safely (no dangling reads).

**Dependencies.** M1 (performance), M2 (`vnum` for filter design helpers).
**Effort:** 4–6 weeks.

---

### M4 — Compile-time guarantees for numerical code (the differentiator)

**Goal.** Move the language's guarantees from *measured* to *checked*: shapes,
ranges, allocation effects, uniqueness, and per-file safety. This is what makes
Vyne a numerical language rather than a general one with `scratch` bolted on.

**Scope**
- **Shape types** (`F8`): `Float64[M, N]` with unification at call sites and
  shape arithmetic; `scratch buf :: Float64[M, N]`; shape inference for `vlin`
  returns.
- **Effect typing** (`F9`): `!alloc`, `!io`, `!block`, `!throw`, `pure`;
  effect inference = union of callees. Turns the audio-callback rule into a
  signature.
- **Uniqueness / linear types** (`F7`): `unique` annotation enabling in-place
  mutation with no alias check or defensive copy; `W = W - lr*grad` lowers to an
  in-place update.
- **Per-file safety rulesets** (`todo_rulesets.md`): `ruleset { safety {
  scratch_bounds = off; region_escape = off; } }`, **CLI > file > default**
  precedence, build-time `[safety]` report.
- **Static peak bounds**: compose `@bounded`, `scratch`, and shape types so peak
  memory is a compile-time number.
- Promote remaining region policies from `todo_2.md` as capacity allows:
  `@ring<N>`, `@persistent`, `@streamed`.

**Deliverables**
- Type-system extensions in parser/`types.h`/`ctype.h` + codegen.
- A "signature proves it" demo: a kernel whose header states no allocation,
  in-bounds access, matching shapes, and in-place mutation.
- Updated diagnostics and docs for every new check.

**Acceptance criteria**
- `VNE-072` disappears entirely for well-typed shape+range code.
- A function annotated `!alloc` is verified by the compiler, not by convention.
- `unique` in-place update mutates storage (no per-step allocation) and is
  rejected when aliased.
- Per-file safety has a passing test matrix (CLI override, import isolation,
  unknown-key warning).

**Dependencies.** M1 (range infrastructure). Shape/uniqueness/effects compose but
can land in independent slices. **Effort:** 3–6 months (each axis is
paper-scale; land in slices, not all at once).

---

### M5 — Flagship domain pipeline: computational chemistry

**Goal.** Prove the numerical stack on a real scientific pipeline. This is the
"does it actually work" milestone, and `vchem/` is already scaffolded.

**Scope (from `todo_drug_discovery.md`, Stage 1 → Stage 4)**
- **Stage 1 — Ingestion:** finish `vchem_common` (elements 1–118), `vmol`
  (SDF V2000 + data block, MOL2, PDB, XYZ), `vsmiles` (parser + non-canonical
  writer; canonicalization deferred), grow `vgraph` (subgraph isomorphism VF2,
  MCS).
- **Stage 2 — Descriptors:** `vdesc` (MW, counts, Lipinski, Wildman-Crippen logP,
  TPSA, QED), `vfp` (Morgan/ECFP, MACCS, Tanimoto), `vcluster`, `vscaffold`.
  Requires `vcsv`, `vstat`, `vbitset` from M2.
- **Stage 3 — 3D physics:** `vforce` (UFF energy + analytic gradients,
  Gasteiger charges), `vconf` (distance geometry + MMDS, torsions), `vnum`
  eigenvalue solver, `vsparse`.
- **Stage 4 — Docking:** `vdock` (grid, MC + simulated annealing, Vina-style
  scoring), `vsmarts`, `vpdbfix`.

**Deliverables / checkpoints**
- **M1 milestone:** read a ChEMBL compound (SDF → `Molecule`) matching RDKit.
- **M2:** parse SMILES to the same graph.
- **M3:** Lipinski descriptors within 1%.
- **M4:** Morgan fingerprint; Tanimoto within 0.01 of RDKit.
- **M5:** generate a 3-D conformer within 1.5 Å RMSD for 70% of test molecules.
- **M6:** dock a compound near Vina's score.
- **M7:** screen a DUD-E target; ROC AUC within 5% of Vina.

**Acceptance criteria.** The `screen.vy` command in `todo_drug_discovery.md`
runs end-to-end and emits a ranked CSV, with compile-time peak memory, within
the documented time budget.

**Dependencies.** M2 (`vcsv`, `vstat`, `vsparse`, `vnum`, `vlin`, `vfft`) and
M1 codegen. The device path is **not** required (chemistry is CPU-bound).
**Effort:** 6–12 months (tip of the pyramid is `vdock`, 8–12 weeks alone). Run in
parallel with M3/M4; do not block the core language work.

---

### M6 — Accelerator interop (strategic, gated)

**Goal.** Extend the same region discipline to a second memory space: GPU
compute through `@device` regions and cuBLAS, without writing CUDA kernels.

**Scope (from `todo_2.md` F2–F5)**
- **F2** device arena (`cudaMalloc`-backed, same checkpoint/rewind API).
- **F3** `@device` region modifier; residency stack in the emitter; reject
  `scratch` and `region.commit` on device values in v1.
- **F4** explicit `to_device` / `from_device` at region boundaries only.
- **F5** three-way dispatch: host loop / `cblas_dgemm` / `cublasDgemm`.

**Acceptance criteria**
- Same source runs host and device; checksums match.
- Mixing residencies in one expression is a **compile error**.
- Link flags appear only when `@device` is present.

**Dependencies.** M1 (native ABI, BLAS path validated). **Effort:** ~6 weeks of
bounded additions once M1 lands (F0/F1 already done). **Gating:** only pursue if
the CPU numerical story is already strong; the docs explicitly warn not to write
GPU codegen, only interop.

---

### Cross-cutting track X — Verification, tooling, ecosystem (continuous)

Runs through every milestone:

- **Verification:** restore the root test harness; golden `.vy.c` codegen tests;
  safety suite; reference-tolerance tests per library; host-side AVR compile in CI.
- **Tooling:** keep LSP (`editors/vscode/lsp`), Neovim plugin, and diagnostics
  in sync with new syntax (shape/effect/unique/rulesets).
- **Docs:** reconcile the drift between `docs/` HTML pages, `docs/tutorials/`,
  and actual code state (new region policies, landed codegen fixes).
- **Utilities:** `vjson` README (pending), `vplot`, `vstring`, `vconvert`.

---

## 5. Horizon summary — what is realistic, and when

| Horizon | Milestones | What becomes true |
| --- | --- | --- |
| **0–1 month** | M0 | Reproducible baseline; Paper 1 gate closed; compiler bugs + peepholes fixed; tests restored |
| **1–4 months** | M1, start M2 | Numerical kernels match C **with checks on**; range-based bounds elision; core stats/numerics/optimizers/sparse/LAPACK |
| **3–8 months** | M3, M5 Stage 1–2 | Real FFT, filters, signals; chemistry ingestion + descriptors verified vs RDKit |
| **6–12 months** | M4, M5 Stage 3–4 | Shape/effect/uniqueness guarantees; per-file safety; conformer generation and docking pipeline |
| **12+ months** | M6, M4 tail | Device interop; full static shape/peak proofs; autodiff-capable tensor types (needs Papers 2/3) |

**Defensible positioning after M1–M3:** a no-GC numerical language that matches C
on dense kernels, ships a validated scientific standard library, and gives
region-based deterministic memory on both workstation and MCU.

**Defensible positioning after M4–M5:** the only numerical language where
"no allocation, in bounds, correct shape, in-place, bounded peak memory" is a
**signature** the compiler checks, demonstrated on a real drug-discovery
pipeline.

---

## 6. Dependency graph

```
M0 (baseline + Paper 1 gate)
 │
 ├──► M1 (perf parity: peepholes, range types, Tensor kind)
 │      ├──► M2 (voptim, vstat, vnum, vsparse, vlapack, vsort/vsearch/vbitset, vcomplex/vquat)
 │      │      ├──► M3 (vfft rfft, vfilter, vsignal, vinterp, vunits, vgeom)
 │      │      └──► M5 (vchem: vmol, vsmiles, vdesc, vfp, vforce, vconf, vdock)
 │      └──► M6 (device arena, @device, transfers, cuBLAS)  [gated on M1]
 │
 └──► M4 (shape types, effects, uniqueness, per-file safety, static peak bounds)
        └── composes with M2/M3/M5; can land in independent slices
```

Hard sequencing rules preserved from the planning docs:

- Do **not** start M1–M4 compiler work before M0 closes the Paper 1 gate.
- Do **not** start a library before its dependencies are done; serialize library
  work (one library at a time).
- Do **not** ship `vblas`/`vlapack` before the native array ABI (already done)
  and the region-backed buffers are stable.
- Do **not** build `vtensor`/autodiff before shape types (M4) and linear types.

---

## 7. Success metrics

Concrete, reviewable numbers that define "scientific/numerical programming is
achieved":

| Metric | Target | Milestone |
| --- | --- | --- |
| 1024² matmul vs hand-written C | checksum identical; peak RSS parity; wall clock within noise, **checks on** | M1 |
| Bounds-check cost on well-typed kernels | **0%** (statically elided) | M1/M4 |
| FFT vs FFTW | 0.7–0.9× | M3 |
| Real-time EQ callback allocations | **0 bytes** over N blocks | M3 |
| Numerical library reference agreement | within documented tolerance (1% stats, 0.01 Tanimoto, 1e-4 gradients) | M2/M5 |
| ML training epochs-to-target with `voptim` | measurably fewer than current SGD | M2 |
| Shape/alloc/in-bounds/in-place proof | compiler-verified for a flagship kernel | M4 |
| Chemistry pipeline | DUD-E ROC AUC within 5% of AutoDock Vina | M5 |
| Test coverage | every shipped library has a test + reference check; CI green on 3 backends | X |

---

## 8. Risks, constraints, non-goals

### Constraints (from the planning docs)
- **Paper 1 freeze:** the compiler should not move until the regions/scratch
  paper is submitted. M0 is the release valve.
- **Serialized library work:** each library teaches something about the
  language; parallel library pushes create rework.
- **AVR is a 2 KB SRAM target:** numerical choices there are constrained; desktop
  is the numerical proving ground.
- **Dual license:** AGPL-3.0 compiler, MIT runtime — contributions must respect
  the SPDX split.

### Risks
- **Scope explosion:** `todo_libraries.md` lists 30+ libraries. The milestone
  plan deliberately caps M2/M3 to the numerically load-bearing set.
- **Codegen fragility:** the native struct/typed-array paths are where correctness
  bugs hide (see residual C2/C3 items). Golden `.vy.c` tests are mandatory.
- **Docs drift:** docs and code are already out of sync; every milestone must
  update docs as part of acceptance, not after.
- **Paper-gate slippage:** if M0 stalls on paper edits, it blocks the entire
  numerical program. Timebox the paper edits.
- **Device path temptation:** `todo_2.md` is explicit — interop, not CUDA codegen;
  region-boundary transfers only, no mid-region switching.

### Non-goals (explicit)
- A general-purpose language (no chasing Python/Go).
- Writing CUDA kernels (only calling cuBLAS through device regions).
- Multi-stage programming.
- Reworking the runtime to shrink `VyneValue` (breaks array layout).
- `-ffast-math` / `-march=native` as defaults (opt-in only; keeps the baseline
  honest).
- Web/dashboard/ADMET/retrosynthesis front-ends for the chemistry pipeline.

---

## 9. Traceability — planning docs → milestones

| Planning doc | Key items | Milestone |
| --- | --- | --- |
| `todo.md` | Paper 1 edits, Phase 0/1 done, benchmarks, Experiment 3 decision | **M0** |
| `todo_optimization.md` | Tier 0/1 peepholes, Tier 2 runtime, O3.1 range types, O3.4 Tensor | **M0**, **M1** |
| `todo_2.md` | F0 (done), F1 (done); F6 range, F9 effects, F8 shape, F7 unique, F10 units, F12 const, F13 precision; F2–F5 device | **M1**, **M4**, **M6** |
| `todo_libraries.md` | voptim, vstat, vnum, vsparse, vlapack, vsort/vsearch/vbitset, vcomplex/vquat, vfilter, vsignal, vinterp, vfft rfft | **M2**, **M3** |
| `todo_rulesets.md` | Per-file `safety { }` rulesets, CLI precedence, build report | **M4** |
| `todo_drug_discovery.md` | C0–C5 (mostly done), vchem stages 1–4, M1–M8 pipeline | **M5** |

---

## 10. Recommended first actions (read-only recommendation)

1. **Finish M0 paper edits** (§1 reframing, §7 paragraph, §5.7 typo) — this is
   the documented gate for everything else.
2. **Restore the test harness** so M1's performance work has a regression net.
3. **Land `O0.1`/`O0.2` + Tier 1 peepholes** and re-measure the matmul; this is
   ~1 day and converts the "3.9% gap" into a known, closed number.
4. **Prototype range refinement on the matmul induction variables** before
   committing to the full F6 design — it is the single largest positive-framing
   win for the numerical story (checks become free).
5. **Keep chemistry and device work off the critical path** until M1/M2 are
   green; they are demonstrations of the core, not substitutes for it.

---

*This roadmap is a planning artifact. No source files were modified to produce
it; the only new file is this `milestone.md`.*

---

# 11. Advanced region-based memory model — comprehensive idea set

> **Why this section exists.** The current memory model is already the language's
> strongest differentiator: lexical `region`, stack `scratch`, `@pool`,
> `@speculative`, `@bounded`, `@zeroed`, `@deterministic`, commit arena, escape
> analysis. The ideas below are the *next* layer. They are deliberately not a
> re-proposal of **region rules / per-file safety rulesets** (your direction,
> covered in `todo_rulesets.md`); they are **mechanisms, analyses, and type-system
> features** that make the region model more capable, more checkable, and more
> useful for scientific and numerical work.

## 11.0 Design principles for every idea

Every proposal below must preserve these invariants:

1. **Lexical ownership.** Every byte has a region whose closing brace owns its
   collection. No hidden GC, no finalizers, no liveness guessing in release.
2. **Determinism.** Same source + same inputs ⇒ same allocation behaviour and,
   where promised, same bytes. No allocator-order nondeterminism in hot paths.
3. **Zero-cost when provable.** A guarantee that can be proven at compile time
   emits no runtime check; a guarantee that cannot be proven falls back to a
   runtime trap, never to silent corruption.
4. **Backend parity.** Any feature that reaches the compiled backends must either
   work on AVR or fail with a clear diagnostic on AVR — never silently diverge.
5. **Composition over keywords.** Prefer a new *attribute on an existing
   construct* over a new statement form. The region is the interface to the
   allocator; new features parameterize or analyze it.

**Scoring dimensions** used in the catalog: *Leverage* (how much it unlocks),
*Safety* (how much it removes from the runtime), *Fit* (how naturally it composes
with regions), *Effort* (S = ≤3 days, M = 1–2 weeks, L = 3+ weeks), *Risk*.

---

## 11.1 Idea taxonomy (map)

| Group | Theme | Count | Representative ideas |
| --- | --- | --- | --- |
| **A** | Static region analysis & region polymorphism | 8 | region-polymorphic functions, region inference, region handles, region subtyping |
| **B** | New allocator mechanisms and policies | 10 | size-class slab, copying compaction, lazy virtual, static region, custom allocator, DMA |
| **C** | Safety, verification, observability | 10 | generation tags, poison-on-rewind, rewind audit, budgets, telemetry, manifest |
| **D** | Shape- and layout-aware memory | 8 | region-relative pointers, static memory planner, SoA/tiled layout, zero-copy views |
| **E** | Ownership, borrowing, moves | 7 | unique values, borrows, rehoming, scoped refcounts, alias-free certificates |
| **F** | Concurrency & parallelism | 5 | scoped threads, parallel regions, deterministic reduction, async tasks |
| **G** | Persistence, serialization, interop | 7 | FFI regions, snapshot/restore, deterministic replay, mmap regions, scoped interning |
| **H** | Numerical / DSP / embedded specialization | 8 | workspace contracts, size-class pools, kernel scratch, embedded planner, DMA |
| **I** | Language ergonomics | 6 | `with region`, per-call regions, region combinators, region diagnostics |

---

## 11.2 Group A — Static region analysis & region polymorphism

The biggest conceptual upgrade: **make the region a parameter, not an implicit
global stack.** Today allocation targets "whichever region is active." That is
ergonomic but prevents libraries from choosing the caller's lifetime, prevents
parallel arenas, and forces escape analysis to be syntactic.

| ID | Idea | Mechanism | Why it fits regions | Effort | Risk | Milestone |
| --- | --- | --- | --- | --- | --- | --- |
| **A1** | **Region-polymorphic functions** | A function can declare an implicit region parameter (`for<'r> fn f(...) -> T` allocates into `'r`); calls instantiate it with the caller's region. Emitted C takes a `Region*`. | Libraries (`vlin.multiply`) stop allocating into a global default; the caller chooses lifetime. This is the allocator-parameterization the docs hint at. | M | Med | M1/M4 |
| **A2** | **Region inference** | Infer the innermost region covering all uses of a temporary; infer region parameters for unannotated functions via a constraint solver over the call graph. | Removes manual `region` blocks for common cases while keeping the model lexical. Must be opt-in (`@infer`) to preserve predictability. | L | High | research |
| **A3** | **First-class region handles** | `Region` is a value: `r = region(); with r { ... };` and `f(r)`. Enables multiple independent arenas and passing an allocator explicitly. | Turns the runtime checkpoint stack into an explicit capability; prerequisite for A1 and F1. | M | Med | M1 |
| **A4** | **Full escape analysis (dataflow)** | Replace syntactic VNE-070 with a use-def/liveness pass: a value escapes if it reaches a store to a shallower-lifetime location or a call that requires a longer region. Emit an **escape certificate**. | Makes the guarantee sound rather than approximate; removes false positives that force `commit` everywhere. | L | High | M4 |
| **A5** | **Region subtyping / outlives** | `'child ⟂ 'parent` with rules `'child : 'parent`; assignments require `lhs.region : rhs.region`. Values may flow outward only via `commit`/`move`. | Gives escape analysis a formal lattice; makes nested regions compositional. | M | Med | M4 |
| **A6** | **Region effects** | Extend effect typing with `allocates(r)`, `commits(r)`, `escapes(r)`, `reads(r)`, `writes(r)`. `!alloc` is the special case "no region effect." | A kernel signature can say "allocates only in `r`, commits nothing" — the real-time contract, checkable. | M | Med | M4 |
| **A7** | **Allocation cost model** | Static bytes-per-call / bytes-per-iteration estimate per allocation site, surfaced as a diagnostic and in the manifest (C7). | Lets the compiler warn "this loop allocates 16.7 MB/iteration" before you run the benchmark. | M | Low | M1/M4 |
| **A8** | **Region-aware monomorphization** | Specialize kernels on `(element type, alignment, layout policy, region policy)` tuples, not just type. | A `dot` instantiated for `@align<32>` + `@soa` can emit different, faster C with no source change. | M | Med | M1 |

**Key design note for A1/A3.** The cleanest surface is:

```vyne
fn map_add(r :: Region, a :: Array<Float64>, b :: Array<Float64>) -> Array<Float64>
    allocates(r) {
    ...
}
```

Callers write `with region { out = map_add(r, a, b); }`, and the interpreter and
C backend both realize `r` as a checkpoint handle. This subsumes "which arena am
I in?" without a global.

---

## 11.3 Group B — New allocator mechanisms and policies

Policies are only real if a concrete allocator sits behind them (`todo_2.md`'s
rule: *"a policy should map to an actual allocator in `detail/arena.h`"*). These
are mechanism ideas, most needing runtime support.

| ID | Idea | Mechanism | Why it fits regions | Effort | Risk | Milestone |
| --- | --- | --- | --- | --- | --- | --- |
| **B1** | **Size-class slab region `@slab`** | Power-of-two size classes (16 B … 1 MB), per-class free lists; allocate by rounded shape; O(1) alloc/free. | Generalizes today's fixed `@pool<T,N>` to "any shape that fits a class." The numerical buffer allocator. | M | Low | M1 |
| **B2** | **Copying compaction `@copying`** | Semispace within the region; live set compacted on demand. | Eliminates fragmentation for long-lived region caches without leaving the region model. | L | High | research |
| **B3** | **Descending / LIFO region `@down`** | Bump pointer descends from a high-water mark; free is still scope close. | Exact stack semantics for recursive numerical kernels; pairs with per-call regions (I2). | S | Low | M2 |
| **B4** | **Lazy virtual region `@lazy<reserve>`** | Reserve virtual address space, commit pages on touch (`mmap`/`madvise`, `VirtualAlloc` reserve/commit). | Huge sparse matrices / large tensors without paying peak RSS up front; peak becomes "touched bytes." | M | Med | M2 |
| **B5** | **Aligned / huge-page region `@align<N>`, `@huge`** | Guarantee 32/64-byte alignment or transparent huge pages for the region's arrays. | SIMD kernels get aligned loads with zero per-array fixups; `--native`-independent. | S | Low | M1 |
| **B6** | **Static region `@static`** | Region storage is a fixed `.bss` array or `static` buffer, no runtime bump; all sizes compile-time. | Perfect for AVR and for "weights live here for the whole program" with zero allocator overhead. | S | Low | M2 (AVR) |
| **B7** | **Custom / foreign allocator `@alloc<provider>`** | Runtime hook (`alloc`/`free` callbacks) selects `malloc`, `mmap`, `cudaMalloc`, an RTOS pool, or a user allocator; close calls `free`. | One construct wraps any external memory while retaining checkpoint/rewind bookkeeping. | M | Med | M2/M6 |
| **B8** | **Recycling region `@recycle`** | Allocations keyed by `(site, shape)` are reused across iterations; optional zero/poison on reuse. | Turns a per-iteration `region` into a zero-cost steady state without relying on the optimizer. Must be explicit about stale data. | M | High | M2 |
| **B9** | **Epoch region `@epoch`** | Deferred reclamation at safe points; allocations tagged with an epoch. | Fits async/simultaneous workloads without weak refs; complements region-scoped threads (F1). | L | High | research |
| **B10** | **DMA / volatile region `@dma`, `@volatile`** | Buffers placed at compatible alignment/address; volatile access semantics; double-buffer variants. | Embedded DMA and peripheral buffers must obey placement rules; making it a region policy is the right abstraction. | M | Med | AVR roadmap |

**Caution on B8/B9.** Both introduce *non-lexical reuse* of bytes. They must be
opt-in, named clearly, and never change the observable value of a fresh
allocation unless `@recycle` explicitly documents "contents are undefined." A
`@recycle @zeroed` combination should be the safe default.

---

## 11.4 Group C — Safety, verification, and observability

These make the memory model *auditable*. They are the cheapest ideas with the
highest debugging ROI, and they compose with everything else.

| ID | Idea | Mechanism | Why it fits regions | Effort | Risk | Milestone |
| --- | --- | --- | --- | --- | --- | --- |
| **C1** | **Generation-tagged pointers (checked mode)** | Fat pointer `{region_id, generation, offset}`; rewind bumps the region generation; stale deref traps. Release mode compiles to a thin pointer when escape analysis proves safety. | Turns use-after-rewind from "undefined" into a deterministic trap in debug; zero cost in verified builds. | M | Med | M4 |
| **C2** | **Poison-on-rewind / poison-on-alloc** | Fill rewound bytes with NaN (`Float64`), a sentinel (`Int64`), and `0xA5` (bytes); optionally poison fresh allocations to catch read-before-write. | A use-after-rewind then produces `NaN` in a numeric result instead of a random number — caught by any sanity check. | S | Low | M1 |
| **C3** | **Rewind audit** | Shadow map of live pointers per region (debug); at rewind, verify none still referenced; report the allocation site and the holder. | Makes VNE-070's runtime backstop explicit and gives it a source-span diagnostic. | M | Low | M1 |
| **C4** | **Region budget contracts `@budget<N>`** | Compile-time check when all sizes are static; runtime trap otherwise. Hierarchical: a parent budget caps children. | Extends `@bounded` from one region to a tree, giving a program-wide peak bound. | M | Low | M2 |
| **C5** | **Scratch stack-budget analysis** | Worst-case `scratch` depth × bytes per frame; reject or warn if it exceeds `@stack-budget`. | Critical on a 2 KB AVR: turns "it crashed at runtime" into a compile error. | M | Low | M2 (AVR) |
| **C6** | **Allocation telemetry** | Per-region / per-site high-water, count, total; emit Chrome-trace / JSON. | The benchmark story already measures peak; this attributes it. | S | Low | M1 |
| **C7** | **Region manifest (memory contract report)** | Build emits a per-region manifest: peak bytes, commit bytes, escapes, allocation sites, policy, and whether each is statically proven. | The memory-model analogue of the `[safety]` report; makes guarantees reviewable in CI. | M | Low | M1 |
| **C8** | **Unbounded-commit detection** | Static loop analysis: a `commit` inside an unbounded loop without a matching rewind grows the commit arena; warn with the site. | The README already documents this as the main leak; codify it. | S | Low | M1 |
| **C9** | **Deterministic layout guarantee** | `@deterministic` additionally fixes allocation order and padding so region snapshots are byte-identical across runs/backends. | Required for G2/G3 and for reproducible scientific artifacts. | M | Med | M2 |
| **C10** | **Memory modes `--memory=checked\\|verified\\|off`** | Per-region / per-file selection of generation tags + audit vs. proven-thin vs. no checks, with a build report. | Distinct from region rules (which *waive safety*); this *chooses the verification level*. | M | Low | M4 |

**Why C1+C2 together are the highest-ROI pair.** Escape analysis catches the
syntactic cases; generation tags and poison catch the aliasing cases the analysis
cannot see. Neither changes release semantics. Both make `region` debugging feel
like Rust's borrow-checker feedback instead of "why is my loss NaN?"

---

## 11.5 Group D — Shape- and layout-aware memory

The endgame for numerical work: the region knows *how big* and *how laid out*,
so allocation becomes pointer arithmetic and peak memory becomes a constant.

| ID | Idea | Mechanism | Why it fits regions | Effort | Risk | Milestone |
| --- | --- | --- | --- | --- | --- | --- |
| **D1** | **Region-relative pointers `RRef<T>`** | 32-bit `{block_id, offset}` instead of 64-bit pointers inside region-owned structures; deref = `block_table[block_id] + offset`. | Halves pointer-heavy structures (graphs, molecules, sparse indices). Lifetime safety is already guaranteed by the region. | M | Med | M2/M5 |
| **D2** | **Static memory planner** | When shapes and control flow are static, assign each allocation a stack slot or `.bss` offset; eliminate arena calls; report **peak as a compile-time number**. | The full promise of "compile-time peak bounds"; subsumes `scratch` budgeting and `@static`. | L | High | M4 |
| **D3** | **Shape-typed region `region<Float64[M,N]>`** | Region declares the shapes it may allocate; allocation sizes become expressions in `M,N`; no per-allocation header. | Composes shape types (F8) with the allocator: "this region holds one `M×N` tile." | M | Med | M4 |
| **D4** | **SoA layout policy `@soa`** | A region (or struct) stored structure-of-arrays; field access rewritten to strided loads; native struct ABI carries the layout. | Numerical kernels want SoA; making it a region/layout property keeps the source clean and the C layout predictable. | M | Med | M2 |
| **D5** | **Tiled / blocked layout `@tiled<64>`** | Compiler-aware blocking of 2-D arrays; index lowering maps logical `[i,j]` to tile-local storage. | Cache-blocked matmul without hand-written blocking; pairs with `vlin`. | L | High | M3 |
| **D6** | **Zero-copy views / borrow slices `view<T>`** | `view` = `{ptr, len, region}` with a lifetime bound; slicing a typed array returns a view, not a copy. | Submatrices, windowed FFT, convolution input, graph neighbourhoods — all without allocation. | M | Med | M4 |
| **D7** | **Scratch trees / parallel-safe scratch stacks** | Per-worker scratch stacks (F1) with a region-owned parent; `scratch` resolves to the current worker's frame. | Makes `scratch` correct under parallelism without locks. | M | Med | M4 |
| **D8** | **Per-element padding / false-sharing avoidance** | `@padded<N>` region layout for parallel reductions (cache-line padding); `@align` for SIMD. | Parallel numeric reductions need padded accumulators; encode it once. | S | Low | M3 |

**D1 is the sleeper idea.** On 64-bit hosts, a graph or molecule with millions of
edges spends half its memory on pointers. A 32-bit region-relative pointer makes
`vgraph`/`vmol` approximately 2× smaller and cache-friendlier — a real scientific
win that is only safe *because* the region bounds the lifetime. It also makes
regions naturally serializable (G4): an offset is portable, a pointer is not.

---

## 11.6 Group E — Ownership, borrowing, and moves

Regions answer *when* memory dies. Ownership answers *who may touch it*. Combined,
they give in-place numerical kernels with no alias checks and no defensive copies.

| ID | Idea | Mechanism | Why it fits regions | Effort | Risk | Milestone |
| --- | --- | --- | --- | --- | --- | --- |
| **E1** | **Unique / linear values** | `unique T`: exactly one reference; enables in-place mutation with no alias check or copy. | `W = W - lr*grad` lowers to an in-place update; the region proves `W` doesn't outlive the update. | L | High | M4 |
| **E2** | **Borrow parameters `&T` / `&mut T`** | Read-only / mutable borrows with region lifetimes; a borrow cannot outlive its region. | Libraries take `&Matrix` instead of copying; composes with D6 views. | L | High | M4 |
| **E3** | **Move / rehome across regions `move x to r`** | Transfer a value's allocation to a parent region without a deep copy when the allocation is the last / a whole block (block promotion). | Eliminates copy-on-commit for the common "return this buffer" case. | L | High | research |
| **E4** | **Region-scoped reference counts** | Non-atomic refcounts reset at region close; copy-on-write within a region. | Shared immutable tensors without atomics, safe because the region is single-threaded. | L | Med | research |
| **E5** | **Alias-free region certificate `@unique region`** | Compiler proves no value allocated in the region aliases anything outside it (and vice versa); enables in-place ops and rewind without audit. | The strongest form of "this region is a sandbox"; makes E1 usable at region granularity. | L | High | M4 |
| **E6** | **In-out / must-alias contracts `inout`** | Parameters guaranteed to alias the same buffer; enables fused kernels (`C += A*B`) with one region and no temporaries. | BLAS `beta`/`alpha` semantics become a type property; fused ops allocate nothing. | M | Med | M2 |
| **E7** | **Freeze / thaw regions** | A region can be frozen (immutable view shared widely) then thawed for a new write phase. | Weight matrices: freeze after training, share read-only across inference regions. | M | Med | M4 |

---

## 11.7 Group F — Concurrency and parallelism

Regions are naturally per-thread scopes. The design tenet stays: *the region is
the interface to the allocator, and each thread owns its own arena.*

| ID | Idea | Mechanism | Why it fits regions | Effort | Risk | Milestone |
| --- | --- | --- | --- | --- | --- | --- |
| **F1** | **Region-scoped threads** | `spawn` inside a region creates a child arena; threads must join before the region closes; no cross-thread allocation. | Rewind after join is safe by construction; no locks in the allocator. | L | High | M4 |
| **F2** | **Parallel region `parallel region`** | Body partitioned across workers, each with a sub-region; a `reduce`/`combine` block merges results deterministically. | The numerical primitive: parallel matmul/FFT with region-per-worker scratch. | L | High | M4 |
| **F3** | **Structured async tasks** | Tasks are scoped to a region; cancellation = rewind; a task cannot outlive its region. | Async without lifetime escape; complements B9 epoch reclamation. | L | High | research |
| **F4** | **Deterministic reduction** | Fixed reduction tree order regardless of thread count, so results are bit-identical across `-jN`. | Scientific reproducibility is a requirement, not a nice-to-have. | M | Med | M4 |
| **F5** | **Shared read-only region `@shared`** | A frozen region shared across workers with no interior mutability (or explicit locks). | Inference weights shared read-only; each worker writes only its own region. | M | Med | M4 |

---

## 11.8 Group G — Persistence, serialization, interop

| ID | Idea | Mechanism | Why it fits regions | Effort | Risk | Milestone |
| --- | --- | --- | --- | --- | --- | --- |
| **G1** | **Region-aware FFI** | Pass arena pointers + lengths to C with a borrow contract; foreign memory can be wrapped in an `@alloc` region (B7); forbid storing pointers past the region. | Zero-copy BLAS/LAPACK/OS calls without abandoning lifetime guarantees. | M | Med | M2 |
| **G2** | **Region snapshot / restore** | Copy a region's bytes at a checkpoint and restore later. Requires `@bounded` + no FFI pointers + `@deterministic`. | Time-travel debugging, autotune rollback, simulation checkpoints — using the same checkpoint machinery. | M | Med | M3 |
| **G3** | **Deterministic replay** | RNG streams + deterministic layout (C9) + snapshot (G2) reproduce a run bit-for-bit. | The scientific reproducibility story; pairs with checkpointing. | M | Med | M3 |
| **G4** | **Region serialization with relative pointers** | Serialize a region as `{bytes, block table}` using `RRef` (D1); restore by mmap. | Zero-copy model weights, chemistry databases, and snapshots that survive process exit. | L | High | research |
| **G5** | **Memory-mapped persistent region** | `@persistent @mmap<file>` maps a file as a region; read-only weights or an append-only store. | Weight loading without parse/copy; ties to the `vurage` heritage. | M | Med | M3 |
| **G6** | **Device / unified-memory regions** | Extends F2–F5 from `todo_2.md`: unified memory, async-copy region, double-buffer pipeline region. | One source, host or device, same region discipline. | L | High | M6 |
| **G7** | **Region-scoped interning / symbol arena** | An intern table whose entries are reclaimed at region close (parsers, JSON, SMILES). | Today interning is process-global and never freed; a parse-heavy pipeline leaks symbols. Region-scoped interning is a real fix. | M | Low | M2 |

**G7 is especially concrete.** `vjson`, `vsmiles`, and any recursive-descent
parser allocate transient symbols. A region-scoped intern pool that is rewound
with the parse means a 100k-molecule SDF parse stays flat instead of growing the
global intern table. It also demonstrates the memory model on a workload the
matmul benchmark cannot show.

---

## 11.9 Group H — Numerical / DSP / embedded specialization

| ID | Idea | Mechanism | Why it fits regions | Effort | Risk | Milestone |
| --- | --- | --- | --- | --- | --- | --- |
| **H1** | **Workspace contracts** | A solver declares `workspace_bytes(n)`; the caller supplies a region whose size is checked (static or runtime). | LAPACK's `lwork` query becomes a region contract; no hidden allocations inside solvers. | S | Low | M2 |
| **H2** | **Size-class numeric buffer pool** | `@pool` generalized to shapes/size classes; `alloc(shape)` returns a slot; O(1). | Training loops reuse gradient/activation buffers across steps with zero fragmentation. | M | Low | M2 |
| **H3** | **Kernel scratch stack** | Thread-local `scratch` stacks so kernels allocate nothing and are re-entrant. | Correct `scratch` under recursion/parallelism; the audio-callback contract. | M | Med | M3 |
| **H4** | **BLAS/LAPACK workspace bridge** | Allocate `lwork` buffers in a region and pass to `cblas`/`lapack` directly. | Zero-copy interop; keeps the "no allocation in the hot loop" claim. | S | Low | M2 |
| **H5** | **Zero-allocation callback verification** | Effect/telemetry proves a callback region performs no `arena_alloc`. | Makes the real-time DSP promise a compile-time fact, not a convention. | M | Med | M3 |
| **H6** | **Embedded static arena planner** | With static shapes, place all `scratch`/`@static` in `.bss`/stack and error if the 2 KB budget is exceeded. | Turns AVR memory overflow into a compile error; a headline embedded feature. | L | High | M4/M6 |
| **H7** | **Region-aware DMA buffers** | Fixed alignment/placement + double-buffer policy (B10) for peripheral I/O. | Embedded scientific sensing: ADC/DMA streams into region-owned buffers. | M | Med | AVR roadmap |
| **H8** | **Packed precision regions `@packed`** | Store `f32`/`bf16` in a region with conversion at the boundary; region knows the storage type. | Memory/bandwidth reduction for large tensors; composes with F13 precision types. | M | Med | M4 |

---

## 11.10 Group I — Language ergonomics

| ID | Idea | Mechanism | Why it fits regions | Effort | Risk | Milestone |
| --- | --- | --- | --- | --- | --- | --- |
| **I1** | **`with region { ... }` expression** | Region as an expression returning a committed value or a view. | Removes the statement-only limitation; enables `out = with region { ... };`. | S | Low | M2 |
| **I2** | **Automatic per-call region** | An allocating function called inside a loop gets a child region scoped to the call (opt-in `@autoregion`). | Recursive numerical functions keep flat memory without manual annotations. | M | Med | M2 |
| **I3** | **Region combinators** | Named patterns (`region.map`, `region.reduce`) over typed arrays with guaranteed lifetimes. | Ergonomic pipelines without allocating intermediate boxed arrays. | M | Med | M3 |
| **I4** | **Region result syntax `region -> value`** | The region's last expression is committed/returned. | Terse, readable numerical helpers. | S | Low | M2 |
| **I5** | **Exported region declarations** | Interfaces can declare a region parameter, so libraries document their allocation contract in the type. | Callers see "this function needs a region" in the signature/hover. | M | Low | M4 |
| **I6** | **Region stack traces** | Diagnostics show "allocated here → region opened here → rewound here" with source spans. | Turns VNE-070 and runtime audits into actionable feedback. | M | Low | M1 |

---

## 11.11 Deep dives — the ten highest-leverage ideas

### D-1. Region-polymorphic functions + first-class region handles (A1 + A3)

**Problem.** Today `arena_alloc` targets a global current region. A library
cannot say "allocate into the caller's region," and two independent arenas are
impossible. This blocks parallelism and makes escape analysis syntactic.

**Design.**
```vyne
fn zeros(r :: Region, n :: Int64) -> Array<Float64> allocates(r) { ... }

region step {
    buf = zeros(r_current, 1024);   # explicit handle
};
```
- `Region` is an opaque handle `{block, offset, generation}`.
- The emitter passes it as a hidden/`Region*` parameter; the interpreter seeds it
  as a scope id.
- `allocates(r)` is checkable (A6); calling without a region is a compile error.

**Pitfalls.** Default-region inference must stay deterministic. Do not let a
handle be stored past its region (that is what C1 catches). AVR: region handle is
a `uint16_t` index into a fixed stack.

**Acceptance test.** Two interleaved regions allocate and rewind independently;
a library call allocates into the caller's region; rewind leaves the other
region intact.

**Effort.** M. **Why first.** It is the substrate for A2, A4, D3, F1, F2, H1.

---

### D-2. Static memory planner → compile-time peak bounds (D2)

**Problem.** The docs promise "compile-time peak bounds," but today peak is
*measured* by `vmem` after the fact.

**Design.** When a region's control flow and shapes are static (no dynamic
recursion, no data-dependent allocation, all `scratch`/shape types known), the
planner computes the exact set of live buffers and assigns:
- stack offsets for `scratch`,
- `.bss` offsets for `@static`/`@persistent`,
- a single arena offset range otherwise.

Output: `peak_bytes` in the region manifest (C7), plus a C-level `static_assert`
that the region never exceeds its `@budget`.

**Pitfalls.** Data-dependent loops (`while`) and dynamic shapes disqualify a
region; fall back to runtime `@bounded` and say so in the manifest. Do not
pretend a dynamic region is static.

**Acceptance test.** A fixed-size MLP training step reports a compile-time peak
and allocates exactly 0 arena bytes at runtime, with an emitted
`static_assert(peak <= budget)`.

**Effort.** L. **Why it matters.** It turns the paper's central claim into a
compiler output a reviewer can read.

---

### D-3. Region-relative pointers (D1)

**Design.** `RRef<T>` = `{uint32_t block; uint32_t offset;}`; regions maintain a
small block table. Deref is one add. Structs containing `RRef` are safe because
the region guarantees all targets die together.

**Use cases.** `vgraph` adjacency (millions of ints), `vmol` bond lists, sparse
indices, ASTs in `vjson`/`vsmiles`.

**Pitfalls.** A region > 4 GB per block needs 64-bit offsets; choose the offset
width by policy (`@near` / `@far`). Cross-block refs require the block table.
Interop with C expects raw pointers — provide `RRef.toPtr()` at FFI boundaries.

**Acceptance test.** `vgraph` CSR built with `RRef` is ≤ 55% the memory of the
pointer version and traverses BFS correctly; a region rewind invalidates the
whole graph.

**Effort.** M. **Why it matters.** It is a novel, region-*enabled* optimization
that general-purpose languages cannot safely offer.

---

### D-4. Zero-copy views / borrow slices (D6 + E2)

**Design.** `view<T> = {const T* ptr; Int64 len; Region lifetime;}`. Slicing a
typed array yields a view; views can be passed to kernels and returned only if
the lifetime outlives the call.

**Use cases.** Submatrix blocks, FFT windows, convolution patches, graph
neighbourhoods, sliding-window DSP.

**Pitfalls.** Mutable views need uniqueness/borrow checking (E1/E2). A view that
escapes a region is exactly what escape analysis must reject.

**Acceptance test.** `vfft` forward on a windowed view allocates 0 bytes; the
window is not copied.

**Effort.** M. **Why it matters.** Eliminates the dominant allocation in
signal-processing and linear-algebra wrappers.

---

### D-5. Region-scoped interning / symbol arena (G7)

**Design.** `@intern region parse { ... }` installs a scoped intern table whose
entries are freed at region close; lookups fall back to the global pool only for
strings that must survive.

**Use cases.** `vjson` keys, `vsmiles` tokens, SDF property names, `vmol` element
symbols.

**Pitfalls.** A string interned in a child region cannot be stored globally
without promotion — the same escape rule as any region value.

**Acceptance test.** Parsing 100k molecules in a loop keeps the global intern
table flat and arena peak bounded.

**Effort.** M. **Why it matters.** Fixes a real, documented leak vector and
demonstrates the model on non-matrix workloads.

---

### D-6. Generation tags + poison-on-rewind (C1 + C2)

**Design.**
- Debug/checked: pointers carry `{region_id, generation}`; rewind increments the
  generation; deref checks and traps with a source-annotated diagnostic.
- Always-available: `@poison` fills rewound memory with NaN/sentinel so numeric
  misuse shows as NaN.

**Pitfalls.** Fat pointers break the typed-array layout if applied globally;
apply them only to boxed/dynamic values in checked mode, or use a shadow map
(C3) instead. Poison must not run in performance builds unless opted in.

**Acceptance test.** `boxed_local_in_region.vy` and a new aliasing case trap with
"value belongs to rewound region 'step' (generation 3), allocated at line 42."

**Effort.** M. **Why it matters.** Makes the memory model debuggable; the single
biggest developer-experience upgrade.

---

### D-7. Region snapshot / restore + deterministic replay (G2 + G3 + C9)

**Design.** For a `@deterministic`, `@bounded`, FFI-free region, `snapshot(h)`
copies the region's bytes; `restore(h)` writes them back. Combined with
deterministic RNG streams, a run reproduces bit-for-bit.

**Use cases.** Autotune rollback (try config, restore, try next), simulation
checkpoints, bug reproduction, publication-grade reproducibility.

**Pitfalls.** Snapshots are invalid if the region contains raw pointers or
committed values; the type system should reject snapshotting such a region.
Copying whole regions is O(bytes); scope it to small state, not 1 GB tensors.

**Acceptance test.** A Monte-Carlo experiment snapshots at step 100, restores,
and reproduces the same final result with a second RNG seed sequence.

**Effort.** M. **Why it matters.** It is a differentiator almost no numerical
language offers at the language level.

---

### D-8. Size-class pools + workspace contracts (B1 + H1 + H2)

**Design.** `@slab region` with power-of-two classes; `alloc(shape)` rounds to a
class. Solvers expose `workspace_bytes(n)`; the caller opens `region(workspace_bytes(n))`.

**Use cases.** Training loops (grad/batch buffers), LAPACK work arrays, FFT plans,
sparse solver scratch.

**Pitfalls.** Class rounding wastes memory; expose `class_of(shape)` and a
manifest so the waste is visible. Wrong-size reuse must be a type error, not a
silent reinterpretation.

**Acceptance test.** 1,000 training steps allocate 0 arena bytes after warm-up;
peak is a compile-time constant; a deliberate shape mismatch is rejected.

**Effort.** M. **Why it matters.** This is the mechanism that makes "bounded
memory" provable rather than measured.

---

### D-9. Scoped parallel regions + deterministic reduction (F1 + F2 + F4)

**Design.**
```vyne
parallel region reduce(+sum) {
    scratch local :: Float64[256];
    # each worker gets its own child region + scratch
    partial = dot(A_view[worker], B_view[worker]);
} combine -> total;
```
- One child region and scratch stack per worker.
- Join before the parent region closes.
- Reduction tree order fixed ⇒ bit-identical across worker counts.

**Pitfalls.** No allocation from a worker into the parent; no shared mutable
buffers without `@shared`/locks (E5). Deterministic reduction is slower than
atomic adds — make it the default for scientific code and allow opt-out.

**Acceptance test.** `-j1` and `-j8` produce byte-identical checksums; arena peak
is flat and per-worker.

**Effort.** L. **Why it matters.** Parallel numerical work without abandoning the
region model or reproducibility.

---

### D-10. Region-aware FFI + custom allocator regions (G1 + B7)

**Design.** `extern "C" fn cblas_dgemm(...)` can take `view`/typed-array pointers
with a borrow contract. `@alloc<provider>` wraps foreign memory and calls the
matching `free` on region close.

**Use cases.** BLAS/LAPACK, libc, OS file mapping, CUDA host/device, RTOS pools.

**Pitfalls.** Foreign allocators may not support checkpoint/rewind; document that
such a region is "close-to-free" only, not "rewind-invalidating." Never let a
foreign pointer into a non-foreign region.

**Acceptance test.** `vlin.multiply --blas` passes region-owned buffers to
`cblas_dgemm` with zero copies; a malloc-backed `@alloc` region frees on close.

**Effort.** M. **Why it matters.** It keeps the ecosystem interoperable without
losing the memory contract.

---

## 11.12 Composition matrix

`++` = strong synergy, `+` = compatible, `~` = needs care, `-` = avoid.

| Idea ↓ / Existing → | `@pool` | `@bounded` | `@zeroed` | `@deterministic` | `scratch` | Shape types | Uniqueness | Effect typing | AVR |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A1 region-poly | ++ | ++ | + | + | ++ | ++ | + | ++ | + |
| A3 region handles | ++ | ++ | + | + | + | ++ | + | ++ | + |
| A4 full escape | + | ++ | + | + | ++ | ++ | ++ | ++ | ++ |
| B1 slab pool | ++ | ++ | + | + | + | ++ | + | + | + |
| B4 lazy virtual | + | ++ | + | ~ | - | + | + | + | - |
| B6 static region | + | ++ | ++ | ++ | ++ | ++ | + | + | ++ |
| C1 generation tags | + | ++ | + | ++ | ~ | + | + | + | ~ |
| C2 poison | + | + | ++ | ++ | ~ | + | + | + | ++ |
| C7 manifest | ++ | ++ | ++ | ++ | ++ | ++ | + | ++ | ++ |
| D1 relative ptr | ++ | ++ | + | ++ | ~ | ++ | + | + | + |
| D2 static planner | ++ | ++ | ++ | ++ | ++ | ++ | + | ++ | ++ |
| D6 views | + | ++ | + | + | ++ | ++ | ++ | ++ | + |
| E1 unique | ++ | + | + | + | + | ++ | ++ | ++ | + |
| E5 alias-free region | ++ | ++ | + | ++ | + | + | ++ | ++ | + |
| G7 scoped intern | ++ | ++ | + | ++ | + | + | + | + | + |
| G2 snapshot | ~ | ++ | ++ | ++ | ~ | + | ~ | + | ~ |
| F1 scoped threads | + | ++ | + | ++ | ++ | + | + | ++ | - |
| H1 workspace | ++ | ++ | + | + | ++ | ++ | + | + | + |

**Reading the matrix.** `@bounded` + `@deterministic` + `C7 manifest` + `D2
planner` is the "provable peak" cluster. `A1/A3 + A6 + D6 + E1/E2` is the
"allocator-as-parameter" cluster. `B6 + C5 + H6` is the AVR cluster. These three
clusters are internally consistent and should be built together.

---

## 11.13 Prioritization

### P0 — High leverage, low risk, do first
1. **C2 poison-on-rewind** (S) — immediate debugging win.
2. **C6 telemetry + C7 manifest** (S/M) — makes the existing model visible.
3. **C8 unbounded-commit warning** (S) — closes a documented leak.
4. **B1 size-class slab / H1 workspace contracts** (M) — provable bounded memory.
5. **A1 + A3 region polymorphism + handles** (M) — substrate for everything below.
6. **C3 rewind audit** (M) — runtime backstop for VNE-070.

### P1 — The numerical differentiators
7. **D6/E2 views + borrows** (M) — zero-copy numerical wrappers.
8. **D1 region-relative pointers** (M) — 2× memory win for graphs/molecules.
9. **G7 scoped interning** (M) — flat parse pipelines.
10. **C1 generation tags** (M) — checked memory safety.
11. **B6 static region + C5 stack budget** (S/M) — embedded.
12. **F1 scoped threads + F4 deterministic reduction** (L) — parallel numerics.

### P2 — Compile-time guarantees and persistence
13. **D2 static memory planner** (L) — compile-time peak.
14. **D3 shape-typed regions** (M) — after shape types (M4).
15. **A4/A5/A6 full escape + subtyping + effects** (L) — the formal layer.
16. **E1/E5 unique values + alias-free regions** (L) — in-place tensors.
17. **G2/G3 snapshot + deterministic replay** (M) — reproducibility.
18. **B8 recycling region** (M) — steady-state zero-alloc.

### Research bets (high payoff, high uncertainty)
- **A2 region inference** — remove manual regions.
- **B2 copying compaction** — fragmentation-free region caches.
- **E3 move/rehome across regions** — zero-copy commit.
- **E4 scoped refcounts / COW** — shared tensors.
- **G4 serialized region images** — mmap-able model/DB snapshots.
- **D5 tiled layout + G6 device regions** — performance/accelerator endgame.

### Explicitly deferred / avoid
- Region inference as the *default* (unpredictable; keep opt-in).
- Non-lexical free (weak refs, cycle collectors) inside a region.
- Mid-region policy switching (breaks checkpoint/rewind symmetry).
- A policy with no concrete allocator behind it.
- Applying generation tags to every typed array in release (kills the ABI).
- Snapshotting regions that contain raw pointers, commits, or FFI handles.

---

## 11.14 Decisions and remaining questions

### Decisions received (this round)

| Decision | Choice | Consequence for this idea set |
| --- | --- | --- |
| **Backend priority** | **Desktop-first** | D1/D2/D5/D6 and the host arena move up. AVR-specific ideas (B6, C5, H6, H7) stay specified but are back-burnered; AVR gets clear "not supported on this target" diagnostics rather than divergence. |
| **Research appetite** | **One or two bets** | Ship the P0/P1 mechanics, plus exactly two bets: **(1) A1 + A3 region handles/polymorphism** and **(2) D2 static memory planner** (the ML showcase needs a compile-time peak number). Defer A2 region inference, B2 compaction, E3 moves, G4 serialized images. |
| **Showcase domain** | **ML / training** | Prioritize B1 size-class slab, H1 workspace contracts, H2 numeric buffer pool, A1/A3, E1/E6 in-place & fused updates, C2/C6/C7 observability, and later F4 deterministic reduction. Deprioritize G7 scoped interning and D1 relative pointers unless sparse/embedding workloads appear. |

### Remaining decisions (non-blocking)

1. **Interpreter scope:** should new memory features (region handles, views,
   unique) work in the interpreter, or be compiled-backend-only with a clear
   diagnostic (as `scratch` does today)? *Recommendation: A1/A3 work in the
   interpreter; D1/D2/C1 are compiled-only.*
2. **Concurrency roadmap:** is single-threaded the assumption for the next year?
   If not, move **F4 deterministic reduction** ahead of F1/F2.
3. **32-bit relative pointers:** acceptable to cap a region block at 4 GB to get
   the ~2× memory win? *Recommendation: yes for desktop `RRef`; `@far` uses
   64-bit when needed.*
4. **Manifest verbosity (C7):** normal builds or only behind a flag? *Recommendation:
   emit a one-line summary always, full JSON only with `--memory-report`.*
5. **Backward compatibility:** may region syntax change (`with region`,
   `region -> value`)? *Recommendation: additive only; keep existing `.vy`
   files compiling unchanged.*

---

## 11.15 Suggested first three experiments (small, testable)

1. **Poison-on-rewind (C2) + rewound-region audit (C3)** — one debug-mode runtime
   change and one diagnostic. Proves the memory model is observable in a week.
2. **Size-class slab behind `@slab` (B1) + workspace contract (H1)** — implement
   the allocator and use it in one `vlin` training step; measure 0 arena bytes
   after warm-up. Proves "bounded memory" as a mechanism.
3. **Region handles prototype in the interpreter (A3)** — expose `Region` as a
   value and pass it to one library function; verify two independent regions
   rewind independently. Proves the allocator-as-parameter direction before
   committing to the emitter change.

---

## 11.16 Relationship to the existing roadmap

- These ideas slot into the milestone plan as follows:
  - **M1:** C2, C3, C6, C7, C8, B5, A7, H4.
  - **M2:** A1, A3, B1, B3, B4, B6, C4, C9, G7, H1, H2, I1, I2, I4.
  - **M3:** D6, D8, G2, G3, G5, H3, H5, I3.
  - **M4:** A4, A5, A6, C1, C10, D2, D3, E1, E2, E5, F1, F2, F4, F5, H8, I5.
  - **M5 (chemistry):** D1 (graph/molecule memory), G7 (parse pipelines), H1/H2
    (force-field workspaces).
  - **M6 (device):** B7, G1, G6, and the `@device` policies from `todo_2.md`.
- The **P0 cluster** is compatible with the "compiler frozen until Paper 1"
  constraint because poison/telemetry/manifest are runtime + diagnostic changes
  that do not alter the benchmark kernel; A1/A3/B1 need a deliberate
  post-submission window.

> **Bottom line.** The existing region model already gives Vyne lifetime
> determinism. The advanced layer is to make the region **parameterizable**
> (A1/A3), **observable** (C2/C3/C6/C7), **size-class allocatable** (B1/H1),
> **relocatable and compact** (D1/D2), **borrowable** (D6/E2), **parallel-safe**
> (F1/F4), and **reproducible** (G2/G3/C9). Those six adjectives are the
> advanced memory model, and every one of them is a natural extension of
> "lifetime is a lexical fact."

---

## 11.17 Selected direction — desktop-first, ML showcase, two bets

Based on the decisions above, this is the recommended execution slice. It is the
subset of §11 most likely to produce a **measurable ML memory-model story**
without destabilizing the compiler before Paper 1 is submitted.

### The two bets

1. **A1 + A3 — region handles + region-polymorphic allocation.** The substrate.
   An ML run needs at least two lifetimes: a **session region** (weights,
   optimizer state, Adam `m`/`v`) and a **per-step region** (activations,
   gradients, scratch). Today both target one global arena; with handles, `vlin`
   and `vml` allocate into the caller's chosen lifetime. This is what makes
   "per-step allocation = 0" expressible.
2. **D2 — static memory planner on a fixed-shape network.** The strongest
   scientific claim in the idea set: for a fixed-shape MLP, the compiler emits a
   `peak_bytes` constant and a `static_assert(peak <= budget)`, and runtime arena
   allocation for the step is zero. Fixed-shape MLPs are the easiest possible
   target (no dynamic shapes, no data-dependent allocation), which makes the bet
   tractable.

### Phased plan (≈10–12 weeks, desktop-first)

| Phase | Ideas | Deliverable | Acceptance |
| --- | --- | --- | --- |
| **0** (safe now) | C2, C6, C7, C8, H4 | Poison-on-rewind, per-region telemetry + one-line manifest, unbounded-commit warning, BLAS workspace allocation from a region | A training run prints a per-region peak manifest; a use-after-rewind produces NaN/trap; commit growth in an unbounded loop is warned |
| **1** (weeks 1–3) | B1, H1, H2 | `@slab` size-class allocator; `workspace_bytes(n)` contracts; numeric buffer pool | A training step allocates **0 arena bytes after warm-up**; class sizes and waste reported in the manifest |
| **2** (weeks 3–7) | A1, A3, I1, A6-min | First-class `Region`; region-polymorphic `vlin`/`vml` ops; `with region { ... }`; minimal `allocates(r)` effect | Two interleaved session/step regions rewind independently; `vlin.multiply(r, a, b)` allocates into `r`; interpreter and C backend agree |
| **3** (weeks 6–9) | E1-min, E6, C4 | `unique`/`inout` prototype for `W -= lr*grad`; `@budget` | The SGD update mutates in place with no copy and no alias check; budget is enforced |
| **4** (weeks 8–12) | **D2 bet** | Static planner for a fixed-shape MLP; emitted `peak_bytes` + `static_assert` | Step allocation is a **compile-time number**; runtime arena bytes = 0; peak proven ≤ budget |

### ML memory-model acceptance metrics

- **Session vs step:** weights and Adam state live in a session region; a step
  region rewinds to the same offset every iteration.
- **Zero steady-state allocation:** after the first step, `arena_alloc` calls per
  training step = 0.
- **Compile-time peak:** fixed-shape MLP reports `peak_bytes` at compile time and
  emits a `static_assert` against its budget.
- **In-place updates:** `W -= lr*grad` allocates nothing and writes through the
  existing buffer (E1/E6), with no defensive copy.
- **Optimizer state reuse:** Adam `m`/`v` allocated once via `@slab`/`@pool`,
  never reallocated per step.
- **Manifest clean:** per-step allocations and commit-arena growth both read 0.
- **Later (F4):** parallel training produces bit-identical results across worker
  counts.

### Deliberately out of the first slice

- **G7 scoped interning** and **D1 region-relative pointers** — valuable for
  chemistry/parsing, not central to an ML showcase. Revisit if sparse matrices or
  embedding tables become the ML focus.
- **F1/F2 parallel regions** — land F4's deterministic reduction first if
  parallel training is wanted.
- **G2/G3 snapshot/replay**, **G4 serialized images**, **M5 chemistry**, **M6
  device** — later milestones.
- **D5 tiling** — a performance idea, not a memory-model idea; leave to the M1/M3
  optimization track.

### Risks specific to this slice

- **Emitter churn from A1/A3** can perturb the frozen benchmark code. Gate phases
  2–4 behind Paper 1 submission; phase 0 is safe because it does not change hot
  kernel lowering.
- **D2 is shape-static only.** Document the disqualifiers (dynamic shapes,
  recursion, data-dependent allocation) and fall back to `@bounded` with an honest
  manifest entry.
- **E1 uniqueness is large.** Keep the first cut to one update rule and one
  signature; do not attempt a general linear type system in this window.
- **Slab waste** can hide memory cost. Expose `class_of(shape)`, report rounding
  waste, and make `workspace_bytes` the caller's contract so over-allocation is
  visible.
