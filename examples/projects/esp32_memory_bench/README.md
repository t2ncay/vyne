# ESP32 memory wall: TFLM versus bounded Vyne

A reproducible **Float32 64 → 32 → 10 MLP** comparison on the same classic
ESP32, with ReLU after the hidden layer and linear output logits. This project
is evidence for a blog post, not a preselected winner. Random-data training
provides a valid shared model; classification accuracy is not the topic.

**Initial finding:** the current Vyne ESP32 path is smaller in flash but much
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
| Reserved arena | Actual static tensor/runtime buffer capacity | Scratch/task stacks, runtime globals, other heap users |
| Arena used/peak | TFLM planner's used bytes after `AllocateTensors`; Vyne allocator lifetime peak | Vyne stack scratch and unused reserved capacity |
| Vyne scratch | `sizeof(float) × (64+32+10) = 424` bytes | Other function locals, stack frames, arena, globals |
| Static DRAM data/BSS | ELF `.dram*.data`, `.dram*.bss`, `.noinit` section totals | Task stacks allocated from heap, IRAM, other system memory |
| Task-stack free minimum | ESP-IDF high-water mark in bytes, read before serial reporting | Total SRAM or instantaneous stack usage |
| Internal heap free/min/largest | ESP-IDF internal 8-bit-capable heap telemetry | Reserved static arena and all other SRAM accounting |
| Sketch size | `ESP.getSketchSize()` application image bytes | Model payload alone, SRAM, bootloader/partition overhead |

The `.bss` arena is forced to remain in the linked Vyne image even when inference
uses no arena payload; otherwise compiler elimination could falsely make a
64 KiB default look free. Common telemetry storage is present on both sides.
Task stacks are already reserved from heap; adding every counter double-counts
memory. **Total device peak SRAM is not measured by this project.** Linker
data/BSS, task-stack high-water and heap observations are complementary evidence,
not a universal single peak. Build logs, expanded properties, all ELF sections,
compiler/model/library hashes, configuration and command lines are retained.

## Comparison table and analysis

| Runtime | Handwritten LOC | Memory strategy | Actual peak memory | Latency | Determinism |
| --- | --- | --- | --- | --- | --- |
| TFLM | See measured LOC, plus common reporting | Static manually sized tensor arena; planner and persistent interpreter metadata | Arena used + separate SRAM/stack/heap metrics below; no total peak claim | Measured below | Fixed model, planned allocation; OS/cache interrupts still affect time |
| Vyne | See measured firmware + stubs + compatibility + common reporting | Lexically bounded stack scratch, separate static runtime arena | 424 B scratch plus arena peak; no total peak claim | Measured below | Explicit lexical storage/budget; no wall-clock determinism claim |

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

Device measurements pending final repeated/default/sized runs. Export numerical
checks and initial COM5 execution passed; no final comparison is asserted here.

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
