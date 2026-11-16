# Registered regression suites

`suites.toml` inventories automated fixtures at their actual repository paths.
`../run_tests.py` requires Python 3.11+ and a built host compiler. Tests generate
temporary artifacts under ignored `build/test-runs/`, not beside fixture sources.

```sh
python scripts/run_tests.py --list
python scripts/run_tests.py --suite safety
python scripts/run_tests.py --suite codegen
python scripts/run_tests.py --suite all --list
python scripts/run_tests.py --suite compiler --suite dsp --suite training
python scripts/run_tests.py --suite safety --compiler ./vyne_run
```

`VYNEC` selects another compiler; `--compiler` overrides it. Source paths are
root-relative in the manifest. `--source <path>` selects a source already
registered in the requested suites. Invocations work from other directories;
explicit relative CLI paths resolve from the caller's directory.
Xmake tasks use the repository's host compiler and honor `PYTHON`; their default
interpreter is `python` on Windows and `python3` on POSIX hosts.

## Inventory and outcome contracts

There are 21 registered cases in eight suites:

| Suite | Cases | Contract |
| --- | ---: | --- |
| compiler | 3 | Native arithmetic, discarded native call side effects, region checksum |
| safety | 10 | Four positive cases, four compile rejections, two runtime bounds failures |
| codegen | 2 | Reviewed scalar/region C goldens plus executable output |
| ast | 1 | Interpreter scalar output, shared with a native golden fixture |
| dsp | 2 | Complex arithmetic and interpolation library assertions |
| training | 1 | Deterministic CPU MLP loss decrease and accuracy |
| graphics | 1 delegated suite | Existing headless raylib acceptance runner; no camera/display/audio flag |
| optimization | 1 delegated suite | V01–V04 acceptance and adversarial runner tests |

Delegated suites perform additional internal checks; do not add their assertion
counts to registered-case counts as if those were distinct programs.

The default Python invocation runs compiler, safety, codegen, and ast (16 cases).
`xmake test` additionally runs optimization acceptance. `xmake test-all` runs all
eight registered suites. Dedicated tasks include `test-safety`, `test-codegen`,
`test-ast`, `test-optimization`, `test-dsp`, `test-training`, and `test-graphics`.
`quick-test` selects compiler smoke cases. `test-init` uses the independent
initialization/hardware-configuration acceptance runner.

Optimization acceptance also requires **PowerShell 7+ (`pwsh` on PATH)** and a
host C++ compiler (`CXX`, or `--cxx` for `examples/optimization/run.py`). V04
compiles controlled native timing/memory targets and tests the root `bench.ps1`
measurement/export contract. Missing PowerShell fails explicitly. The two Linux
CI compiler configurations run V04; Windows process-lifetime/peak accounting is
covered by the local Windows acceptance matrix. See
[`OPTIMIZATION.md`](../../examples/optimization/OPTIMIZATION.md#v04--benchmark-timing-and-memory-reporting)
for raw evidence, platform limitations, and schema-v2 migration.

Archival interactive network/VServ programs are not registered headless tests.
Their old task names report a missing automated suite instead of passing an empty
directory. Register an actual bounded test and its expectations to add coverage.

For each selected suite, discovery patterns and registered source paths must
agree. Missing sources, unknown suites, empty inventories, unregistered discoveries,
duplicate IDs, missing goldens/output contracts, malformed expectations, and skip
entries fail before execution. A new source matching an inventory pattern needs
an explicit case; it is not silently ignored.

## Case schema

```toml
[[cases]]
id = "negative_index"
suite = "safety"
source = "examples/safety/negative_index.vy"
kind = "runtime_failure"
expected_exit = 1
required = ['\bVNE-072\b']
```

- `native`: compile-only must return zero and produce an executable; direct
  execution must return zero with required output.
- `runtime_failure`: compilation must succeed first; direct execution must return
  the exact nonzero status with its diagnostic. A compile failure cannot satisfy it.
- `compile_failure`: compile-only must return the exact nonzero status with its
  diagnostic and must not leave an executable.
- `interpret`: invoke `--ast`, assert zero status and required output.
- `golden`: compile and compare normalized C, then assert native status/output.
  Requires `golden = "path/to/reviewed.expected.c"`.
- `command`: a registered script performs a delegated suite. The `command` array
  supports `{python}`, `{source}`, `{root}`, and `{compiler}` placeholders; `VYNEC`
  is set to the selected compiler. Shell execution is not used.

`required` is a nonempty list of regular expressions. Optional `forbidden`
patterns augment the default failure/unreachable markers. Optional positive
`timeout` defaults to 120 seconds. Timeouts fail even after a success marker and
terminate the process tree. A zero exit without required output is not success.

Exit statuses: `0` all selected cases passed, `1` one or more execution contracts
failed, `2` discovery/manifest/selection or missing-compiler error. `--list` validates
the inventory without requiring a compiler or executing it.

## Golden maintenance

Goldens live under `examples/optimization/codegen/`. Normalization only canonicalizes
machine-specific `vyne/runtime/` include roots and insignificant whitespace/blank
lines. Arithmetic, temporaries, bounds guards, cleanup, and other emitted statements
remain part of the comparison. Generate candidate C under `build/`, review the
diff and runtime assertions, and deliberately update expected files when an
intentional compiler change requires it. The runner never rewrites expected output.

Legacy shell/PowerShell entry points delegate to this runner. Run
`python examples/optimization/run.py --task V03` for the 30 adversarial runner
regressions and actual registered core cases. Windows can reproduce the old
false-pass predicate and contrast it with current checking:

```sh
python examples/optimization/v03_runner_tests.py --baseline --compare
```

Pending compiler reproducers such as C03/C17 are identified in their sources and
the optimization roadmap. They are not claimed as passing regression coverage.
