# ESP32 memory wall: TFLM versus bounded Vyne

A reproducible **Float32 64 → 32 → 10 MLP** comparison on the same classic
ESP32, with ReLU after the hidden layer and linear output logits. This project
is evidence for a blog post, not a preselected winner. Random-data training
provides a valid shared model; classification accuracy is not the topic.

**Measured finding after the native-conversion codegen fix:** Vyne now takes
**0.132350 ms/inference**, versus **0.077040 ms** for official bundled TFLM.
That is about **23.4× faster than the previous Vyne build**, although TFLM remains
about **1.72× faster**. Vyne uses less linked static DRAM and flash in this workload.
The original baseline is retained below; weights remain in flash and the MLP
source, bounds checks, model and timing protocol are unchanged.

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
mode is `--emit-c`, and the native Float32/Float64 conversion patch described
below must be present to reproduce the updated results.

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

python tests/check_codegen_assembly.py
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
header supplies Float32-rounding scalar constructors at boxed boundaries. The
compiler fix now materializes native numeric conversions into typed temporaries
and keeps known Float32 arithmetic on the existing native path. Runtime
conversion helpers remain for dynamic/string/boolean inputs. The benchmark's
multiply-accumulate loops no longer contain boxed/double intermediate operations;
the data bridge still performs no inference arithmetic. General array ABI and
interpreter conversion discrepancies are outside this fix. This project is
**not** a standalone `.vwt` device loader or a production-ready Float32 ML library.

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
baseline images the Vyne arena remained linked at its configured size even though
the workload allocated no arena payload. After native lowering removes the hot
generic-dispatch references, the unused arena is eliminated in both Vyne profiles:
**0 linked bytes**, despite configured capacities of 65,536 and 1,024 bytes.
This is normal dead-storage elimination for this workload, not automatic
`@bounded` resizing or a guarantee for programs that actually use the arena.
Common telemetry storage is present on both sides.
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
| TFLM | 62 firmware + 46 shared reporting | Static manually sized tensor arena; planner and persistent interpreter metadata | 904 B arena used; separate SRAM/stack/heap metrics below; no total peak claim | 0.077040 ms median (sized) | Fixed model, planned allocation; OS/cache interrupts still affect time |
| Vyne | 52 firmware + 11 stubs + 9 compatibility + 46 shared reporting | Lexically bounded stack scratch; unused runtime arena eliminated | 424 B scratch + 0 B arena payload peak; no total peak claim | 0.132350 ms median (sized) | Explicit lexical storage/budget; no wall-clock determinism claim |

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

### Preserved baseline before native-conversion lowering

Measured source baseline: `caa738e` (documented in `6d353f5`), compiler SHA256
`5f2c104e20cdd002942ae714d2daa52eb63e096180bf7a230228f2590cc642aa`.
These are the original ten accepted captures, separate from the ten updated
captures below. The complete original JSON, Markdown, capture logs, ELF files,
model data and compiler binary were preserved under
`build/archive/20261117T004605439342/`. Archived result paths are relative to
the original project layout; resolve their `default/` and `sized/` directories
under that archive. No old/new samples are pooled.

| Profile | Runtime | Runs/batches | Configured/linked arena B | Arena used/peak B | Scratch B | Static DRAM data/BSS B | Median ms/inference (min–max) | Flash B |
| --- | --- | --- | --- | ---: | ---: | ---: | --- | ---: |
| default | TFLM | 2/40 | 16384/16384 | 904 | 0 | 39060 | 0.077030 (0.076990–0.077060) | 322272 |
| default | Vyne | 2/40 | 65536/65536 | 0 | 424 | 87860 | 3.091650 (3.091600–3.091710) | 284944 |
| sized | TFLM | 3/60 | 1024/1024 | 904 | 0 | 23700 | 0.077035 (0.076990–0.077060) | 322272 |
| sized | Vyne | 3/60 | 1024/1024 | 0 | 424 | 23348 | 3.101680 (3.101630–3.101760) | 284928 |

All baseline runs validated 80 outputs each. Maximum errors were
`1.34110451e-7` (TFLM) and `1.1920929e-7` (Vyne). Task stack capacity was 8,192 B.
Baseline internal heap free/minimum/largest values:

| Profile/runtime | Heap free/minimum/largest B | Stack minimum free B |
| --- | --- | --- |
| default/TFLM | 246500/214836/110580 | 6688–6720 |
| default/Vyne | 197992/166112/110580 | 6748 |
| sized/TFLM | 261860/230196/110580 | 6720 |
| sized/Vyne | 262436/230556/110580 | 6720–6748 |

