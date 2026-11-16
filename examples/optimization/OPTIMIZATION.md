# Optimization outcomes and reproducibility

This is the evidence ledger for the [optimization roadmap](../../docs/todos/todo_optimization.md).
Parent issue: [#150](https://github.com/t2ncay/vyne/issues/150).
Branch: `optimization/memory-safety-and-codegen`; baseline source: `6d6ef99`.
All changes accumulate in one PR. Source findings start unverified; completed
results must link their tests, issue, and implementation commit.

## Current measured gains

V01-V03 are verified on Windows and Linux/WSL: child failures reach CLI callers,
diagnostic/instrumented generated-C builds are explicit, and regression discovery
and phase-specific expectations are enforced. V04 corrects benchmark timing and
separates sampled, OS-reported, and logical arena memory with measured evidence
below. Executable
speed and runtime memory gains are **not measured**. Debug/instrumented artifact
sizes and functional detector results are recorded separately below.

| ID | Performance | Memory | Safety/correctness | User experience | Reproducibility | Status |
| --- | --- | --- | --- | --- | --- | --- |
| Setup | Not measured | Not measured | No runtime change | Full actionable roadmap | 70 native sub-issues, stable task IDs and evidence policy | Established |
| V01 | Not measured | Not measured | Runtime failures no longer masked by CLI | Correct child exit codes and decoded summaries | Cross-platform exact-status regression and CI step | Implemented/tested awaiting merge |
| V02 | Not measured | Runtime memory not measured; larger debug artifacts reported | Emitted-runtime ASan/UBSan faults detected; normal bounds checks retained | Visible warnings, selectable detectors, explicit unsupported modes | Identical emitted C/checksum across supported profiles; 28 Windows/38 Linux checks | Implemented/tested awaiting merge |
| V03 | Not measured | Not measured | Empty/false-pass/wrong-phase/missing-artifact/golden/timeout failures rejected | Actual fixture discovery, useful failures, portable compatibility entries | 30 adversarial regressions per platform; 21 registered Windows contracts; reviewed C goldens | Implemented/tested awaiting merge |
| V04 | Measurement error corrected; no executable speedup claimed | Missed 32 MiB burst distinguished from OS peak; no memory reduction claimed | Exit-aware timing, null unavailable metrics, explicit arena snapshots | Labeled sources/units, raw exports, compatible-baseline checks | 17 contracts each on Windows/Linux; preserved polling reproducer | Implemented/tested awaiting merge |

## Running tests

New task regressions, native runtime reproducers, benchmark sources, and runner
documentation live here. Prefer sources over committed generated `.vy.c`, binaries,
or machine-local reports. Each runner must distinguish expected compile rejection,
expected runtime failure, successful output, timeout, and sanitizer failure.
Invoke generated binaries directly where needed; do not infer success solely from
absence of a diagnostic. Build the compiler, then run:

```sh
python examples/optimization/run.py --task V01
python examples/optimization/run.py --task V02
python examples/optimization/run.py --task V03
python examples/optimization/run.py --task V04
python examples/optimization/run.py --task all
# Select a differently named compiler, as in CI:
python examples/optimization/run.py --task V01 --compiler ./vyne_run --cxx g++-13
```

`VYNEC` and `CXX` are also supported. The runner stores generated artifacts in
temporary space-containing directories under ignored `build/optimization/` and
removes them after each run. It fails on missing compiler, incorrect exit status,
missing required output, forbidden output, compiler failure, or timeout. This is
functional acceptance, not timing instrumentation. `--baseline` deliberately
asserts the old false-success behavior and is never used by acceptance CI.
V04 requires PowerShell 7+ (`pwsh` on PATH); absence is an error, not a skip.

## Required outcome record per task

1. **Identity:** roadmap ID, GitHub sub-issue, before/after revision, test sources,
   implementation commit, date, and implemented/tested versus merged status.
2. **Before:** reproduced defect or measured expensive mechanism, exact observed
   output/status, applicable language contract, and confidence/limitations.
3. **After:** targeted acceptance results and affected existing regressions.
4. **Performance:** elapsed runtime/throughput/latency where relevant, compile and
   cold-start versus warm execution separated, samples/variability and percent
   change. Kernel-only timing is not application timing.
5. **Memory:** allocation count/bytes, peak live arena usage, process and device
   memory where relevant. Distinguish retained capacity from live memory and
   sampled working set from a real peak. State unavailable metrics explicitly.
6. **Safety/correctness:** failure prevented, checks/proofs added, sanitizer results,
   semantic consistency, checksums or numerical error/tolerance.
7. **User experience:** diagnostics, exit statuses, predictable resource behavior,
   and tooling improvements demonstrated by tests.
8. **Reproducibility:** exact commands, compiler/linker versions and flags,
   OS/CPU/GPU/backend, input sizes/seeds, warmups/repetitions, and raw sample
   collection. State fallback backend and unavailable hardware checks explicitly.
9. **Tradeoffs:** measured overhead/regressions, retained memory, code size,
   numerical changes, supported platforms, and inconclusive results.
10. **Language philosophy compatibility:** deterministic regions, explicit
    promotion/ownership, no GC, observable sequencing/mutation, numerical contract,
    visible costs, portable defaults, and embedded budgets as applicable.

## Reporting rules

- Establish baseline evidence before claiming improvement; missing metrics are
  **not measured**, never invented or assumed zero.
- Require outputs/checksums/tolerances and successful runs before timing claims.
- Benchmark enough repetitions to characterize noise; exclude failures and timeouts
  from successful-run aggregates while retaining their failure evidence.
- Preserve important safety overhead transparently rather than disabling checks
  to improve a score. A correctness fix need not produce a performance gain.
- ASan/UBSan on the compiler does not instrument emitted C; arena reuse/poison
  regressions remain necessary even with generated-runtime sanitizers.
- Keep portable reference comparisons distinct from ISA, algorithm, and backend
  changes. CPU fallback is not GPU verification.
- Report regressions/inconclusive results alongside gains. Compare the original
  baseline to the latest verified revision directly; do not add task percentages.

## Verified task results

### V01 — Generated executable exit-status propagation

- **Issue:** [#151](https://github.com/t2ncay/vyne/issues/151), native child of #150.
- **Date/status:** 2026-11-15; implemented/tested awaiting merge.
- **Before revision:** `1d80a09` (documentation-only; code equals `6d6ef99`).
- **Implementation:** `9180e90`, `fix(cli): propagate generated executable exit status`.
- **Sources:** `cli/file_handler.cpp`, `cli/process_status.h`.
- **Regressions:** `exit_success.vy`, `exit_seven.vy`, `exit_twenty_three.vy`,
  `exit_bounds_failure.vy`, `process_status.cpp`, and `run.py` in this directory.

#### Observed before/after

Each CLI result below was checked through both default execution and `--run`.
Generated binaries were also invoked directly, proving the program itself had
already returned the intended status rather than simply matching diagnostic text.

| Program | Direct binary | Windows baseline CLI | Fixed Windows CLI | Fixed Linux CLI |
| --- | ---: | ---: | ---: | ---: |
| Successful explicit exit | 0 | 0 | 0 | 0 |
| Explicit exit seven | 7 | 0 | 7 | 7 |
| Explicit exit twenty-three | 23 | 0 | 23 | 23 |
| Scratch bounds failure | 1 | 0 | 1 | 1 |

**Gain:** six false-success observations in the Windows baseline matrix became
six correctly reported failures. Compile-only remains successful for all four
programs. Required output appears, and code after termination never executes.
Linux acceptance independently confirms decoded status in both summaries and
process results; a full pre-fix Linux compiler baseline was not executed.

The process-status contract also verifies `system()` launch/wait failure (`-1`)
maps to `1`, normal statuses include `255`, wider Windows status `513` is retained,
and a real POSIX command processor killed by SIGTERM maps to `143`. Unexpected
POSIX stopped status maps to failure. Launch failure is tested at the decoder
boundary; forcing a real command-processor creation failure is not simulated.

**Verification:** 16 Windows baseline checks; 20 post-fix runner checks on Windows
and 20 on Linux/WSL, including a compiled seven-case platform contract and the
existing native-zero side-effect and current codegen-peephole regressions. CI now runs
the acceptance suite in both existing build configurations; remote results are
recorded in the PR, not presumed by these local results.

The stricter runner also rejected the legacy `examples/safety/correct_index.vy`
on both platforms: line 9 uses unsupported typed-declaration syntax. This is
recorded under V03; it is not repaired or counted as passing in V01.

#### Reproduction environment and commands

- Host: Microsoft Windows 10.0.26300, x86_64, Intel Core i7-14700 (20 cores,
  28 logical processors); Python 3.14.5; MinGW GCC/G++ 15.2.0.
- Linux: Ubuntu under WSL2, x86_64, kernel 6.6.114.1-microsoft-standard-WSL2;
  GCC/G++ 15.2.0; Python 3.14.4. CPU backend, no device fallback involved.
- Windows compiler: xmake release configuration, C++23/O3, existing target flags.
- Linux local compiler: C++23/O0 (compiler build only). Both paths emit native
  programs with the existing GCC O3 flags; no numerical/ISA defaults changed.
- Contract compiler: C++23/O2 with `-Wall -Wextra -pedantic`.
- One deterministic functional matrix per stage; no warmups or timing samples.

Windows baseline, **before applying V01**:

```powershell
xmake
python examples/optimization/run.py --task V01 --baseline
```

Windows acceptance after V01:

```powershell
xmake
python examples/optimization/run.py --task V01
```

Linux local build/acceptance from the repository root (the output name is ignored
by the existing `*_release` pattern; it does not replace a user's `vynec` binary):

```sh
g++ -std=c++23 -O0 -I. -Ivendor main.cpp cli/*.cpp \
  vyne/compiler/ast/*.cpp vyne/compiler/lexer/*.cpp \
  vyne/compiler/parser/*.cpp vyne/compiler/codegen/*.cpp \
  vyne/modules/common/*/*.cpp -o v01_linux_release -lm -pthread -ldl
python3 examples/optimization/run.py --task V01 --compiler ./v01_linux_release
```

On Windows these Linux commands were invoked through `wsl --exec`. The Linux
build reports existing vendored JSON literal-operator deprecation warnings;
the standalone process-status contract compiles without warnings.

#### Costs, safety, and philosophy compatibility

- **Performance/memory:** not measured; no speedup, RSS reduction, or allocator
  improvement claimed. Changes are confined to host status decoding/reporting.
- **Safety/correctness:** tools can detect actual runtime failures; this does not
  repair the separate memory-safety findings. No generated-runtime sanitizer gain
  is claimed. Program exit statuses and successful output remain intact.
- **User experience/reproducibility:** scripts and CI receive useful failure
  codes rather than apparent success. POSIX summaries show actual exit codes
  instead of encoded wait values. Failure to launch/wait has an explicit diagnostic.
- **Tradeoff:** scripts that accidentally relied on false success now receive
  nonzero results; that is the intended correction. Signal results use the usual
  shell-style `128 + signal` convention rather than changing process launch APIs.
- **Language philosophy:** no changes to regions, ownership, promotion, GC,
  floating-point arithmetic, aliasing, embedded policy, or program evaluation.
  The host CLI faithfully exposes termination already chosen by the executable.

### V02 — Explicit generated-C diagnostic and sanitizer profiles

- **Issue:** [#152](https://github.com/t2ncay/vyne/issues/152), child of #150.
- **Date/status:** 2026-11-15; implemented/tested awaiting the single PR #221 merge.
- **Before revision:** `9180e90`; the Windows binary was tested before rebuilding,
  and the preserved Linux V01 binary was used for the Linux baseline.
- **Implementation:** `86b6c1b`, `feat(cli): add generated-C diagnostic and sanitizer profiles`.
- **Sources:** `main.cpp`, `cli/file_handler.h/.cpp`, and `cli/bench.cpp`.
- **Tests:** `profile_safe.vy`, `profile_warning.vy`, `profile_overflow.vy`,
  `profile_address.vy`, and `run.py` in this directory. CI runs V02 in both
  existing compiler build configurations, independently of compiler instrumentation.

#### Profile contract

| Profile | Generated-program flags | Support |
| --- | --- | --- |
| release (default) | `-O3 -w` | Existing host GCC backend |
| diagnostic | `-O2 -g -Wall -Wextra -Wformat=2` | Host Windows/POSIX GCC modes |
| sanitize | `-O1 -g -Wall -Wextra -Wformat=2 -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer` | Supported POSIX compiler/linker sanitizer runtimes |

`--sanitizers address`, `--sanitizers undefined`, or either ordering of
`address,undefined` explicitly selects detectors with the sanitize profile. Values
are allowlisted, not appended as arbitrary shell/compiler flags. Default selection
is both. Compile, run, assembly, and explicit-file project build/run use the same
profile flags. Profile parsing accepts space and equals forms. Missing/unknown
values, detector selection without sanitize, nondefault profiles in interpreter/
emit-C/firmware modes, and benchmark overrides return `2` with an explanation.

Windows GCC sanitizer requests are explicitly unsupported, not silently turned
into uninstrumented builds. A POSIX compiler/linker failure still returns nonzero
and retains its actual diagnostics; unsupported toolchains are not reported as
successful sanitizer runs. No sanitizer-default environment overrides are installed
for normal users. Diagnostic/sanitize profiles never imply `--no-scratch-bounds`
or change arena poison/zeroing/retention policies.

#### Observed before/after and acceptance

On both baseline platforms, `--c-profile=diagnostic` and `--c-profile=sanitize`
were unrecognized and silently ignored by the old flag scan: the safe fixture
still used GCC O3, and the overflow detector fixture completed without a UBSan
report. Two baseline checks ran on each platform. This demonstrates missing
instrumentation, not a defined overflow result or a new overflow policy.

Post-fix evidence:

- **28 Windows checks** and **38 Linux/WSL checks** passed.
- Default/explicit release and diagnostic emit byte-identical C for the valid
  region/scratch numerical fixture; Linux sanitize emits the same C too.
  Their native outputs all assert `V02 checksum: 10`.
- Direct execution and compile-and-run both detect intentional signed overflow
  under UBSan and stack overflow under ASan, returning `1` without reaching the
  fixture's escaped marker. These faults occur in generated executables, not in
  the C++ compiler. Each detector is independently selected for its fault test.
- Language bounds checks remain effective in diagnostic and combined sanitizer
  modes. The ASan-specific detector fixture deliberately requests
  `--no-scratch-bounds` only to reach its unsafe stack read; this is not a normal
  profile setting and not a workaround used for valid-program acceptance.
- A real unused-local warning is exposed. The warning fixture surfaced **5 GCC
  warnings on Windows** and **7 on Linux** in this environment, including runtime
  header warnings. Counts are observational, not a promise across tool versions.
- Diagnostic assembly is produced; sanitized Linux assembly contains ASan
  instrumentation. Explicit-file `build`/`run` accept diagnostic profiles.
- Invalid options/modes and Windows sanitizer requests fail explicitly. Unsupported
  firmware paths reject before invoking hardware tools.
- V01's **20 checks per platform** still pass in the same combined test run:
  **48 Windows** and **58 Linux/WSL** total functional checks.
- The maintained CLI manual and generated site/search output are synchronized;
  **5 documentation pipeline tests** and the freshness/link check pass locally.

#### Measured artifact overhead (not RSS or executable speed)

File sizes below were observed for `profile_safe.vy` during one functional matrix
with the printed profile flags. They include debug/instrumentation data and are
not memory-usage, code-section-size, throughput, or statistical timing measurements.

| Platform/toolchain | Release bytes | Diagnostic bytes | Combined sanitizer bytes |
| --- | ---: | ---: | ---: |
| Windows MinGW GCC 15.2.0 | 73,937 | 91,178 | Unsupported |
| Linux/WSL GCC 15.2.0 | 16,944 | 38,344 | 100,840 |

Diagnostic artifact overhead was **17,241 bytes on Windows** and **21,400 bytes
on Linux** for this fixture. Combined sanitizer overhead was **83,896 bytes on
Linux** relative to release. Runtime, peak RSS, allocation count/bytes, live arena
usage, and sanitizer CPU/memory overhead are **not measured**. Release remains
the appropriate baseline for later performance measurements.

#### Reproduction

Environment: same Windows/i7-14700 and Ubuntu/WSL2 environment as V01; host GCC/
G++ 15.2.0, Windows Python 3.14.5, Linux Python 3.14.4. Windows compiler uses xmake
release C++23/O3; local Linux compiler uses C++23/O0. Generated-program optimization
levels are independently selected by the profiles above. No GPU involved. One
deterministic functional matrix, no timing warmups/repetitions.

Baseline, **using a V01 compiler before applying V02**:

```sh
python examples/optimization/run.py --task V02 --baseline
# Linux: use the preserved previous compiler, not the current one:
python3 examples/optimization/run.py --task V02 --baseline --compiler ./v01_linux_release
```

Windows acceptance:

```powershell
xmake
python examples/optimization/run.py --task all
```

Linux acceptance from the repository root:

```sh
g++ -std=c++23 -O0 -I. -Ivendor main.cpp cli/*.cpp \
  vyne/compiler/ast/*.cpp vyne/compiler/lexer/*.cpp \
  vyne/compiler/parser/*.cpp vyne/compiler/codegen/*.cpp \
  vyne/modules/common/*/*.cpp -o v02_linux_release -lm -pthread -ldl
python3 examples/optimization/run.py --task all --compiler ./v02_linux_release
```

Documentation validation from `docs/`:

```sh
npm run sync
npm test
npm run check
```

The runner explicitly sets `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=0`
and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` for its sanitizer fixtures
to make detector outcomes independent of local settings. Temporary artifacts use
space-containing directories under `build/optimization/`, then are removed.
No generated artifacts or machine-local reports are committed.

#### Follow-ups exposed by diagnostics

- **C03 / #170:** scratch passed to an Array<Float64> parameter generates an
  invalid `.data` member access on a raw C array. Safe-input reproducer retained
  as `scratch_array_argument.vy`; it should pass after C03, but is not counted as
  passing V02 coverage. Source: `codegen/native_dispatch.cpp:152-153`.
- **R20 / #222:** LP64 format warnings for `%lld` with `int64_t` in
  `runtime/detail/printing.h:22` and `conversions.h:76`. Added to the roadmap as
  the 71st native sub-issue. These are reproduced warnings, not a claimed crash.
- Existing generated/runtime unused declarations and macro-comment warnings are
  visible rather than suppressed in diagnostic mode. Cleanup can follow targeted
  semantic fixes; V02 does not promote every warning to a compiler failure.

#### Philosophy compatibility and limitations

The profile is a deliberate host-tooling choice. It does not add GC, automatic
promotion, altered region reclamation, alias restrictions, relaxed math, hidden
native ISA assumptions, or firmware instrumentation. Valid generated C is unchanged;
warnings/debug data/instrumentation have visible flags and documented costs.
Sanitizer rejection/termination is explicit and preserves V01's failure-reporting
contract. Existing bounds checks remain enabled unless the caller independently
opts out. Instrumentation helps find defects; it does not solve C03/C11 or establish
complete arena memory safety. Retain poison/reuse and target-specific regressions
as the next safety work proceeds.

### V03 — Real discovery and phase-specific outcome checking

- **Issue:** [#154](https://github.com/t2ncay/vyne/issues/154), child of #150.
- **Date/status:** 2026-11-15; implemented/tested awaiting PR #221 merge.
- **Before revision:** `86b6c1b` (all six CI checks passed on that revision).
- **Implementation:** `3d441c4`, `fix(tests): enforce discovery and phase-specific outcomes`.
- **Sources:** `scripts/run_tests.py`, `scripts/tests/suites.toml`, `xmake.lua`,
  shell compatibility entries, the now-versioned safety PowerShell wrapper, and
  restored `examples/safety/` fixtures.
- **New tests:** `v03_runner_tests.py`, controlled baseline fixtures under
  `fixtures/`, and reviewed scalar/region fixtures plus `.expected.c` goldens
  under `codegen/`. All are under `examples/optimization/`.

#### Reproduced baseline failures

1. `xmake test-compiler` returned success while reporting **0 total, 0 passed,
   0 failed** because its `tests/` patterns selected a nonexistent tree.
2. The original local PowerShell safety runner accepted **10/10 cases** with a
   controlled compiler double that **always exits 1**. Four supposed positive
   cases printed syntax failures; two runtime-negative cases failed in compilation
   with the expected code. The old predicate accepted all six inappropriate
   outcomes. The preserved predicate fixture reproduces this behavior; it is not
   used for current acceptance. The corrected wrapper accepts the four proper
   compile rejections and rejects the **six false positives/wrong-phase cases**.
3. Real fixture failures became visible: typed declarations used obsolete `=`,
   the boxed accumulator was never declared, and the commit example ended a
   function with an unsupported semicolon. The restored boxed fixtures declare
   their locals with current syntax and intentionally retain boxed representation.
4. Nonempty xmake execution exposed previously unreachable helper errors: root
   DSL functions lacked sandbox `catch`/`pcall`, and target-derived suffix selection
   chose a stale Linux `vynec` on Windows. Task helpers now receive the sandbox
   APIs explicitly and select the host executable suffix.

#### Verified gains

| Parameter | Evidence |
| --- | --- |
| Discovery | A checked manifest inventories 21 cases in eight registered suites; missing/empty/unregistered selections fail |
| Phase correctness | Compile rejections require exact status/diagnostic and no executable; runtime failures require successful compilation first |
| Positive correctness | Zero status, required output, and executable existence are all checked; failure/unreachable markers reject the run |
| Codegen reproducibility | Two reviewed C goldens compare on Windows and Linux, retaining guards, temporaries, and cleanup |
| Timeout behavior | A process that prints a success marker then hangs with a descendant is rejected; process-tree cleanup is exercised |
| User experience | Current paths, explicit case labels, useful failure details, and portable shell/PowerShell entries replace silent/no-op behavior |
| Runtime performance/memory | Not measured; no executable speed, RSS, allocation, or arena improvement claimed |

Local verification:

- **30 adversarial runner regressions passed on Windows and on Linux/WSL.**
  They inject incorrect statuses/diagnostics, missing outputs/artifacts/goldens,
  duplicate or unregistered discovery, all-skipped definitions, altered C, and
  real timeout/descendant behavior. They do not merely mirror successful execution.
- **16 registered core contracts passed on both platforms:** three compiler,
  ten safety, two golden/native, and one interpreter case.
- **Windows `xmake test-all` passed all 21 registered contracts across eight
  suites**, including numerical/training tests and existing headless graphics
  and optimization delegates. A delegated suite is one registry contract with
  additional internal checks; counts are not summed as distinct programs.
- **Linux/WSL 19 direct registered contracts passed** across core, DSP, and
  training. The shell codegen, aggregate-core, and explicit-source AST entries
  also passed. The Windows PowerShell safety entry passed its ten real cases.
- The combined optimization runner still passes V01's 20 checks and V02's
  28 Windows / 38 Linux checks alongside V03. No prior acceptance was discarded.
- Five documentation pipeline tests and freshness/link validation pass after
  synchronizing the maintained installation/testing guidance and generated site.
- Boundary commands reject an unregistered network suite and a nonexistent AST
  source with nonzero status; neither becomes an empty successful test run.

The new CI step runs V03 in both compiler build configurations. Remote results
are recorded in the PR after the commit, not inferred from these local results.

#### Reproduction and inventory policy

Environment: same Windows/i7-14700 and Ubuntu/WSL2 setup as V02; Python 3.14.5
on Windows and 3.14.4 on Linux, GCC 15.2.0, existing V02 compiler binaries (no
compiler/runtime C++ implementation was changed in V03). The portable runner
requires Python 3.11+; cases use default release compilation. Each native case
compiles to fresh temporary output then directly executes that output. Tests do
not reuse old `.exe`/`.vy.c` files in `examples/`.

Windows baseline before V03:

```powershell
xmake test-compiler
python examples/optimization/v03_runner_tests.py --baseline --original
```

The original PowerShell file had been ignored by Git; its acceptance predicate
is preserved in `fixtures/legacy_safety.ps1.in` for subsequent reproduction.
The corrected PowerShell wrapper has a narrowly scoped ignore exception and is
now versioned. Reproduce the controlled contrast on Windows after V03:

```powershell
python examples/optimization/v03_runner_tests.py --baseline --compare
```

Current Windows acceptance:

```powershell
xmake test
xmake test-all
pwsh -NoProfile -File examples/safety/run_safety.ps1
python examples/optimization/run.py --task all
```

Linux/WSL acceptance from the repository root, selecting the preserved V02 host
compiler rather than an older unrelated `vynec` binary:

```sh
python3 scripts/run_tests.py --suite compiler --suite safety --suite codegen \
  --suite ast --suite dsp --suite training --compiler ./v02_linux_release
python3 examples/optimization/run.py --task all --compiler ./v02_linux_release
VYNEC=./v02_linux_release bash scripts/run_all_tests.sh
VYNEC=./v02_linux_release bash scripts/test_codegen.sh
VYNEC=./v02_linux_release bash scripts/test_ast.sh examples/optimization/codegen/scalar.vy
```

Use `python scripts/run_tests.py --suite all --list` to inspect and validate the
registry without compiling. Registry fixtures and discovered paths must agree.
Empty/missing output contracts, missing fixtures/goldens, duplicates, skip entries,
and unknown suites return failure before execution. Individual failed execution
contracts return nonzero after reporting all selected cases. Per-phase timeouts
default to 120s, with longer explicit limits for delegated suites. Relative CLI
paths resolve from the caller; manifest source paths are repository-relative.
Xmake chooses the host compiler suffix before target configuration and defaults
to `python` on Windows or `python3` on POSIX; `PYTHON` overrides the interpreter.

Artifacts use temporary space-containing directories under ignored
`build/test-runs/` and `build/optimization/`, then are removed. Existing graphics
delegates retain their documented `build/vraylib-tests/` cache. Goldens are reviewed
test sources: only machine-specific runtime include roots and insignificant
layout are normalized. The runner never updates expected C automatically.

The inventory does not claim every application in `examples/` is an automated
test. Archival network/VServ examples have no registered headless suite; their
legacy task names now fail explicitly. Firmware and real-device testing remain
target-specific. See `scripts/tests/README.md` for schema and suite coverage.

Documentation validation from `docs/`: `npm run sync`, `npm test`, and
`npm run check`. The committed generated output matches the maintained sources.

#### C17 follow-up and language philosophy

After restoring the commit fixture's syntax, a mixed boxed/native assignment
revealed a distinct compiler defect: a new typed variable shadows the intended
outer boxed binding. `boxed_native_assignment.vy` prints `C17 outer size: 0`
instead of 3 and exits 1. This is retained as pending **C17 / #223**, the 72nd
native sub-issue. The boxed commit fixture now explicitly tests a boxed value
promoted by named-region commit and verifies size/elements after transient reuse;
the mixed-representation failure is not claimed as solved or passing coverage.

V03 changes test infrastructure and stale test syntax, not language semantics.
Named-region promotion stays explicit, regions remain deterministic/no-GC, and
tests retain the intended boxed reassignment and checkpoint behavior. Goldens
preserve actual emitted bounds/cleanup code; timing limits bound test execution,
not program realtime guarantees. Better failure detection makes later safety and
performance claims reproducible without disabling checks or hiding known defects.

### V04 — Benchmark timing and memory reporting

- **Issue:** [#156](https://github.com/t2ncay/vyne/issues/156), child of #150.
- **Date/status:** 2026-11-16; implemented/tested awaiting draft PR #221 merge.
- **Before:** `3d441c4`, with the original ignored local root `bench.ps1` preserved
  before edits. Neither root nor training harness was versioned at that revision;
  their specific ignore exceptions now make the maintained tooling reproducible.
- **Implementation:** `fix(bench): report precise timing and distinct memory measurements`;
  hash linked in #156 after commit.
- **Sources:** root `bench.ps1`, `scripts/benchmark_process.cs`, training wrapper,
  optimization runner/registry, and the existing two Linux CI build configurations.
- **Coverage:** `v04_workload.cpp`, `v04_arena.vy`, `v04_benchmark_tests.py`, and
  `fixtures/v04_legacy_benchmark.ps1.in`, all in this directory. The frozen fixture
  retains the old timer/polling/rounding mechanism for subsequent reproduction;
  it is not the acceptance harness.

#### Baseline and raw timing evidence

The original harness stopped its stopwatch only after observing exit following a
sampling sleep. It rounded seconds to three decimals and sampled memory to two
decimals before exporting/aggregating. A nominal 20 ms native sleep took roughly
one second with 1,000 ms sampling; a nominal 1,200 ms sleep took roughly two.

Raw reported seconds (no warmups; three repetitions except two for long cases):

| Workload / sampling ms | Original Windows polling | V04 Windows OS process lifetime | V04 Linux exit-observed time |
| --- | --- | --- | --- |
| Immediate / 50 | 0.076, 0.058, 0.061 | 0.0192850, 0.0154361, 0.0156566 | 0.0040691, 0.0038116, 0.0035618 |
| Sleep 20 ms / 50 | 0.070, 0.066, 0.076 | 0.0496165, 0.0453450, 0.0365492 | 0.0237420, 0.0239956, 0.0242140 |
| Sleep 20 ms / 1,000 | 1.022, 1.010, 1.008 | 0.0510516, 0.0612680, 0.0519909 | 0.0245546, 0.0248202, 0.0237454 |
| Sleep 1,200 ms / 50 | 1.278, 1.245 | 1.2209913, 1.2301511 | 1.2037832, 1.2041277 |
| Sleep 1,200 ms / 1,000 | 2.033, 2.018 | 1.2231613, 1.2260800 | 1.2043540, 1.2039034 |
| Touch 32 MiB, hold 400 ms / 50 | 0.436, 0.442, 0.442 | 0.4275828, 0.4213523, 0.4267320 | 0.4166958, 0.4123131, 0.4107784 |
| Touch 32 MiB, hold 10 ms, free, sleep 250 ms / 1,000 | 1.027, 1.008, 1.013 | 0.3027730, 0.2953063, 0.2888080 | 0.2776651, 0.2712929, 0.2707315 |

These are **measurement-correctness observations, not executable speedups**.
Windows and Linux V04 evidence collection ran concurrently on the same host;
they are not isolated performance trials. The controlled target gained dormant
timeout/descendant coverage after the original baseline, so these are not
byte-identical before/after application binaries. Each successful native run
independently reports `steady_clock` elapsed time: assertions allow 50 ms
underrun tolerance and 350 ms startup/scheduling/observation overhead, which still
reject the reproduced one-second floor. For example, Windows short/coarse
in-process samples were 0.0343796, 0.0437760, 0.0367251 s. No percentage speed
gain, throughput gain, or summed improvement is inferred from these values.

#### Memory evidence and measurement contract

The burst allocates **33,554,432 bytes**, writes one byte in each 4,096-byte page,
and checksums the stores (`57344`); it cannot be optimized into an untouched
allocation. Original Windows sampled "peak" values were **3.11, 2.57, 2.57 MiB**.
V04's burst still sampled only **2,695,168 bytes** in each run, but Windows
`GetProcessMemoryInfo` reported working-set peaks of **37,814,272; 37,826,560;
37,822,464 bytes** (36.0625, 36.07421875, 36.0703125 MiB). This demonstrates a
missed transient allocation, not less application memory. Sustained V04 Windows
allocation samples and OS peaks were 36.0625, 36.07421875, 36.078125 MiB.

- `WallSec` uses Windows OS creation/exit timestamps when available;
  `ProcessLifetimeSec` is null on other hosts. `TimingSource` records the choice.
  Windows lifetime includes loader/startup, workload, and shutdown; it is not a
  kernel-only timer. Timestamp subtraction is not a monotonic clock guarantee.
- `LaunchSec` is the monotonic duration of `Process.Start()`. `ExitObservedSec`
  runs from before launch until exit is observed, before output parsing/accounting.
  These intervals overlap process lifetime and **must not be added**. POSIX wall
  time uses this explicit observation measurement, not .NET's observed exit
  timestamp presented as an exact kernel timestamp.
- `SampledWorkingSetBytes`/`MiB` are the maximum positive direct-child working-set
  samples. Sampling can miss brief usage and perturbs scheduling. `Samples` and
  `SampledWorkingSetStatus` distinguish sampled/missed/unsupported observations.
- Windows `ProcessPeakWorkingSetBytes` and `ProcessPeakPagefileBytes` are separate
  OS counters read through a retained process handle after exit. Pagefile usage
  is the Windows API's commit-charge counter, not bytes actually paged out.
  POSIX process peaks are **unsupported/null**, not inferred from sampled RSS.
- `CpuSec` is OS accounting where available. Linux/WSL post-exit accounting was
  **unavailable/null** in this matrix; Windows valid zero/coarse CPU samples are
  retained. `CpuStatus` and aggregate counts preserve the distinction.
- Opt in to logical metrics by calling existing allocation-free **`vmem.dump()`**
  at a deliberate measurement point. The last complete stderr dump is exported
  with its count, main live/peak/blocks/capacity and commit live/peak counters.
  A malformed dump becomes `ArenaStatus=malformed` and null metrics; no dump is
  `not-reported`. Peaks retain the caller's runtime/reset scope, not an invented
  whole-program scope. Main counters describe the active thread/context;
  committed storage is reported separately. Counters include rounding/alignment,
  not just payload, and are not reachable-object counts or total process bytes.
- The region fixture checks an 8,192-byte typed allocation and post-region
  reclamation before calling dump. Both platforms reported main live **0**,
  main peak **8,192**, main block capacity **0**, and commit live/peak **0**,
  with checksum **7**. Those are measured zeros, unlike unavailable metrics.
  Missing sampling was also observed in Linux's immediate-exit case: null WS,
  zero samples, `missed`, while two other runs produced positive samples.

Schema **version 2** exports full raw per-run seconds, bytes, sources, statuses,
sample counts, timeout/exit state and captured output in JSON; CSV retains the
measurement fields without output text. Seconds/statistics are not pre-rounded.
Empty metric statistics are null with count zero; sample standard deviation is
null for one observation. Console formatting uses `n/a` for absent values.
Version-1/incompatible-timing-source baseline comparisons are explicitly skipped
with a diagnostic; sampled-WS deltas also require equal sampling intervals.
`-Source` compilation elapsed time is a separate `compile_sec` measurement and
is never included in executable wall samples. Warmup metadata reports the actual
discard count, including zero for `-NoWarmup`.

#### Verification, environment, and commands

**17 V04 contracts passed per platform**: seven duration/memory matrices,
output drainage, timeout, descendant timeout, malformed arena dump, real region
snapshot, empty/single statistics, compatible/legacy baselines, training wrapper,
and source compile-time separation. Per-run exports are checked against CSV,
independent workload time/checksum, byte-to-MiB conversion, platform sources and
Python's independent mean/median/sample-standard-deviation calculations.

Windows `xmake test-all` passed **all 21 registered contracts** across eight suites,
including the expanded optimization delegate. Linux `--task all` retained V01's
20 checks, V02's 38 checks, and V03's 30 adversarial regressions plus 16 core
contracts; the separate core/DSP/training selection passed **19 contracts**.
CI adds V04 to both Performance and Memory-Safety compiler configurations and
requires installed `pwsh` explicitly. Remote results belong in #156/PR #221 after
push; local success does not imply remote success. Windows-specific accounting
is locally verified; no new hosted Windows V04 job or macOS V04 run is claimed.

Host: Windows 10.0.26300 x86_64, Intel i7-14700; Python 3.14.5, PowerShell 7.6.10,
.NET 10.0.15, MinGW GCC/G++ 15.2.0. Linux: Ubuntu/WSL2 x86_64 kernel 6.6.114.1,
Python 3.14.4, GCC/G++ 15.2.0, PowerShell 7.5.5/.NET 9.0.14. The standalone target
uses `-std=c++17 -O2 -Wall -Wextra -pedantic`. Vyne uses the existing Windows
release compiler and preserved `v02_linux_release` compiler, with default emitted
release C flags; V04 changes no compiler/runtime C++ implementation. No GPU,
BLAS, host-native ISA, relaxed math, or firmware execution is involved.

Original local Windows baseline, before harness edits:

```powershell
python examples/optimization/v04_benchmark_tests.py --baseline --report build/optimization/v04_before.json
```

The original script was preserved as ignored `build/optimization/v04_baseline.ps1`.
The checked-in frozen mechanism supports later reproduction on either platform:

```sh
python examples/optimization/run.py --task V04 --baseline
# Explicit original (if retained locally):
python examples/optimization/v04_benchmark_tests.py --baseline --harness build/optimization/v04_baseline.ps1
```

Windows acceptance/evidence:

```powershell
python examples/optimization/v04_benchmark_tests.py --report build/optimization/v04_after_windows.json
xmake test-all
```

Linux acceptance/evidence, from the root with `pwsh` on PATH:

```sh
python3 examples/optimization/v04_benchmark_tests.py --compiler ./v02_linux_release --report build/optimization/v04_after_linux.json
python3 examples/optimization/run.py --task all --compiler ./v02_linux_release
python3 scripts/run_tests.py --suite compiler --suite safety --suite codegen --suite ast --suite dsp --suite training --compiler ./v02_linux_release
```

For this local WSL verification only, PowerShell's official 7.5.5 Linux x64
archive was unpacked under ignored `build/optimization/pwsh`; the environment used
`PATH="$PWD/build/optimization/pwsh:$PATH"` and
`DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1` because compatible ICU was unavailable.
That setting affects this host-tooling test environment, not Vyne math/runtime
defaults. Raw reports remain ignored; the evidence above is committed. Temporary
space-containing workload directories are removed by the runner.

The frozen baseline also reproduces on Linux/WSL. Its cold metadata query can
outlive a short child, so not every run reaches the polling sleep: the observed
short/coarse series was 0.529, 1.007, 1.003 s. The baseline assertion requires
reproducing interval inflation in a run, not assuming every run follows the same
schedule. Existing missing-CPU/memory fallback also printed zero in that baseline.
Five documentation pipeline tests and site freshness/link validation passed after
synchronizing the PowerShell test prerequisite into the installation guidance.

#### Tradeoffs, pending findings, and philosophy compatibility

PowerShell 7/.NET and a small host C# helper are now explicit test/tooling
dependencies. Helper compilation and environment discovery occur before timed
runs. Their startup cost, sampling/redirected-output overhead, and harness memory
are **not measured** as performance gains; output is captured in host memory,
not streamed into the Vyne arena. Direct-child process metrics exclude descendants
and device allocations. Exit-aware waits bound timeout observation independently
of sampling; tree termination and five-second output-close waits prevent the
tested hang cases. A descendant retaining pipes after normal parent exit fails
explicitly rather than hanging indefinitely.

**V05 remains pending:** legacy checksum presence/validation and failed-run
aggregation are not solved. Failed/timeout reports retain evidence and visibly
warn against performance claims; none of their values is used in the timing table
above. V06 GPU labels/checks remain pending. C03 scratch/Array ABI, C17 boxed/native
shadowing, R20 LP64 formatting warnings, and other roadmap findings remain open.
V04 does not make any of those unsafe/pending programs into passing coverage.

These changes are host measurement infrastructure. They add no GC, automatic
promotion, ownership changes, alias restrictions, evaluation reordering, allocator
retention policy, unchecked indexing, numerical reassociation, or embedded runtime
dependency. Region reclamation and explicit commit/move remain intact; the arena
fixture observes the existing policy without altering it. Executable speed, runtime
memory reductions, allocation-count reductions, device memory, and firmware costs
are **not measured**, and no such gains are claimed.
