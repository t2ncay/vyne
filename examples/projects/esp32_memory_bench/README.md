# ESP32 memory wall: TFLM versus bounded Vyne

A reproducible **Float32 64 → 32 → 10 MLP** comparison on the same classic
ESP32, with ReLU after the hidden layer and linear output logits. This project
is evidence for a blog post, not a preselected winner. Random-data training
provides a valid shared model; classification accuracy is not the topic.

**Measured finding:** the current Vyne ESP32 path is smaller in flash but much
slower than official bundled TFLM. `@bounded` and fixed-size scratch do not
automatically make inference faster or shrink the default runtime arena.

## Project layout

```text
esp32_memory_bench/
  project.toml                 # hw950 target and checked benchmark contract
  hardware.toml                # COM5, 115200 baud
  dependencies.json            # baseline, Arduino/core pin, bundled TFLM hash
  requirements.txt             # primary Python export dependencies
  requirements.lock            # tested full Python 3.13 environment
  data/README.md               # generated VWT/TFLite/header format
  scripts/setup.py             # isolated Python environment; core verification
  scripts/train_and_export.py  # PyTorch training and verified dual export
  scripts/bench.py             # stage/build/upload/capture/report/archive
  src_vyne/bench.vy            # bounded scratch, actual Vyne MLP loops
  src_vyne/data_bridge.vy      # fail-closed data/reporting ABI declarations
  src_tflm/esp32_memory_bench.ino
  shared/report.h              # common output sink, validation and telemetry
  shared/float32_compat.h      # project-local missing MCU scalar helpers
  tests/                       # hostile-capture, export and emitted-code contracts
  build/                       # ignored environments, ELF files, logs, reports
```

## Reproduce

Use repository baseline `0b4fd8355e14dfe5f40c45cee618885a9b17ce76` plus this
branch's small embedded emit-only correction and benchmark sources. Rebuild
the compiler (`xmake -y` from repository root). Merely using an installed 0.8.0
binary is insufficient: emission must retain the ESP32 target when the final
mode is `--emit-c`.

Run the commands below **from this project directory**. Python 3.13 is the
tested training/export environment; the standard-library runner also works
with Python 3.14. `VYNEC` or `--compiler /path/to/vynec` selects another freshly
built compiler. On Windows, the virtual environment executable is
`build\venv\Scripts\python.exe`; on Linux it is `build/venv/bin/python`.

```powershell
# Installed here: Arduino CLI 1.5.1, esp32:esp32 core 3.3.12.
arduino-cli core install esp32:esp32@3.3.12
python scripts/setup.py --tflm

# Replace this with your Python 3.13 executable path.
python scripts/setup.py --export-env --python "C:\path\to\Python313\python.exe"
.\build\venv\Scripts\python.exe scripts/train_and_export.py

# Optional independent-process byte reproducibility check.
.\build\venv\Scripts\python.exe scripts/train_and_export.py --output build/reexport
python tests/check_export_repro.py data build/reexport

# Contracts; do not omit export tests by silently skipping dependencies.
.\build\venv\Scripts\python.exe -m unittest discover -s tests -v

# Default reservations: TFLM 16 KiB; Vyne 64 KiB.
python scripts/bench.py run --profile default --implementation both

# Explicitly sized/headroom configuration: both reserve 1 KiB arenas.
python scripts/bench.py run --profile sized --implementation both

# Repeat each profile in reversed runtime order to assess order effects.
python scripts/bench.py run --profile default --implementation vyne
python scripts/bench.py run --profile default --implementation tflm
python scripts/bench.py run --profile sized --implementation vyne
python scripts/bench.py run --profile sized --implementation tflm

python scripts/bench.py report --update-readme
```

`stage` prepares sketches without invoking the board toolchain; `build` stops
after compiling. `run` uploads **both firmwares sequentially to COM5** and
resets/captures the board. Override the connection with `--port`. It leaves
the last benchmark firmware idle on the board; it does not restore the plotter.

For a deliberately insufficient TFLM arena:

```powershell
python scripts/bench.py run --profile sized --implementation tflm --tflm-arena 896
# Or assert the exact physical failure and absence of accepted samples:
python tests/check_arena_failure.py
```