<!-- BENCH_RESULTS_BEGIN -->

## Measured device results

Raw times are batch durations / 100, not independently timed inference calls.

| Profile | Runtime | Runs/batches | Configured arena B | Linked arena B | Arena used/peak B | Scratch B | Static DRAM data/BSS B | Median ms/inference (min–max) | Flash B |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- | ---: |
| default | tflm | 2/40 | 16384 | 16384 | 904 | 0 | 39060 | 0.077040 (0.076990–0.077060) | 322272 |
| default | vyne | 2/40 | 65536 | 0 | 0 | 424 | 22324 | 0.132350 (0.132340–0.132390) | 283424 |
| sized | tflm | 3/60 | 1024 | 1024 | 904 | 0 | 23700 | 0.077040 (0.076990–0.077090) | 322272 |
| sized | vyne | 3/60 | 1024 | 0 | 0 | 424 | 22324 | 0.132350 (0.132340–0.132390) | 283424 |

All accepted runs checked 80 outputs at atol=2e-5, rtol=2e-4. Configured capacity can differ from linked storage. Arena high-water and scratch bytes overlap with total SRAM and must not be blindly summed.

Source LOC (nonblank, whole-line language comments excluded; braces and C/C++ preprocessor lines counted): {"common_reporting": 46, "staging_runner": 401, "tflm_firmware": 62, "vyne_compat": 9, "vyne_firmware": 52, "vyne_stubs": 11}

Failed captures excluded: 1.

### Capture provenance

- `build\default\tflm\capture-20261117T005019152670.log`: model `5b5ed844710c6c886e82c597e4ce30bcfdcd4d5a4059bd5f425d946070998f4e`, capture SHA256 `85ebebf0296f26ae7e6e00560a1395c095d56563ba7753ad256794ec63554a97`, max absolute error `1.34110451e-07`, stack free minimum `6688` B, internal heap free/minimum/largest `246500/214836/110580` B, task stack reserved `8192` B, setup `707 us`. Build metadata and ELF sections are retained alongside the capture.
- `build\default\tflm\capture-20261117T005344091895.log`: model `5b5ed844710c6c886e82c597e4ce30bcfdcd4d5a4059bd5f425d946070998f4e`, capture SHA256 `d6bcae76445496cc5fd456defa9f9e87b9b0dc2d01a34ad0893d2e071ddb3569`, max absolute error `1.34110451e-07`, stack free minimum `6688` B, internal heap free/minimum/largest `246500/214836/110580` B, task stack reserved `8192` B, setup `706 us`. Build metadata and ELF sections are retained alongside the capture.
- `build\default\vyne\capture-20261117T005101266274.log`: model `5b5ed844710c6c886e82c597e4ce30bcfdcd4d5a4059bd5f425d946070998f4e`, capture SHA256 `322e4b9b98c92ca2731eb19400eca228f693f6b2cdd5d3338a990a8dfb15174f`, max absolute error `1.1920929e-07`, stack free minimum `7036` B, internal heap free/minimum/largest `263460/231580/110580` B, task stack reserved `8192` B, setup `not measured`. Build metadata and ELF sections are retained alongside the capture.
- `build\default\vyne\capture-20261117T005320466582.log`: model `5b5ed844710c6c886e82c597e4ce30bcfdcd4d5a4059bd5f425d946070998f4e`, capture SHA256 `21557935d649f2cb78a216d12c07dcaf05f31b35d2d34d5c482bf62d15c67d6f`, max absolute error `1.1920929e-07`, stack free minimum `7036` B, internal heap free/minimum/largest `263460/231580/110580` B, task stack reserved `8192` B, setup `not measured`. Build metadata and ELF sections are retained alongside the capture.
- `build\sized\tflm\capture-20261117T005215288322.log`: model `5b5ed844710c6c886e82c597e4ce30bcfdcd4d5a4059bd5f425d946070998f4e`, capture SHA256 `c4e54b87fb08b9e6a8dc098f656578fa6bfd43e548830ae1515cda214b22ca3a`, max absolute error `1.34110451e-07`, stack free minimum `6688` B, internal heap free/minimum/largest `261860/230196/110580` B, task stack reserved `8192` B, setup `712 us`. Build metadata and ELF sections are retained alongside the capture.
- `build\sized\tflm\capture-20261117T005430387820.log`: model `5b5ed844710c6c886e82c597e4ce30bcfdcd4d5a4059bd5f425d946070998f4e`, capture SHA256 `e963c0a39e0048f7a61289582fc44f5ea15be42718e68beca9074fe669ae4772`, max absolute error `1.34110451e-07`, stack free minimum `6720` B, internal heap free/minimum/largest `261860/230196/110580` B, task stack reserved `8192` B, setup `712 us`. Build metadata and ELF sections are retained alongside the capture.
- `build\sized\tflm\capture-20261117T005641064880.log`: model `5b5ed844710c6c886e82c597e4ce30bcfdcd4d5a4059bd5f425d946070998f4e`, capture SHA256 `15f27092e0ac14dd5cd8850d00dc337f3d44b948cb607e9dd944bfe60877ff76`, max absolute error `1.34110451e-07`, stack free minimum `6720` B, internal heap free/minimum/largest `261860/230196/110580` B, task stack reserved `8192` B, setup `711 us`. Build metadata and ELF sections are retained alongside the capture.
- `build\sized\vyne\capture-20261117T005257029458.log`: model `5b5ed844710c6c886e82c597e4ce30bcfdcd4d5a4059bd5f425d946070998f4e`, capture SHA256 `02db70d4a913e52561e70fc47634bf0b9d1a828a3a50208e26e0cf5af31801d2`, max absolute error `1.1920929e-07`, stack free minimum `7036` B, internal heap free/minimum/largest `263460/231580/110580` B, task stack reserved `8192` B, setup `not measured`. Build metadata and ELF sections are retained alongside the capture.
- `build\sized\vyne\capture-20261117T005406744254.log`: model `5b5ed844710c6c886e82c597e4ce30bcfdcd4d5a4059bd5f425d946070998f4e`, capture SHA256 `1034031b18f1bdb2c967fe1e1491aade4023cc817ef6763d42c5a69221d1c1ec`, max absolute error `1.1920929e-07`, stack free minimum `7036` B, internal heap free/minimum/largest `263460/231580/110580` B, task stack reserved `8192` B, setup `not measured`. Build metadata and ELF sections are retained alongside the capture.
- `build\sized\vyne\capture-20261117T005704077463.log`: model `5b5ed844710c6c886e82c597e4ce30bcfdcd4d5a4059bd5f425d946070998f4e`, capture SHA256 `8e9ef4544d1a9f54e74d201869e4c887820074e0d0c0d43e635d621acecc1c1a`, max absolute error `1.1920929e-07`, stack free minimum `7036` B, internal heap free/minimum/largest `263460/231580/110580` B, task stack reserved `8192` B, setup `not measured`. Build metadata and ELF sections are retained alongside the capture.

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

