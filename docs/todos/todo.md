# The full TODO list

**Last updated:** 2026-11-10

Everything we discussed, ordered. Phases are sequential; items inside a phase can be interleaved.

> **Status snapshot (2026-11-10).** This document was written while the
> compiler was frozen behind Paper 1. It is no longer the frontier: the
> code-side work in Phases 0–2.5 is complete, and the compiler has since
> shipped well past the paper. Landed after this plan: `@pool`, `@slab`,
> `@ring`, `@streamed`, `@persistent`, `@speculative`, `@device`, `@bounded`,
> `@zeroed`, `@deterministic`, `@realtime`, `@layout<soa>`; early `drop()` and
> zero-copy `region.move()`; thread-local arenas; first-class `Float32` and the
> CUDA `@device` backend; `@differentiable` / `@backward` / `grad`; executable
> contracts (`requires` / `ensures` / `@invariant`); `vsignal`, `vcontrol`,
> `vcsv`, `vstat`, `vcomplex`, `vinterp`, `vtermplot`; I2C/SPI. The Paper 1
> source is not in this repository, so the three prose edits below remain
> unverified externally rather than actively open in the tree. Treat Phases
> 4–5 as historical deferrals; forward planning now lives in `todo_2.md`,
> `todo_expansion.md`, and `todo_libraries.md`.

---

## Phase 0 — Compiler bugs — DONE ✓ (commit ready)

- [x] **Reclaim bug in `vyne_array_push`** — verified in arrays.h
- [x] **Reclaim bug in `vyne_array_f64_push`** — verified in typed_arrays.h
- [x] **Reclaim bug in `vyne_array_i64_push`** — verified in typed_arrays.h
- [x] **Reclaim bug in `vyne_array_place_all`** — verified in arrays.h
- [x] **`vmath.seed` runtime function** — verified in vmath.h
- [x] **Seed registration in `VMATH_MAP`** — verified in native_maps.h
- [x] **Native globals fix in `assignments.cpp`** — verified
- [x] **Same class of bug in `groups_modules.cpp`** — verified
- [x] **Same class in `imports.cpp`** — verified
- [x] **Parser import recursion fix** — verified (shared cache + cycle guard)
- [x] **Loud failure on missing import** — VNE-005 in parser and linker
- [x] **Parameter `typePath` propagation** — verified end-to-end
- [x] **`-march=native` behind `--native` flag** — verified
- [x] **Duplicate reset block in `arena_free_all`** — bonus, removed

**Commit:** `Fix parser, emitter, and runtime bugs; add native-target flag`

---

## Phase 1 — Safety — DONE ✓ (commit ready)

- [x] **Scratch bounds checks** — emitted in `scratchFlatIndex`; per-dimension
      VNE-072 on violation.
- [x] **Region depth tracking** — `regionDepthAtDeclaration`, `committedVars`,
      `markCommitted`, `lookupLocalRegionDepth`
- [x] **Escape check in `AssignmentNode::compile`** — VNE-070
- [x] **Escape check in `MemberAssignmentNode::compile`** — same
- [x] **Escape check in `IndexAssignmentNode::compile`** — same
- [x] **Update §2.2** — VNE-070 check and its soundness gap documented
- [x] **Update §6.4** — syntactic approximation described, full analysis future

**Commit:** `Add scratch bounds checking and syntactic region escape checks`

---

## Phase 2 — Benchmark completion

### Experiment 1 (baseline vs region over iteration count)

- [x] **Config 3 (hoisted baseline)** — implemented in matmul_1024.vy
- [x] **ITERS=10 on all four configs** — measured (mean of 3):
      | config | peak (MB) | wall (s) | checksum |
      |--------|-------------:|-------------:|----------|
      | 0 | 308.0 ± 6.3 | 1.44 ± 0.03 | 3.47777 |
      | 1 | 62.8 ± 0.2 | 1.46 ± 0.01 | 3.47777 |
      | 2 | 35.5 ± 0.0 | 1.43 ± 0.03 | 3.47777 |
      | 3 | 63.6 ± 0.0 | 1.41 ± 0.02 | 3.47777 |
- [x] **ITERS=100 on all four configs** — measured (mean of 3):
      | config | peak (MB) | wall (s) | checksum |
      |--------|-------------:|-------------:|----------|
      | 0 | 2836.6 ± 2.7 | 13.83 ± 0.07 | 3.47777 |
      | 1 | 63.6 ± 0.1 | 13.72 ± 0.06 | 3.47777 |
      | 2 | 35.5 ± 0.0 | 14.54 ± 1.27 | 3.47777 |
      | 3 | 63.6 ± 0.0 | 13.65 ± 0.36 | 3.47777 |
      Config 0 growth slope: 28.0 MB/iteration. Ratio at ITERS=100: 79.9×.