Expect an initialization failure, a nonzero runner exit, a retained raw log and
`failed-*.json`, and **no successful result**. Afterward rebuild the regular
sized configuration. To change model/source/config/compiler versions, first use
`python scripts/bench.py archive`. This preserves prior build directories and
model artifacts under `build/archive/`; reporting refuses mixed source, model,
compiler, hardware or TFLM binary identities.

`project.toml` follows the `plotter-esp32` hw950 target/bench pattern.
`hardware.toml` reuses its board and connection. The runner reads both and tests
their consistency; source location and staged weight integration require this
runner rather than a generic `vynec run` project entry point. The firmware
contract fixes CPU=240 MHz, warmup=10, batches=20 and iterations=100. Unsupported
TOML changes fail rather than pretend to alter hardcoded firmware. Display and
Wi-Fi/BT are not initialized; PSRAM is disabled in the FQBN and verified as zero
by device telemetry. `hw950` does not imply a display workload here.

## Export and numerical contract

- Seed `20261117`; 512 synthetic training examples; five deterministic CPU
  Adam epochs; 2,410 Float32 parameters (9,640 bytes of weight/bias payload).
- TFLite conversion is **not** a direct PyTorch conversion: an equivalent Keras
  graph receives the reread VWT weights, with output-major → input-major
  transposition for Dense weights. No retraining or quantization occurs.
- VWT roundtrip is exact at Float32 precision. PyTorch, Keras and desktop TFLite
  must agree on eight 64-element vectors at **atol=2e-5, rtol=2e-4**.
- Verify all export tensor dtypes, operator allowlist/version and artifact
  SHA256 values. In the tested export, hidden ReLU is fused into the first
  FullyConnected operator; two FC nodes execute. A standalone ReLU registration
  remains available if the converter emits it separately.
- Headers contain const flash-resident data. Both deployments share the same
  Float32 model and test inputs. TFLM uses the FlatBuffer weights; Vyne uses
  output-major arrays generated by rereading `model.vwt`.

### Explicit Vyne integration costs

Current Vyne has neither embedded `vfs` loading nor project-local native C
bindings. The runner emits **ESP32-targeted code** and replaces only the named
data/reporting stub function bodies with checked flash reads and common
instrumentation. `bridge-audit.json` records hashes and verifies the entire
`vyne_setup()` inference body is **byte-for-byte unchanged**. Unexpected ABI
changes, missing/duplicate stubs and reapplication fail closed. Boxed calls to
the bridge fail rather than accepting the known boxed Float32 parameter gap.

The MCU runtime lacks `vyne_float32` and `vyne_to_float32`; a short project-local
header supplies Float32-rounding scalar constructors. It does not alter the
compiler's arithmetic lowering or general array ABI. Generated code still
contains boxed/double intermediate operations, and its cost is part of this
benchmark. This project is **not** a standalone `.vwt` device loader and is not
evidence of a production-ready Float32 ML library.

## Measurement protocol

Each cold-reset run performs ten warmups and twenty batches of 100 inferences.
The same eight input vectors cycle in the same order. The timed interval uses
the 64-bit `esp_timer_get_time()` clock; Vyne's `esp32.timer_read()` binding
reaches that same clock, avoiding `micros()` rollover. Timing includes input
copy, computation and ten volatile output stores per inference on both sides.
No serial output, reference comparison, export or device filesystem I/O occurs
inside a timed batch. The output sink prevents dead-code elimination.

All eight vectors/all ten logits are validated in an untimed phase before any
sample is published. The runner checks the model hash, runtime, CPU, PSRAM,
capacity, checked count, finite error, batch uniqueness/count, positive elapsed
times and completion. Panics, initialization/invocation errors, partial captures
and timeouts cannot become successful performance samples. The reported
microseconds/inference are batch duration / 100, **not** 2,000 independent
per-inference measurements; min/max describe batch means. Repeated cold runs
also matter. No wall-clock hard-realtime guarantee is inferred.

### Memory definitions — keep these separate