- Rebuilt compiler: `xmake -y` passed. Existing registered regressions:
  `xmake test-all` **21/21** passed. The separate existing
  `examples/test/float32_test.vy` suite also passed **17/17** scalar, array,
  promotion and scratch checks.
- Benchmark unittest suite: **18/18** passed (16 standard-library contracts and
  two NumPy export-format tests). New regressions compile/run at **both `-O0`
  and `-O3`**, covering delayed versus early narrowing, Float32 rounding, mixed
  precision, signed zero, NaN/Infinity, string/boolean runtime fallback behavior,
  exactly-once ordered side effects and scratch accumulation. Emitted-code tests
  reject boxing in the MLP's multiply-accumulate loop.
- Repeated the real Python export after the fix and compared it against the
  existing baseline: **all four model artifacts are byte-identical**. Maximum
  PyTorch/Keras/TFLite export error remains **1.043081283569336e-7**. Model SHA256:
  `5b5ed844710c6c886e82c597e4ce30bcfdcd4d5a4059bd5f425d946070998f4e`.
- The updated results contain **10 new accepted cold-reset COM5 captures**:
  **20,000 timed inferences** and **800 validated reference outputs**. Default
  profiles have two runs/runtime; sized profiles have three runs/runtime.
  Runtime order was reversed on the second pass. The original ten captures are
  preserved separately above. Maximum device errors are unchanged:
  **1.34110451e-7 TFLM**, **1.1920929e-7 Vyne**.
- Updated compiler binary SHA256:
  `bd66976b194b6d5467478783493add917bd0f2835549bb70d2352f358ad63a11`.
  These measurements use the working-tree conversion patch on `6d353f5`, not
  the unmodified committed compiler; the same compiler source is now committed
  in `907137a`. Build snapshots retain the original capture-time revision/dirty state,
  project-source hashes and compiler/model/library/FQBN identities. Each Vyne
  profile's `compiler.patch` and `codegen-audit.json` retain the compiler diff,
  source hashes, assembly hash and ELF hash to identify the actual measured code.
