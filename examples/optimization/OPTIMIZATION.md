# Optimization outcomes and reproducibility

This is the evidence ledger for the [optimization roadmap](../../docs/todos/todo_optimization.md).
Parent issue: [#150](https://github.com/t2ncay/vyne/issues/150).
Branch: `optimization/memory-safety-and-codegen`; baseline source: `6d6ef99`.
All changes accumulate in one PR. Source findings start unverified; completed
results must link their tests, issue, and implementation commit.

## Current measured gains

No implementation has been verified yet. Performance, memory, and safety gains
are **not measured**. Documentation/tracking setup is not an executable speedup.

| ID | Performance | Memory | Safety/correctness | User experience | Reproducibility | Status |
| --- | --- | --- | --- | --- | --- | --- |
| Setup | Not measured | Not measured | No runtime change | Full actionable roadmap | 70 native sub-issues, stable task IDs and evidence policy | Established |

## Running tests

New task regressions, native runtime reproducers, benchmark sources, and runner
documentation live here. Prefer sources over committed generated `.vy.c`, binaries,
or machine-local reports. Each runner must distinguish expected compile rejection,
expected runtime failure, successful output, timeout, and sanitizer failure.
Invoke generated binaries directly where needed; do not infer success solely from
absence of a diagnostic. Runner commands will be added with the first regression.

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

Entries will be added only after their acceptance criteria have run successfully.