| Metric | What it includes | What it excludes |
| --- | --- | --- |
| Configured arena capacity | Compile-time tensor/runtime buffer definition | Whether unused storage survives linker optimization |
| Linked arena bytes | Actual buffer symbol size in the final ELF, measured with `nm -S -C` | Scratch/task stacks, runtime globals, other heap users |
| Arena used/peak | TFLM planner's used bytes after `AllocateTensors`; Vyne allocator lifetime peak | Vyne stack scratch and unused reserved capacity |
| Vyne scratch | `sizeof(float) × (64+32+10) = 424` bytes | Other function locals, stack frames, arena, globals |
| Static DRAM data/BSS | ELF `.dram*.data`, `.dram*.bss`, `.noinit` section totals | Task stacks allocated from heap, IRAM, other system memory |
| Task-stack free minimum | ESP-IDF high-water mark in bytes, read before serial reporting | Total SRAM or instantaneous stack usage |
| Internal heap free/min/largest | ESP-IDF internal 8-bit-capable heap telemetry | Reserved static arena and all other SRAM accounting |
| Sketch size | `ESP.getSketchSize()` application image bytes | Model payload alone, SRAM, bootloader/partition overhead |

Unused storage is allowed to disappear through normal compiler/linker optimization;
there is no artificial arena pointer escape or forced reservation. The runner
measures the actual buffer symbol separately from configured capacity. In these
tested images the Vyne arena remains linked at its configured size even though
the workload allocates no arena payload. Common telemetry storage is present on
both sides.
Task stacks are already reserved from heap; adding every counter double-counts
memory. **Total device peak SRAM is not measured by this project.** Linker
data/BSS, task-stack high-water and heap observations are complementary evidence,
not a universal single peak. Build logs, expanded properties, all ELF sections,
compiler/model/library hashes, configuration and command lines are retained.

Sketch/runtime compilation uses `-ffp-contract=off`. This flag does **not**
recompile the bundled prebuilt TFLM archive; its upstream build configuration is
retained and its exact binary is pinned. Output agreement is tolerance-based,
not a bitwise-equivalent floating-point instruction contract.

## Comparison table and analysis

| Runtime | Handwritten LOC | Memory strategy | Actual peak memory | Latency | Determinism |
| --- | --- | --- | --- | --- | --- |
| TFLM | 62 firmware + 46 shared reporting | Static manually sized tensor arena; planner and persistent interpreter metadata | 904 B arena used; separate SRAM/stack/heap metrics below; no total peak claim | 0.077035 ms median (sized) | Fixed model, planned allocation; OS/cache interrupts still affect time |
| Vyne | 52 firmware + 11 stubs + 9 compatibility + 46 shared reporting | Lexically bounded stack scratch, separate static runtime arena | 424 B scratch + 0 B arena payload peak; no total peak claim | 3.101680 ms median (sized) | Explicit lexical storage/budget; no wall-clock determinism claim |

LOC counts nonblank lines excluding whole-line language comments; C/C++
preprocessor lines and braces remain counted.
Include the data bridge, compatibility code and Python staging machinery when
discussing engineering effort; generated arrays, third-party library lines and
shared reporting must be identified separately. **Less boilerplate is a
hypothesis, not established by counting only the small Vyne loop.**

TFLM's arena is statically allocated in this sketch. Initialization plans
activation reuse and allocates metadata; the caller supplies capacity. A too-small
capacity fails during `AllocateTensors`, but sizing need not be blind trial and
error: `arena_used_bytes()` and recording/planning facilities provide evidence.
Do not call it a conventional fragmented malloc heap or imply it reallocates
on every inference. Official ESP-NN-capable bundled kernels are used unchanged;
this Float32 benchmark does not generalize to optimized Int8 models.

Vyne statically sizes the scratch arrays and reclaims regions lexically.
`@bounded<848>` is the tight **current compiler-accounted** scratch budget;
the actual three Float32 buffers occupy **424 bytes**. Static scratch is charged
at eight bytes/element by current codegen. Tests confirm VNE-101 rejects declared
static scratch over the budget. Dynamic/VLA and arena-backed paths have runtime
checks and known roadmap limitations: this is not a universal compile-time
SRAM proof. `arena_peak()==0` here means no arena payload allocated, **not zero
memory**. `@bounded` does not resize the 64 KiB MCU arena. Our explicit 1 KiB
profile changes the compile-time arena definition on purpose; it is a headroom
configuration, not a proven minimum.

Fixed scratch and monotonic region allocation avoid per-object heap fragmentation
within this workload. TFLM's fixed arena also avoids that conventional heap
fragmentation. Neither statement guarantees fragmentation-free system heap,
zero runtime overhead or lower latency. A win in one metric cannot explain another
without isolating causes. Compare default/default and explicit-sized/explicit-sized
configurations, not a tuned Vyne build against an oversized TFLM buffer.