- [x] **ITERS=1000 extrapolation** — stated in §5.7 from the measured
      28.0 MB/iteration slope. Explicitly not attempted (~28 GB).

### Experiment 2 (boxed vs region vs scratch)

- [x] Done — configs 0, 1, 2, and 3
- [x] Wall-clock parity noted in §5.7
- [x] Check-overhead disclosure rewritten to match the disabled-checks reality

### Experiment 3 (shape specialization) — NOT STARTED

- [ ] New benchmark: boxed `Array<Float64>` vs typed `VyneArray_f64` vs
      scratch, same operation (element-wise add or a small matmul)
- [ ] Measure runtime, instruction count if possible
- [ ] Write up. **NOTE**: the §5.8 slot is now occupied by Safety Checks;
      a shape-specialization write-up would need a new section or an
      appendix. Decision needed: ship without it (declare as future work),
      or add a short appendix with the numbers.

### Experiment 4 (safety) — DONE ✓

- [x] `examples/safety/` directory, one `.vy` per case (10 files)
- [x] `run_safety.ps1` — compiles each, asserts pass/fail with diagnostic
- [x] `test-safety` target in Makefile
- [x] Written up as §5.8 (was §5.9 before renumber)
- [x] `boxed_local_in_region.vy` and `safe_commit.vy` added beyond plan
- **10 passed, 0 failed** on current tree.

### Comparisons

- [x] **Hand-written C equivalent** — `examples/benchmark/matmul_1024_handc.c`.
      Matches Vyne config 2 on checksum, peak RSS (35.4 vs 35.5 MB), and
      wall clock.
- [ ] **NumPy reference** — time and memory for a 100-iteration loop,
      with and without `out=`. **Decision needed**: never started; either
      drop from scope or add as an appendix after submission.
- [x] **§5.7 ISA/codegen footnote** — the "Exposing AVX2 and FMA as an
      opt-in build mode is straightforward future work and is orthogonal
      to the memory-model claim" sentence replaces the dead §6.10 pointer.

---

## Phase 2.5 — Corpus hygiene — DONE ✓

- [x] **§5.6 with current classifier numbers** — table filled in
      (99.5% / 0.226304 / 6.4–6.3 MB / 0.12–0.14 s across three configs)
- [x] **Fix the seed position in ml_seq.vy** — `vmath.seed(42)` above
      "Generating sequences..."
- [x] **Correct §5.7 config count and numbering** — four-config draft applied
- [x] **Re-verify the growth-path decomposition** — replaced with measured
      28.0 MB/iteration (16.7 MB elements + 11.3 MB overhead/copy)
- [x] **Delete §6.7 (growth-path reclaim)** — renumbered 6.8–6.10 → 6.7
- [x] **Check `bench.ps1` flag** — confirmed `--no-scratch-bounds` was used;
      §5.7 header and "Identical wall clock" paragraph updated to match
- [x] **§3.3 cross-ref → §6.7** — verified
- [x] **§8 cross-ref → §6.7** — verified
- [x] **§5.7 dead §6.10 reference** — replaced with prose

**Still to fix (typo, non-blocking):**

- [ ] §5.7 "Identical wall clock": "config 2 at ITERS = 100..., where"
      has a stray `...` — remove.

---

## Phase 3 — Paper updates

**All applied except §1 reframing and §7 stack-array paragraph.**

- [x] **Abstract** — 8.7× / 79.9× at 10/100 iterations / identical-checksum
- [x] **§2.2** — VNE-070 described with soundness gap pointer to §6.4
- [x] **§3.3** — stack-limit bullet present; §6.7 cross-ref correct
- [x] **§4.5** — native scalar ABI documented
- [x] **§5.4** — SLP-vectorization mechanism with disassembly
- [x] **§5.6** — training-loop measurements, all three configs
- [x] **§5.7** — four-config tables at ITERS=10 and ITERS=100, interpretation,
      measured growth slope, config-2 variance, C-row placement, checks-disabled
      disclosure, ISA footnote
- [x] **§5.8 (Safety)** — ten-test table, `boxed_local_in_region` row,
      verbatim runner output
- [x] **§6.4** — syntactic check described, full analysis retained as future
- [x] **§6.6** — "A naming conflation" paragraph appended (storage-class vs.
      shape conflation)
- [x] **§6.7 renumber** — done; §6.7 is now the hoisting subsection
- [x] **§6.7 (new)** — hoisting-gaps subsection: recursion, threads,
      iteration-dependent buffer sizes, with the `1/8` vs `8/8` capability
      result
