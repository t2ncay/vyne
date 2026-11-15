# Optimization outcomes and reproducibility

This is the evidence ledger for the [optimization roadmap](../../docs/todos/todo_optimization.md).
Parent issue: [#150](https://github.com/t2ncay/vyne/issues/150).
Branch: `optimization/memory-safety-and-codegen`; baseline source: `6d6ef99`.
All changes accumulate in one PR. Source findings start unverified; completed
results must link their tests, issue, and implementation commit.

## Current measured gains

V01 and V02 are verified on Windows and Linux/WSL: child failures reach CLI
callers, and diagnostic/instrumented generated-C builds are explicit. Executable
speed and runtime memory gains are **not measured**. Debug/instrumented artifact
sizes and functional detector results are recorded separately below.

| ID | Performance | Memory | Safety/correctness | User experience | Reproducibility | Status |
| --- | --- | --- | --- | --- | --- | --- |
| Setup | Not measured | Not measured | No runtime change | Full actionable roadmap | 70 native sub-issues, stable task IDs and evidence policy | Established |
| V01 | Not measured | Not measured | Runtime failures no longer masked by CLI | Correct child exit codes and decoded summaries | Cross-platform exact-status regression and CI step | Implemented/tested awaiting merge |
| V02 | Not measured | Runtime memory not measured; larger debug artifacts reported | Emitted-runtime ASan/UBSan faults detected; normal bounds checks retained | Visible warnings, selectable detectors, explicit unsupported modes | Identical emitted C/checksum across supported profiles; 28 Windows/38 Linux checks | Implemented/tested awaiting merge |

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
- **Implementation:** commit titled `feat(cli): add generated-C diagnostic and sanitizer profiles`;
  its hash is recorded in #152 after creation.
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