<!-- BENCH_RESULTS_BEGIN -->

## Measured device results

Raw times are batch durations / 100, not independently timed inference calls.

| Profile | Runtime | Runs/batches | Configured arena B | Linked arena B | Arena used/peak B | Scratch B | Static DRAM data/BSS B | Median ms/inference (min–max) | Flash B |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- | ---: |
| default | tflm | 2/40 | 16384 | 16384 | 904 | 0 | 39060 | 0.077030 (0.076990–0.077060) | 322272 |
| default | vyne | 2/40 | 65536 | 65536 | 0 | 424 | 87860 | 3.091650 (3.091600–3.091710) | 284944 |
| sized | tflm | 3/60 | 1024 | 1024 | 904 | 0 | 23700 | 0.077035 (0.076990–0.077060) | 322272 |
| sized | vyne | 3/60 | 1024 | 1024 | 0 | 424 | 23348 | 3.101680 (3.101630–3.101760) | 284928 |

All accepted runs checked 80 outputs at atol=2e-5, rtol=2e-4. Configured capacity can differ from linked storage. Arena high-water and scratch bytes overlap with total SRAM and must not be blindly summed.

Source LOC (nonblank, whole-line language comments excluded; braces and C/C++ preprocessor lines counted): {"common_reporting": 46, "staging_runner": 396, "tflm_firmware": 62, "vyne_compat": 9, "vyne_firmware": 52, "vyne_stubs": 11}

Failed captures excluded: 1.

### Capture provenance