- [x] **§8 Availability** — benchmark path, hand-written-C path,
      `recursion_capability.vy`, `--no-scratch-bounds` note
- [ ] **§1 reframing — layered, not parallel.** Current text still says
      "Two constructs, described here, address the problem from opposite
      directions." Replace with the layered framing: region provides
      lifetime, scratch provides storage class, region-requirement-on-scratch
      makes shape and escape checks tractable. **This is the load-bearing
      claim behind Phase 5.**
- [ ] **§7 Related Work** — paragraph contrasting scratch with `std::array`,
      `std::inplace_vector`, Rust `[T; N]`, Ada constrained arrays.
      Two to three sentences.

---

## Phase 4 — The design fix — RECOMMENDATION: DO NOT DO THIS WEEK

- [ ] (Deferred) Shape-typed function signatures
- [ ] (Deferred) Scratch as a value of a shape type
- [ ] (Deferred) Composability demo
- [ ] (Deferred) §4.3, §4.6, §6.6 updates

**Honest recommendation unchanged: ship the memory-model paper. Save shape
types for Paper 3.**

---

## Phase 5 — Deferred to next papers (do NOT touch now)

**Paper 2 — Linear Tensors:**

- Linear/affine tensor types
- In-place vlinalg ops with static uniqueness proofs
- Buffer pools with size classes
- Copy-on-write with region-scoped refcounts

**Paper 3 — Lightweight Shape-Typed Numerical Programming:**

- Full shape types with arithmetic
- Static memory planning
- Compile-time peak bounds
- Kernel specialization

**Later:**

- Borrow parameters (`&`)
- Views
- Region-scoped cleanup hooks
- Automatic scratch promotion via escape analysis
- Region inference
- Region-scoped threads
- Region-aware FFI
- Activation checkpointing as a language primitive

**Region-vs-hoisting capability benchmark — DONE ✓**

- [x] Recursive function, two configs (hoisted vs. scratch), depth 8
- [x] Result: 1/8 vs 8/8. §6.7 in the paper.
- [x] File: `examples/benchmark/recursion_capability.vy`
- [ ] (Optional) Threaded version with two workers. Not required for this
      paper.

---

## Remaining work, in order

The Paper 1 draft lives outside this repository, so these items cannot be
checked from the tree. They are retained as the last known external checklist.

1. **§1 reframing** — replace the "opposite directions" sentence with the
   layered framing. Two to three sentences. Ten minutes.
2. **§7 stack-array paragraph** — contrast scratch with `std::array`,
   `std::inplace_vector`, `[T; N]`, Ada constrained arrays. Three
   sentences. Ten minutes.
3. **§5.7 stray `...` typo** — remove. One minute.
4. **Decide on Experiment 3 / §5.8 slot.** Recommendation: leave
   shape specialization as future work; do not write it into this paper.
   Note in §6.6 as a natural extension if you want.
5. **Decide on NumPy reference.** Recommendation: drop for this paper.

Steps 1–3 were the only edits standing between the draft and a
submittable state. Steps 4–5 are scope decisions. If the paper has since
shipped, close this document and track new work in the companion files.

---

## Commit boundary

Historical — every "READY" item below has long since landed. Kept only so the
old commit plan is still legible against `git log`.

- Phase 0: `Fix parser, emitter, and runtime bugs; add native-target flag` — landed
- Phase 1: `Add scratch bounds checking and syntactic region escape checks` — landed
- Phase 2 (partial): `Add hoisted baseline; measure four-config matmul` — landed
- Phase 2 (complete): `Complete benchmark suite; measure ITERS=100; add recursion capability demo` — landed
- Phase 3 (partial): `Fill §5.6; add §6.6 conflation; add §6.7 hoisting-gaps; renumber §5.8/§6.7` — landed
- Phase 3 (final): `Reframe §1; expand §7 Related Work` — external to the repo

---

## Status summary

**Phases 0–2.5: complete.** Four-config matmul at ITERS=10 and ITERS=100,
classifier at three configurations, safety suite at 10 tests, recursion
capability demo at 1/8 vs 8/8, hand-written C at 0.3% peak-RSS parity.

**Phase 3: externally gated.** Two prose edits (§1, §7) and one typo were the
last known open items; the paper source is not in this tree.

**Phases 4–5: correctly deferred, and partly overtaken.** Several "next paper"
features have already shipped in the compiler (region policies, `@device`,
autodiff, `@realtime`, `@layout<soa>`), so this file is a record of the Paper 1
pipeline rather than an accurate map of the current language.