- `tests/check_codegen_assembly.py` passed for both profiles. Actual
  `vyne_setup()` disassembly contains hardware **`mul.s` and `add.s`**, with no
  `__extendsfdf2` or `__truncdfsf2` references. The flash-weight accessors are
  inlined and the emitted MAC loop contains no `vyne_binop`, `vyne_float32` or
  `vyne_to_float32` calls. `vyne-setup.asm` is retained. This does not assert that
  unrelated formatting/runtime code elsewhere in the whole image never uses double.
- The **896-byte TFLM negative test passed again**: it failed specifically at
  `AllocateTensors`, retained its raw evidence, and added no accepted sample.
  An initial negative-test build hit a transient external Xtensa GCC 14.2
  internal compiler error; retrying the same source and flags succeeded. Its
  log is retained as `build/sized/tflm/compile-toolchain-ice.log`; that build
  produced no performance sample. The successful 1,024-byte run still uses
  **904 bytes** of tensor arena.
- **Vyne latency improvement:** sized baseline **3.101680 → 0.132350 ms**,
  **23.435× speedup / 95.733% lower latency**. Default baseline
  **3.091650 → 0.132350 ms**. TFLM remains effectively unchanged near
  **0.077040 ms** and is still **1.718× faster**. This fixes the large boxing
  bottleneck; the remaining flash-access/loop/kernel difference has not been isolated.
- **Vyne static DRAM:** default **87,860 → 22,324 B**; sized
  **23,348 → 22,324 B**. `nm` verifies the unused arena is now eliminated
  (**0 linked bytes**); activation scratch stays **424 B**, with no RAM weight
  cache. Versus TFLM, this is **1,376 B less sized static DRAM (~5.81%)** or
  **16,736 B less default static DRAM (~42.85%)**.
- **Vyne flash:** sized **284,928 → 283,424 B** (1,504 B reduction), default
  **284,944 → 283,424 B**. The updated image is **38,848 B smaller than TFLM
  (~12.05%)**. Arena payload peak stays zero. Task stack capacity is still
  **8,192 B** and minimum free stack improves to **7,036 B**; internal heap
  free/minimum/largest is **263460/231580/110580 B** in both Vyne profiles.
- No total peak-SRAM or hard-realtime claim is supported. Static arena elimination
  is workload/compiler/linker-dependent, not a universal region allocation guarantee.
  LOC and integration costs are unchanged apart from measurement tooling; lower
  latency than TFLM and less total handwritten boilerplate are not established.
- COM5 now contains the successful updated sized Vyne benchmark firmware and
  idles after reporting. The previous Wi-Fi plotter was not restored.
- The PR's initial CI failures exposed a plotting-runner handoff problem: a
  relative `VYNEC` path was re-resolved under the nested project directory.
  `2554512` forwards the resolved absolute path and uses true emit-only mode
  for hosted sniffer binding checks. The complete Windows plotting host suite
  passes with a relative root-level compiler override. These test-runner changes
  do not alter the recorded firmware or benchmark measurements.

### What changed in the compiler

`BuiltInCallNode` now exposes the known result types of `float32()` and
`float64()`, and supported Float32 binary arithmetic propagates its result type.
`codegen/builtins.cpp` evaluates conversion arguments once, emits a typed native
temporary for proven numeric inputs, and registers it in the emitter's native
type table. Unknown/string/boolean inputs still pass through the existing runtime
converter once before materialized unboxing. Integer-conversion policy, mixed
promotion rules, bounds checks, region budgets and general array ABI are unchanged.

The exact original benchmark expression remains:

```vyne
acc = float32(acc + float32(input[k] * bench_weight1(j * 64 + k)));
```

Its typed conversion results now reach the **already-existing Float32 operator
fast path** instead of losing type/representation information and entering the
generic boxed dispatcher. Explicit temporary assignments retain rounding points;
Float64 arithmetic is performed before an explicit narrowing cast. No cast is
blindly removed, no fast-math/FMA relaxation is enabled, and no weights are cached
in RAM. Removing the hot generic dispatcher also allows unreachable arena-backed
runtime storage to be eliminated naturally from this image.

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
   initialization evidence and honest LOC. Show the measured 23.4× codegen
   improvement, inspect the native Float32 assembly, and retain TFLM's remaining
   latency advantage rather than extrapolating an unmeasured universal win.
4. **What I would ship and what I would fix.** Choose by measured requirements;
   discuss proper arena sizing, native Float32 lowering, future Int8 tests and
   missing full-SRAM/energy/long-run evidence. Link commands, raw artifacts and PR.