- `build\default\tflm\capture-20261117T002443738425.log`: model `5b5ed844710c6c886e82c597e4ce30bcfdcd4d5a4059bd5f425d946070998f4e`, capture SHA256 `6046bd41bebd9ba5c6c29e647346ccd169fc049020f79f8e20c0422182e0403d`, max absolute error `1.34110451e-07`, stack free minimum `6720` B, internal heap free/minimum/largest `246500/214836/110580` B, task stack reserved `8192` B, setup `707 us`. Build metadata and ELF sections are retained alongside the capture.
- `build\default\tflm\capture-20261117T002800164920.log`: model `5b5ed844710c6c886e82c597e4ce30bcfdcd4d5a4059bd5f425d946070998f4e`, capture SHA256 `22b52f691234df5b31cb82763c4fa946bd4af55b9604de8c91e95c3b6beb6239`, max absolute error `1.34110451e-07`, stack free minimum `6688` B, internal heap free/minimum/largest `246500/214836/110580` B, task stack reserved `8192` B, setup `706 us`. Build metadata and ELF sections are retained alongside the capture.
- `build\default\vyne\capture-20261117T002525480789.log`: model `5b5ed844710c6c886e82c597e4ce30bcfdcd4d5a4059bd5f425d946070998f4e`, capture SHA256 `f6d327cd02c40f3c70951c8764ef17ea6446b41b7894dd448ab0a1b3630d45c8`, max absolute error `1.1920929e-07`, stack free minimum `6748` B, internal heap free/minimum/largest `197992/166112/110580` B, task stack reserved `8192` B, setup `not measured`. Build metadata and ELF sections are retained alongside the capture.
- `build\default\vyne\capture-20261117T002730548702.log`: model `5b5ed844710c6c886e82c597e4ce30bcfdcd4d5a4059bd5f425d946070998f4e`, capture SHA256 `fbe82fac9cc96c456b258492281503dfc33e517c91925671bd93c6c7d140d0ef`, max absolute error `1.1920929e-07`, stack free minimum `6748` B, internal heap free/minimum/largest `197992/166112/110580` B, task stack reserved `8192` B, setup `not measured`. Build metadata and ELF sections are retained alongside the capture.
- `build\sized\tflm\capture-20261117T002619081711.log`: model `5b5ed844710c6c886e82c597e4ce30bcfdcd4d5a4059bd5f425d946070998f4e`, capture SHA256 `f6fa7f46085936bfa5d7419d0efba119a0ab1b4094e821ad0f7f4ba9c678f7ac`, max absolute error `1.34110451e-07`, stack free minimum `6720` B, internal heap free/minimum/largest `261860/230196/110580` B, task stack reserved `8192` B, setup `712 us`. Build metadata and ELF sections are retained alongside the capture.
- `build\sized\tflm\capture-20261117T002853081656.log`: model `5b5ed844710c6c886e82c597e4ce30bcfdcd4d5a4059bd5f425d946070998f4e`, capture SHA256 `d8f32a06cda65f31712bb30a36d7165a54c8302e6627caa7176e5a71a3c7fc2f`, max absolute error `1.34110451e-07`, stack free minimum `6720` B, internal heap free/minimum/largest `261860/230196/110580` B, task stack reserved `8192` B, setup `711 us`. Build metadata and ELF sections are retained alongside the capture.
- `build\sized\tflm\capture-20261117T003024164417.log`: model `5b5ed844710c6c886e82c597e4ce30bcfdcd4d5a4059bd5f425d946070998f4e`, capture SHA256 `91326bcfd9bb8f594f34bacce163691ed84508e6897e7823e60cb5ebdb472afc`, max absolute error `1.34110451e-07`, stack free minimum `6720` B, internal heap free/minimum/largest `261860/230196/110580` B, task stack reserved `8192` B, setup `711 us`. Build metadata and ELF sections are retained alongside the capture.
- `build\sized\vyne\capture-20261117T002701148835.log`: model `5b5ed844710c6c886e82c597e4ce30bcfdcd4d5a4059bd5f425d946070998f4e`, capture SHA256 `a27b7557a070eb639c824edf2fe044492a3d6a4b8bc792ed00fc86302102fe8d`, max absolute error `1.1920929e-07`, stack free minimum `6720` B, internal heap free/minimum/largest `262436/230556/110580` B, task stack reserved `8192` B, setup `not measured`. Build metadata and ELF sections are retained alongside the capture.
- `build\sized\vyne\capture-20261117T002823290540.log`: model `5b5ed844710c6c886e82c597e4ce30bcfdcd4d5a4059bd5f425d946070998f4e`, capture SHA256 `e221a35f6b96c3d1b8817abfe116aeebfcdc09de2b5d0f19d2b66abf69c47773`, max absolute error `1.1920929e-07`, stack free minimum `6748` B, internal heap free/minimum/largest `262436/230556/110580` B, task stack reserved `8192` B, setup `not measured`. Build metadata and ELF sections are retained alongside the capture.
- `build\sized\vyne\capture-20261117T003047477135.log`: model `5b5ed844710c6c886e82c597e4ce30bcfdcd4d5a4059bd5f425d946070998f4e`, capture SHA256 `3e16080d0add456520e66d74cecc89bd1809383a95556f87ad6747a993c8fbe9`, max absolute error `1.1920929e-07`, stack free minimum `6748` B, internal heap free/minimum/largest `262436/230556/110580` B, task stack reserved `8192` B, setup `not measured`. Build metadata and ELF sections are retained alongside the capture.

<!-- BENCH_RESULTS_END -->

## Validation catalog

- Seeded real export with exact VWT roundtrip and independent cross-framework
  output checks, plus corrupt shape/name/header/nonfinite VWT tests.
- Host capture adversaries: missing/duplicate metadata/batches/completion, wrong
  model/runtime/CPU, failed firmware, invalid memory/time/PSRAM/stack/error fields.
- Project/hardware consistency; AVR and ESP32 emit-only target preservation;
  over-budget static scratch rejection; generated inference body preservation
  and rejection of altered/reapplied data bridge ABI.
- Physical COM5 compile/upload/capture for both default and 1 KiB profiles.
  The final result section distinguishes executed tests from future experiments.
- Meaningful future checks: recorded per-inference distribution, energy/thermal
  measurements, Int8/ESP-NN and additional model shapes, isolated native Float32
  lowering fixes, extended long-run allocation tests and broader boards.

### Executed evidence and interpretation (2026-11-17 UTC)

- Compiler release build: `xmake -y` passed. Existing registered regressions:
  `xmake test-all` **21/21** passed. Benchmark unittest suite **16/16** passed;
  **14 standard-library contracts** are wired into both CI compiler profiles,
  with the two NumPy export-format tests executed locally in the pinned environment.
- Two independent Python 3.13 exports produced byte-identical VWT, FlatBuffer,
  model header and shared data header. Export maximum absolute error:
  **1.043081283569336e-7**. Model hash:
  `5b5ed844710c6c886e82c597e4ce30bcfdcd4d5a4059bd5f425d946070998f4e`.
