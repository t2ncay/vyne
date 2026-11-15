# Optimization outcomes and reproducibility

This is the evidence ledger for the [optimization roadmap](../../docs/todos/todo_optimization.md).
Parent issue: [#150](https://github.com/t2ncay/vyne/issues/150).
Branch: `optimization/memory-safety-and-codegen`; baseline source: `6d6ef99`.
All changes accumulate in one PR. Source findings start unverified; completed
results must link their tests, issue, and implementation commit.

## Current measured gains

V01 is verified on Windows and Linux/WSL: child failures now reach CLI callers.
Executable speed and memory gains are **not measured**; this is a failure-reporting
and reproducibility fix, not a numerical-kernel or allocator optimization.

| ID | Performance | Memory | Safety/correctness | User experience | Reproducibility | Status |
| --- | --- | --- | --- | --- | --- | --- |
| Setup | Not measured | Not measured | No runtime change | Full actionable roadmap | 70 native sub-issues, stable task IDs and evidence policy | Established |
| V01 | Not measured | Not measured | Runtime failures no longer masked by CLI | Correct child exit codes and decoded summaries | Cross-platform exact-status regression and CI step | Implemented/tested awaiting merge |

## Running tests

New task regressions, native runtime reproducers, benchmark sources, and runner
documentation live here. Prefer sources over committed generated `.vy.c`, binaries,
or machine-local reports. Each runner must distinguish expected compile rejection,
expected runtime failure, successful output, timeout, and sanitizer failure.
Invoke generated binaries directly where needed; do not infer success solely from
absence of a diagnostic. Build the compiler, then run:

```sh
python examples/optimization/run.py --task V01
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
- **Implementation:** commit titled `fix(cli): propagate generated executable exit status`;
  its hash is recorded in #151 after creation. A commit cannot embed its own hash.
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