- **10 accepted cold-reset COM5 captures**, 20 batches each: **20,000 timed
  inferences**, plus 800 validated reference outputs across captures. Default
  profiles have two runs/runtime; sized profiles have three runs/runtime.
  Runtime order was reversed on the second pass. Maximum device error:
  TFLM **1.34110451e-7**, Vyne **1.1920929e-7**.
- Successful firmware sources are from `caa738e`, including the explicit native
  arena-peak binding and natural linked-symbol accounting; the fresh compiler binary SHA256 is
  `5f2c104e20cdd002942ae714d2daa52eb63e096180bf7a230228f2590cc642aa`.
  The result-only formatting update does not alter firmware. Build snapshots
  retain source hashes, full revisions and dirty-tree state; report grouping
  verifies identical measured source/compiler/model/library/FQBN identities.
- TFLM's **896-byte** negative run failed specifically at `AllocateTensors`;
  its raw log and failure metadata were retained, and no successful sample
  was added. A 1,024-byte configuration passed using **904 bytes**. This brackets
  this model/configuration's requirement; it is not a general minimum proof.
- An earlier rejected Vyne capture showed the fail-closed policy when direct passing of
  `arena_peak()` selected the boxed reporting fallback. It was archived under
  `build/archive/20261117T001319313192/` and excluded after the explicit Int64
  binding fix. Source/measurement revisions are never pooled.
- The earlier pointer-escape instrumentation was removed and its captures
  archived under `build/archive/20261117T002340098074/`. The final ten accepted
  captures use natural linking and record `symbols.txt`; both tested runtime
  arena symbols retain their configured capacities. Those earlier captures do
  not enter the final tables.
- **Latency:** sized Vyne is about **40.26× slower** than TFLM here. This is not
  evidence for lower latency. Generated boxed/double intermediates are an
  observed candidate cause; this benchmark does not isolate their individual cost.
- **Flash:** sized Vyne's image is **37,344 bytes smaller** (~11.59%).
  **Sized static DRAM data/BSS:** Vyne saves **352 bytes** (~1.49%), with stack
  storage measured separately. **Default static DRAM data/BSS:** Vyne reserves
  **48,800 bytes more** because of its much larger default arena.
- No total peak-SRAM or hard-realtime claim is supported. Arena payload and
  static DRAM savings cannot be used interchangeably. Smaller flash does not
  establish less handwritten boilerplate; the supported Vyne path includes
  stubs, compatibility and a substantial staging runner.
- The physical board currently contains the successful sized Vyne benchmark
  firmware and idles after reporting. The previous Wi-Fi plotter was not restored.

Raw model files, environments, ELF images and machine-local logs stay ignored in
Git. This README records the measured summaries and capture hashes; preserve
`build/bench.json`, capture logs, ELF section tables and generated `data/` together
when distributing the complete blog evidence bundle.

## Blog outline (four sections)

Working title: **“The ESP32 Memory Wall: Why I Ditched TFLM's Dynamic Arena for
Vyne's Static Regions.”** The word “dynamic” and the claimed switch are hypotheses
to revise before publication. An evidence-led title for the current results is
**“The ESP32 Memory Wall: What Bounded Vyne Actually Saves—and What TFLM Wins.”**

1. **The wall and a falsifiable experiment.** Classic ESP32 SRAM constraints;
   identical 64→32→10 Float32 MLP, flash weights, pinned board/toolchain, correctness
   first, and why a synthetic model avoids training-task confounders.
2. **Two fixed buffers, different orchestration.** Show the TFLM lifecycle and
   planner, Vyne scratch/region lowering, default reserved capacity, VNE-101,
   conservative Float32 accounting and the cost of the explicit integration bridge.
3. **The numbers, including inconvenient results.** Default and sized DRAM,
   arena versus stack, flash image, batch latencies, repeated cold runs, failed
   initialization evidence and honest LOC. Explain why current boxed Float32
   lowering is a latency bottleneck without inventing measured optimization gains.
4. **What I would ship and what I would fix.** Choose by measured requirements;
   discuss proper arena sizing, native Float32 lowering, future Int8 tests and
   missing full-SRAM/energy/long-run evidence. Link commands, raw artifacts and PR.
