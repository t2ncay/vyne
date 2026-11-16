# Vyne optimization and memory-safety roadmap

Last updated: 2026-11-17. Baseline source: `6d6ef99` (before this effort).

Parent issue: [#150](https://github.com/t2ncay/vyne/issues/150). The first validation
milestone, V01–V04, is delivered by [PR #221](https://github.com/t2ncay/vyne/pull/221)
from `optimization/memory-safety-and-codegen`, based on `master`. It merged in
`00f1e7a`. Completed issues #151, #152, #154, and #156 are closed;
the parent and the other 68 tasks
remain open. Continue remaining work in focused follow-up branches and PRs based
on updated `master`. Related interpreter discussion: [#35](https://github.com/t2ncay/vyne/issues/35).

For benchmark projects, record the exact `master` revision, toolchain, flags,
workload, and raw samples. V05 [#158](https://github.com/t2ncay/vyne/issues/158)
remains pending: explicitly validate checksums/results and reject failed or
timed-out samples before computing successful-run statistics.

This replaces the previous optimization draft with the complete session audit.
Source references below describe the baseline and may move as fixes land.
**Static evidence is not an executed reproducer; an expensive mechanism is not
a measured speedup.** Validate each finding before choosing its solution.

Results and reproduction commands belong in
[`examples/optimization/OPTIMIZATION.md`](../../examples/optimization/OPTIMIZATION.md).
New regressions and benchmark sources belong in `examples/optimization/`;
reuse relevant existing cases rather than duplicating their coverage.

## Language philosophy: acceptance criteria

Every task, sub-issue, and completed results entry must explain compatibility:

1. Preserve deterministic lexical region reclamation, no GC, explicit resource
   ownership, and deliberate `commit()`/`move()` promotion.
2. Preserve documented evaluation order, short-circuiting, shared-reference
   mutation, and value semantics. Representation specialization cannot change
   observable behavior. Resolve interpreter/native differences against the
   language contract rather than selecting whichever implementation is faster.
3. Keep costs visible: copies, allocations, retained capacity, synchronization,
   and reclamation. Honor `@bounded`, `@zeroed`, `@persistent`, realtime, and
   device policies. Bounded allocation does not imply identical wall-clock time.
4. Preserve the established scientific floating-point contract. Reassociation,
   parallel reductions, fusion, and alternative algorithms need numerical
   validation. Relaxed math and host-specific ISA assumptions stay explicit.
5. Respect AVR/ESP32 stack, SRAM, alignment, and toolchain limitations as well
   as desktop performance. Do not impose a heavy universal runtime.
6. Keep compiler machinery internal where possible. A new ownership model,
   changed escape semantics, automatic promotion, heap fallback, unbounded
   cache, or asynchronous user-visible operation requires a design decision;
   none is pre-approved by this roadmap.
7. Emit unchecked indexing, no-alias contracts, destructive updates, SIMD
   assertions, and invariant hoists only with justified proofs or guarded
   dispatch. Diagnostics should make rejected programs understandable.

## Workflow and definition of done

For each stable ID below:

1. Read the current implementation and document the applicable semantics.
2. Reproduce the defect or establish a benchmark baseline; reassess unsupported
   findings explicitly rather than blindly implementing the proposed fix.
3. Add an independently meaningful test under `examples/optimization/`.
4. Implement the smallest consistent solution and run targeted checks plus
   affected existing regressions. Test optimized output when C UB/aliasing is
   relevant; sanitizing only the compiler does not sanitize generated programs.
5. Update this roadmap and `OPTIMIZATION.md` with commands, evidence, tradeoffs,
   and philosophy compatibility. Record unavailable hardware checks as pending.
6. Inspect status/diff/recent history, stage only intended sources, and create
   a Conventional Commit for that solved item. Do not stage generated binaries,
   `.vy.c`, machine-local reports, or unrelated work.
7. Link the commit in the native GitHub sub-issue. Implemented/tested issues
   remain open awaiting merge; each PR closes only the issues whose acceptance
   criteria it satisfies. Keep the parent roadmap and unfinished issues open.

States: **planned**, **investigating**, **blocked**, **implemented/tested awaiting
merge**, **merged**, **reassessed**. A roadmap checkbox means its acceptance
criteria passed, not simply that a patch exists. Every ID has a GitHub
sub-issue under #150; the issue title starts with the ID for lookup.

Evidence: **S** = concrete source-level concern, not dynamically reproduced;
**H** = conditional source hazard; **B** = benchmark hypothesis;
**D** = design candidate. Priority orders work, not a security severity score.

## Execution phases and dependencies

1. Validation: V01-V08 establish reliable failures, diagnostics, timing, and CI.
2. Lifetime and allocation: C01-C06, C16, R01-R06, R11-R13.
3. Semantics and representation: C07-C15, C17, R09-R10, R14-R20.
4. Allocation/copy reduction: P01-P06, P08-P10, P14-P15.
5. Numerical/device work: R07-R08 first, then P07, P11-P13, P16-P18.
6. Incremental architecture and legacy candidates: A01, L01-L08 as evidence
   and dependencies justify. Reorder independently testable tasks when useful;
   do not enable unsafe optimization assumptions before their correctness fixes.

## V: validation and reproducibility

- [x] **V01 — Propagate executable exit status** (high; reproduced and fixed).
  `cli/file_handler.cpp:1088-1144` prints child failure but returns success.
  Decode POSIX `system()` wait status; retain Windows status; distinguish launch
  failure. Acceptance: default and explicit run preserve success and multiple
  nonzero statuses, actual bounds failure propagates, compile-only still succeeds,
  and the displayed status is decoded. Test POSIX normal/signal/launch cases too.
  Implemented/tested on Windows and Linux/WSL; see the V01 results ledger and
  [#151](https://github.com/t2ncay/vyne/issues/151). Merged via PR #221 (`00f1e7a`).
- [x] **V02 — Generated-C diagnostic and sanitizer profiles** (high; implemented/tested).
  `cli/file_handler.cpp:961-987` uses `-O3 -w`; CI compiler sanitizers do not
  instrument ordinary emitted C. Add an explicit diagnostic path with useful
  warnings and supported ASan/UBSan flags. Acceptance: a generated-runtime
  regression is detected and optimized valid output still passes. Track platform
  support and arena-specific poison/reuse tests; ASan alone misses intra-arena UAF.
  `--c-profile release|diagnostic|sanitize` and explicit detector selection now
  drive desktop GCC compile/run/assembly. Windows diagnostic and POSIX sanitizer
  coverage pass; unsupported targets/modes reject rather than silently downgrade.
  See [#152](https://github.com/t2ncay/vyne/issues/152) and the V02 results ledger.
- [x] **V03 — Repair test discovery and outcome assertions** (high; implemented/tested).
  `xmake.lua:229-236`, `scripts/test_codegen.sh:25,55-66`, and
  `scripts/run_all_tests.sh:15-23` reference missing root test trees.
  `examples/safety/run_safety.ps1:39-40` can classify unrelated failures as passes.
  Acceptance: discover actual sources, reject empty suites, assert exact expected
  compile/runtime outcomes and required output; wire optimization tests into CI.
  V01's strict runner reproduced an additional gap: `examples/safety/correct_index.vy`
  is rejected at line 9 for legacy `:: Type =` declaration syntax. Audit stale
  fixtures before treating their absence-of-diagnostic runner as valid coverage.
  Registered manifest/portable runner now checks discovery, compile/run phases,
  exact status/output, missing artifacts, C goldens, and timeouts. Restored safety
  fixture syntax and genuine boxed cases. Windows `xmake test-all` passes all 21
  registered contracts; 30 adversarial runner regressions and core cases pass on
  Windows/Linux. See [#154](https://github.com/t2ncay/vyne/issues/154) and V03 results.
- [x] **V04 — Correct benchmark timing and memory reporting** (high; reproduced and implemented/tested).
  `bench.ps1:218-246,270-274` measures exit observation after polling sleeps;
  default polling can dominate short cases. Separate process launch, actual
  execution, and sampling. Acceptance: known short/long workloads validate timing;
  distinguish sampled working set, process peak, and live arena bytes; retain raw
  precision and label missed/unsupported memory measurements.
  Exit-aware waits and raw schema-v2 exports now distinguish Windows OS process
  lifetime/peaks, monotonic exit observation, sampled working set, and optional
  allocation-free `vmem.dump()` snapshots. All 17 V04 contracts pass on Windows
  and Linux/WSL. Preserved polling fixture reproduces the timing/memory defects.
  Unix OS peaks/CPU accounting remain unsupported/unavailable explicitly; V05
  failed-run/checksum validation remains pending. See [#156](https://github.com/t2ncay/vyne/issues/156)
  and the V04 results ledger. Merged via PR #221 (`00f1e7a`).
- [ ] **V05 — Require checksums and exclude failed benchmark runs** (high, S).
  `bench.ps1:423-450,514-519,583-587` filters missing checksums and can accept
  none; failed/timed-out runs enter aggregates. Acceptance: absent, mismatched,
  malformed, and expected checksums are handled explicitly; failed runs never
  count as successful performance samples. Numeric comparisons use stated tolerances.
- [ ] **V06 — Make GPU benchmark claims and correctness precise** (high, S).
  `examples/benchmark/device_matmul_bench.vy:35-57,71-94` excludes upload/setup
  from its end-to-end label, prints error without asserting it, and permits CPU
  fallback. Acceptance: label kernel/event, launch+sync, transfer, cold start, and
  application time separately; assert error tolerance and actual backend.
- [ ] **V07 — Runtime sanitizer/concurrency/hardware test matrix** (high, S/D).
  `.github/workflows/c-cpp.yml:16-21,79-118` lacks general standalone runtime
  and emitted-runtime coverage. Add relevant runtime C tests and generated-C
  sanitizers; use TSan for shared registries where supported and CUDA memory
  checking on real hardware. Acceptance: tests fail on meaningful injected
  defects; CPU fallback is never reported as GPU validation.
- [ ] **V08 — PTX freshness and portable CUDA coverage** (medium, S/D).
  `scripts/build_cuda_ptx.ps1:27-58` regenerates embedded PTX for compute_80;
  inspected CI does not establish freshness or actual GPU correctness.
  Acceptance: documented source-to-PTX reproduction and compatibility policy,
  detectable stale artifacts, and explicit unavailable-device outcomes.

## C: compiler and generated-code correctness

- [ ] **C01 — Native typed-array return lifetime** (high, S).
  `vyne/compiler/codegen/control_flow.cpp:239-258` returns storage after region
  unwind; boxed conversion can execute after cleanup. Boxed returns at 312-341
  behave differently. Acceptance: typed return across a region survives subsequent
  allocation according to documented escape semantics, or is rejected clearly.
  Cover empty arrays and boxed/native conversion, not just bare `Array` returns.
- [ ] **C02 — Scope-aware break/continue cleanup** (high, S).
  `control_flow.cpp:373-401`, `emitter.h:1808-1827` unwind the full region stack.
  Track loop cleanup boundaries. Acceptance: outer-region data remains valid
  after an inner loop transfer; exited inner regions clean up once, in reverse
  order. Cover nested loops and regions in both nesting orders.
- [ ] **C03 — Native bounds semantics and length-preserving ABI** (high, S/D).
  `collections.cpp:201-226,296-344,468-485,545-552`, `members.cpp:319-341`,
  `functions.cpp:336-343` emit direct indexing/discard length. Acceptance: negative
  and upper-bound loads/stores on typed arrays, slices, parameters, and struct
  fields obey a documented common contract; only proven-safe paths omit checks.
  V02 also reproduced invalid `.data` lowering for a scratch argument passed to
  Array<Float64>; safe pending reproducer: `examples/optimization/scratch_array_argument.vy`.
- [ ] **C04 — Scratch copy length and overlap** (high, S).
  `assignments.cpp:397-443` checks boxed but not typed source lengths and uses
  `memcpy` for scratch self-copy. Acceptance: short/long typed sources have the
  same length contract as boxed sources; self-copy is valid; possible overlapping
  views use an appropriate safe operation or a proven-disjoint path.
- [ ] **C05 — Representation-independent escape checking** (high, S/D).
  `members.cpp:319-449`, `collections.cpp:372-416`, `assignments.cpp:295-324`,
  `detail/codegen_helpers.h:584-625` bypass checks for native writes and rely on
  direct roots/name suffixes. Acceptance: native reference fields, aliases,
  compound destinations, and helper mutations cannot silently store temporary
  references into longer-lived objects. Summaries use resolved symbol identity.
- [ ] **C06 — Checked scratch shapes and live stack budget** (high, S).
  `maps_strings_scratch.cpp:170-222,273-337,374-376`, `ctype.h:109-128`,
  `parser.cpp:3595-3621` omit runtime product validation; VLA budgets are per
  allocation and static f32 bytes are overcharged. Acceptance: reject invalid or
  overflowing dimensions before allocation; account actual element bytes and
  simultaneously live buffers on desktop/MCUs. Fallback strategy needs design.
- [ ] **C07 — Justified native no-alias contracts** (high, S).
  `functions.cpp:336-343`, `native_dispatch.cpp:75-90,152-153` emit unconditional
  `restrict` without overlap proof. Acceptance: write/read same-array and
  overlapping-buffer calls agree under optimized builds; restricted variants
  require provenance or runtime overlap guards. Read-only self-dot is insufficient.
- [ ] **C08 — SIMD dependence proof** (high, H).
  `loops.cpp:93-145,193-202,422-425` checks purity, not loop-carried dependence;
  desktop flags ordinarily ignore its OpenMP SIMD pragma. Acceptance: recurrences
  and aliased input/output never receive unsupported SIMD assertions; validate
  with SIMD enabled before exposing a corresponding performance profile.
- [ ] **C09 — Ordered operand and argument capture** (high, S).
  `operators.cpp:29-32,913-923`, `function_calls.cpp:666-707,800-849`,
  `collections.cpp:429-438,545-551` generate expression strings without capturing
  earlier reads. Acceptance: `i + i++`, side-effecting arguments/index/RHS, and
  named arguments observe specified source order; retain lvalue identity separately.
- [ ] **C10 — Lazy logical/ternary/coalescing lowering** (high, S).
  `operators.cpp:29-32`, `detail/codegen_helpers.h:491-500`, `program.cpp:648-655`,
  `language_features.cpp:80-107,160-167` can emit unselected prerequisites eagerly.
  Acceptance: unselected calls, throws, allocations, and division guards do not
  execute; side-effect counters match documented interpreter short-circuit behavior.
- [ ] **C11 — Defined integer arithmetic and typed constants** (high, S/D).
  `literals.cpp:30-33`, `operators.cpp:173-218,283-291,811-815,919-932`,
  `loops.cpp:427-429` inherit C integer-width/overflow hazards. Acceptance: Int64
  literal arithmetic uses Int64 width; define overflow policy, validate division
  MIN/-1, modulo, negation, shifts, and inclusive MAX termination under UBSan.
  Current truncating integer division is not a defect solely because of its name.
- [ ] **C12 — Checked boxed primitive entry and Float32** (high, S).
  `functions.cpp:54-73`, `detail/codegen_helpers.h:118-148` do not validate every
  tag/conversion; boxed f32 entry can initialize to zero. Acceptance: native and
  boxed/defer/try paths preserve f32/f64 values, reject incompatible inputs, and
  apply explicit finite/range rules for floating-to-integer conversion.
- [ ] **C13 — Canonical Float32 array/struct ABI** (high, S).
  `functions.cpp:287-296,383-390,550-560`, `native_dispatch.cpp:200-225,305-310`,
  `control_flow.cpp:249-253` disagree on f32 versus i64 layouts. Acceptance:
  Array<Float32> return, parameter, fallback, default, and struct field paths
  agree. One ABI description drives prototypes, dispatch, binding, and returns.
- [ ] **C14 — Declared interface identity in native dispatch** (high, S).
  `native_dispatch.cpp:92-118`, `functions.cpp:353-397`, `program.cpp:323-359`
  flatten the actual interface without proving it matches the declaration.
  Acceptance: incompatible interfaces reject consistently, including layouts
  that happen to be C-compatible; canonical identity controls field order/types.
- [ ] **C15 — Mutation-aware field caching and hoisting** (high, S).
  `members.cpp:191-289`, `loops.cpp:238-295,341-367,420`, `emitter.h:1086-1091`
  lack complete call/alias/reassignment invalidation and conditional dominance.
  Acceptance: helper mutation, receiver replacement, postfix updates, resize,
  and conditional-only field reads remain correct; invariant hoists need proof.
- [ ] **C16 — Unified exceptional and policy-region cleanup** (high, S/H).
  `control_flow.cpp:79-108,373-401`, `regions.cpp:816-865`, `exceptions.cpp:26-128`,
  `program.cpp:387-395` omit some cleanup or scan only top-level try/defer.
  Acceptance: all supported transfers restore zero modes, bounds/realtime state,
  checkpoints, device/persistent state, finally, and defers once. Audit longjmp
  live locals; unsupported combinations must reject consistently until supported.
- [ ] **C17 — Preserve outer binding during boxed/native reassignment** (high; reproduced in V03).
  Reassignment promotion in `codegen/assignments.cpp` emits a new typed outer
  variable inside a region instead of updating its pre-existing boxed binding.
  `examples/optimization/boxed_native_assignment.vy` prints size 0 instead of 3
  after explicit commit and exits 1. Acceptance: compatible assignments update the
  intended lexical binding across representation changes; preserve alias/lifetime
  contracts without shadow declarations. [#223](https://github.com/t2ncay/vyne/issues/223).

## R: runtime safety and lifetime

- [ ] **R01 — Interned-key ownership and collision safety** (high, S/H).
  `vyne/runtime/detail/maps.h:29-41,73-103`, `arena.h:515-528,588-617` retain
  current-arena canonical pointers in a global table after rewind/thread exit.
  Acceptance: repeated keys across reuse and worker cleanup stay valid; collision
  comparison checks candidate length before reading it. Shared table needs R10.
- [ ] **R02 — Transitive commit/move promotion** (high, S/D).
  `modules/vmem.h:186-232,309-313`, `detail/arrays.h:199-245` clone only some
  reachable reference types; persistent context alone does not prove provenance.
  Acceptance: nested strings, structs, typed arrays, maps/keys, and supported
  cycles survive source rewind/handoff; explicitly define unsupported resources
  and functions. Keep promotion deliberate, not automatic lifetime extension.
- [ ] **R03 — Preserve array contents during growth** (high, S).
  `detail/typed_arrays.h:51-150,358-405`, `arrays.h:120-190`,
  `struct_arrays.h:54-68`, `arena.h:331-342,391-425` reclaim before preserving.
  Acceptance: reserve and capacity-crossing push preserve nonzero elements under
  normal, poisoned, and zeroed modes for all array representations and block paths.
- [ ] **R04 — Zero capacity and mutable descriptor identity** (high, S/D).
  `detail/collections.h:25-56`, `typed_arrays.h:115-193,281-287,390-424`,
  `modules/vmem.h:253-289` allow zero capacity and stale boxed/unboxed descriptors.
  Acceptance: supported empty/drop-then-push grows safely; alias growth/drop
  obeys explicit identity/borrowing rules and cannot expose overwritten storage.
- [ ] **R05 — Checked allocation/capacity arithmetic** (high, S).
  `detail/arena.h:298-359`, `typed_arrays.h:13-40,115-145,346-354`,
  `struct_arrays.h:45-63`, `arrays.h:630-644` permit negative sizes, overflow,
  unbounded signed next-power growth, and place_all narrowing. Acceptance: validate
  before early returns/conversions; bound rounding, doubling, products, and fills;
  report invalid requests before writes. Ordinary OOM already terminates explicitly.
- [ ] **R06 — Portable aligned fresh-block allocation** (high, H).
  `detail/arena.h:87-94,348-393` budgets old-pointer padding then recomputes
  padding from a new base. Acceptance: checked worst-case slack and valid power-
  of-two alignment on malloc fallback/large overrides; keep original free base.
  Normal page-aligned desktop defaults are not evidence of fallback correctness.
- [ ] **R07 — Central device operation validation** (high, S).
  `modules/vdevice.h:364-381,481-696,940-978,1147-1181` permit negative transfers,
  f64 operations on f32 allocations, region/backend mismatch, and offset overflow.
  Acceptance: validate range, byte arithmetic, dtype, handle lifetime, and actual
  allocation backend before CPU/CUDA dispatch; cover nested mixed-backend regions.
- [ ] **R08 — Tensor products and GPU addressing width** (high, S).
  `modules/vdevice.h:1185-1381,1637-1667`, `device/cuda/kernels.cu:50-77,161-250`,
  `modules/vconv.h:123-165,504-582,713-722` narrow dimensions/products and index
  with signed int; pixel shuffle squares before validation. Acceptance: checked
  host products and consistent 64-bit addressing or explicit representable limits;
  boundary-shape tests must not require enormous allocations to validate rejection.
- [ ] **R09 — BLAS cache coherence, lifetime, and shape checks** (high, S).
  `detail/blas_bridge.h:43-108`, `modules/vmem.h:133-183`, `arena.h:588-617`
  cache boxed conversion by payload pointer without mutation/lifetime versioning.
  Acceptance: mutation/resize/rewind/reset/thread exit cannot reuse stale converted
  data; validate inner dimensions, backing length, and BLAS integer limits.
- [ ] **R10 — Runtime execution state and shared registry concurrency** (high, H).
  `detail/maps.h:39-102`, `exceptions.h:35-65`, `structs.h:19-42`,
  `modules/vyne_stream_runtime.h:59-70,123-208`, `vdevice.h:105-118,339-435`,
  `vmath.h:207-225` retain global mutable state despite TLS arenas. Acceptance:
  isolate exception/RNG/frame state; synchronize shared registries; make shared
  struct reads nonmutating; establish worker CUDA contexts; audit character pool
  and poison initialization with real concurrent tests where supported.
- [ ] **R11 — Pool/slab slot ownership and alias frees** (high, S).
  `modules/vyne_pool_runtime.h:35-115`, `vyne_slab_runtime.h:177-211,325-359`
  do not reliably detect aliased double free/wrong storage. Acceptance: validate
  slot boundaries/region/allocated state before pointer arithmetic; reject detached
  grown buffers and double frees; two live allocations cannot receive one slot.
- [ ] **R12 — FFT plan lifetime and input shape** (high, S/H).
  `vyne/modules/external/vfft/Ops.vy:25-100`, `Kernels.vy:24-40,66-115` retain
  arena-created plans globally and accept invalid radix-2 sizes. Acceptance:
  first plan inside a temporary region cannot become stale; real/imag lengths and
  powers of two validate; ownership/concurrency are explicit. Existing prewarming
  benchmarks mask the lifetime case.
- [ ] **R13 — Checkpoint identity and generations** (high, H).
  `modules/vmem.h:120-150`, `detail/arena.h:588-616` reuse stack-index handles
  and lack full arena/block membership checks. Acceptance: stale handles,
  persistent-context mismatches, moved blocks, and foreign checkpoints reject
  before freeing/restoring the wrong arena; valid nested checkpoints still work.
- [ ] **R14 — Nullable strings and checked ranges** (high, S).
  `detail/values.h:43-52,80-84`, `strings.h:13-34`, `maps.h:301-311` inconsistently
  accept NULL and narrow indices; substring addition can overflow. Acceptance:
  define nullable representation, validate before dereference/narrowing, and cover
  huge/negative index and substring endpoints according to common semantics.
- [ ] **R15 — Empty-struct stringification size** (high, S).
  `detail/conversions.h:201-227` allocates type_len+4 but emits type_len+6 bytes
  for no fields. Acceptance: empty struct at allocator-rounding boundaries cannot
  overwrite adjacent allocations; ordinary/nonempty stringification remains exact.
- [ ] **R16 — Random integer interval arithmetic** (high, S).
  `modules/vmath.h:228-236` can narrow range 2^32 to zero before modulo and
  overflow hi-lo+1. Acceptance: validate interval, support/document full-width
  behavior and distribution contract, avoid modulo zero and signed overflow.
- [ ] **R17 — Model-loader checked dimensions and partial allocation** (high, S).
  `modules/vconv.h:848-860,888-896` multiply before validation and allocate
  declared text shapes before checking values. Acceptance: malformed/overflowing/
  truncated shapes fail predictably; partial loads have explicit rollback or
  bounded-lifetime behavior. Preserve valid binary/text model compatibility.
- [ ] **R18 — Histogram conversion validation** (high, S).
  `vstat/Kernels.vy:136-144`, `Histogram.vy:15-27` convert nonfinite/extreme
  samples before clamping. Acceptance: validate nonfinite sample handling and
  bin-width/underflow rules before conversion; preserve documented edge bins.
- [ ] **R19 — MCU allocator arithmetic and alignment** (high, S/H).
  `runtime/esp32/detail/mcu_arena.h:22-24,60-79` and
  `runtime/arduino/detail/mcu_arena.h:12-14,48-66` repeat unchecked addition and
  ignore requested alignment. Acceptance: allocation has representable bounds,
  real backing/requested alignment, and documented OOM behavior on AVR/ESP32.
- [ ] **R20 — Portable host Int64 formatting** (high, S; discovered through V02).
  Diagnostic GCC on LP64 Linux reports `%lld`/`int64_t` type mismatches in
  `runtime/detail/printing.h:22` and `conversions.h:76`. These are reproduced
  warnings, not a claimed crash. Acceptance: use a portable fixed-width format
  contract, audit related host formatting, and assert printed/stringified Int64
  boundaries on Windows and Linux with format warnings enabled. Keep values and
  string semantics unchanged. [#222](https://github.com/t2ncay/vyne/issues/222).

## P: measurable executable-performance opportunities

- [ ] **P01 — Nonretained stack call argument vectors** (high, B/D).
  `codegen/function_calls.cpp:753-768,1032-1046` arena-allocates argument vectors.
  Acceptance: prove the vector address cannot escape; use stack storage without
  shortening payload lifetime; test recursion/closures/boxed calls; measure bytes
  per call and throughput. Scalar boxing itself is not a heap allocation.
- [ ] **P02 — Broader native array wrapper dispatch** (high, B/D; C03/C13/R04).
  `codegen/program.cpp:397-427` rejects native variants using array methods.
  Acceptance: length-preserving views enable size/index/forwarding without boxing,
  while mutable metadata keeps declared sharing semantics; measure wrapper workloads.
- [ ] **P03 — Native struct parameters and returns** (high, B/D; C05/C13/C14).
  `native_dispatch.cpp:101-110`, `functions.cpp:426-482`, `emitter.h:909-922`
  box/rebuild structs; struct-array conversion is O(N). Acceptance: canonical
  native ABI avoids conversions without replacing shared reference mutation with
  accidental C by-value behavior; measure interface-heavy loops.
- [ ] **P04 — Direct collect/filter output construction** (high, B; C09/C11/R05).
  `loops.cpp:559-577,618-651` pushes repeatedly; 611/696 can evaluate iterable
  twice. Acceptance: one evaluation, checked count, one allocation, direct collect
  writes/filter cursor, correct logical length; benchmark empty and large inputs.
- [ ] **P05 — Bounded arena block reuse** (high, B/D; R03/R05/R13).
  `runtime/detail/arena.h:97,298-333,601-606` frees post-checkpoint blocks; an
  empty-arena loop can repeatedly map/free 8 MiB. Acceptance: preserve logical
  reclamation and poison, bound retained capacity, provide trimming/accounting,
  and demonstrate lower block churn without hidden unbounded memory retention.
- [ ] **P06 — Avoid redundant initialization and conversion copies** (medium, B/D).
  Typed constructors zero-fill before some kernels overwrite all elements; typed
  concatenation may fall back to boxed elements. Acceptance: uninitialized paths
  require complete-write proof and honor @zeroed; dtype-preserving operations avoid
  O(N) boxing. Same-dtype array boxing is O(1) descriptor allocation, not payload copy.
- [ ] **P07 — CPU GEMM locality and validated BLAS** (high, B; R09/C07).
  `modules/external/vlin/Kernels.vy:163-193`, `vml/Kernels.vy:40-105` use strided
  naive matrix loops. Acceptance: compare blocked/packed kernels and BLAS across
  shapes, small/large crossover, memory, aliasing, and numerical tolerance; keep
  portable reference kernels for memory-model comparisons.
- [ ] **P08 — Prepared convolution weights** (high, B/D; R08/R17).
  `runtime/modules/vconv.h:156-180,240` allocates/transposes/frees each call.
  Acceptance: explicitly owned versioned preparation avoids repeated work; weight
  mutation invalidates it; measure cold/warm inference and retained memory.
- [ ] **P09 — Safe in-place FFT boundary** (high, B/D; R12/C07).
  `modules/external/vfft/Ops.vy:62-99` copies both components in and out.
  Acceptance: expose/document mutation/borrowing and alias constraints, preserve
  outputs/numerical error, measure copies/allocations and transform runtime.
- [ ] **P10 — Device buffer reuse and indexed handles** (high, B/D; R07/R10).
  `runtime/modules/vdevice.h:339-377,413-434` scans allocation records per
  operand and retains allocations until exit. Acceptance: indexed generational
  handles reject stale resources; caller-controlled bounded buffers avoid training
  allocation growth; measure lookup scaling and peak device memory.
- [ ] **P11 — Multi-block GPU reduction** (high, B/D; R07/R08).
  `runtime/device/cuda/kernels.cu:131-158`, `modules/vdevice.h:904-923` use one
  block for any input length. Acceptance: bounded reusable partials, correct
  edge/tail behavior, explicit numerical/deterministic contract, real GPU event
  measurements over sizes/dtypes; no assumed bitwise equivalence after reassociation.
- [ ] **P12 — Dependency-aware transfer/synchronization policy** (high, B/D).
  `modules/vdevice.h:413-428,481-558,1773-1775` synchronizes repeatedly and uses
  synchronous transfers; some sync errors are ignored. Acceptance: first surface
  errors; remove demonstrably redundant sync; async streams/pinned buffers need
  explicit lifetime rules. Measure transfer, kernel, cold/warm application time.
- [ ] **P13 — GPU convolution locality** (high, B/D; R07/R08).
  `runtime/device/cuda/kernels.cu:193-250` uses direct one-thread/output loops.
  Acceptance: benchmark tiled/shared-memory or appropriate vendor-kernel paths,
  validate dtype/shape/numerical output and portability, disclose workspace/cost.
- [ ] **P14 — String traversal and concatenation scaling** (medium, B/D; R14).
  `runtime/detail/maps.h:306-311` strlen per index can make traversal quadratic;
  `operators.h:43-70` copies accumulated prefixes. Acceptance: suitable length
  metadata/builders preserve byte/string semantics and lifetimes; benchmark scaling.
- [ ] **P15 — Map probing and rehash cost** (medium, B/D; R01/R10).
  `runtime/detail/maps.h:127-174` strcmp probes lack stored-hash comparison;
  rehash recomputes hashes and retains old buffers until rewind. Acceptance:
  collisions/deletion/rewind remain correct; measure probe count, bytes, lookup,
  and rehash workloads before selecting layout changes.
- [ ] **P16 — Rolling statistics and ranking algorithms** (high, B/D; R18).
  `vsignal/Kernels.vy:15-26,82-123`, `vstat/Kernels.vy:149-162` moving average
  O(nw), median O(nw^2), ranks O(n^2). Acceptance: rolling sum/window structures/
  sort-based ranking preserve edge/tie/nonfinite rules and numerical accuracy;
  measure asymptotic scaling and additional storage, split distinct algorithms.
- [ ] **P17 — Persistent arena lock contention** (medium, B/H/D; R02/R10).
  `runtime/detail/arena.h:126-155`, `modules/vmem.h:79-95` spin without backoff
  and hold the lock across persistent bodies. Acceptance: document nested/thread
  wait hazards; choose ownership-compatible synchronization and measure contention,
  fairness, throughput, and footprint without changing persistent semantics.
- [ ] **P18 — Explicit optimized build profiles** (medium, B/D; C07/C08/C11).
  `cli/file_handler.cpp:961-987` already uses O3, optional native ISA and BLAS;
  AVR uses Os, ESP32 Arduino properties. Acceptance: portable/host-native/diagnostic
  profiles are explicit; benchmark PGO and relevant LTO, vectorization reports,
  code size, startup, and reproducibility. Single-TU generated C already enables
  substantial interprocedural optimization; LTO is not an automatic large win.

## A: incremental architecture

- [ ] **A01 — Typed lowering between AST and C emission** (medium, D).
  Today type/optimization decisions are spread among parser symbols, AST static
  types, emitter metadata, and native dispatch; no standalone optimizer/typechecker
  directory establishes a central contract. Introduce incrementally for sequencing,
  calls, and cleanup: explicit types, shape/length, allocation provenance, effects,
  aliases, control flow, and ABI descriptors. Acceptance: one migrated vertical
  slice preserves behavior, simplifies proofs, and has measured compiler/runtime
  cost. A full rewrite or new user-facing ownership syntax is not authorized here.

## L: reconciliation of the replaced roadmap

Historical DONE labels are retained here as **historical claims**, not newly
verified successes. The old 1024x1024 comparison (13.36 s C vs 13.90 s Vyne,
roughly 3.9%) does not establish the cause or expected gain of any current change.
C optimizers often eliminate local temps; register-pressure claims require actual
assembly/optimization-report evidence. Old hour estimates and promised percentage
gains are superseded by reproductions and measurements.

- [ ] **L01 — Verify historically landed fixes** (medium, S/B).
  Old O0.1 duplicate scratch store, O0.2 shape-add detection, O1.1 identity folding,
  O1.2 literal-divisor guard elision, O2.1 32-byte typed alignment, O3.2 native
  array ABI, and field hoisting were marked done. Confirm emitted output/regressions;
  do not repeat them or assume current ABI/lifetime/hoist safety. IEEE signed zero,
  NaN, side effects, and unselected branches constrain peepholes.
- [ ] **L02 — Temp-to-assignment fusion and single-use temp inlining** (medium, B/D).
  Old O1.3/O1.4: `codegen/operators.cpp`, `assignments.cpp`, `emitter.h`.
  Acceptance: preserve sequencing/types/diagnostics and demonstrate actual optimized
  output improvement; avoid fragile textual expression substitution without effects.
- [ ] **L03 — Loop-bound simplification and scratch row pointers** (medium, B/D).
  Old O1.5/O1.6: `loops.cpp`, `maps_strings_scratch.cpp`.
  Acceptance: bounds capture semantics, overflow and index checks remain correct;
  invariant row addresses require proof; measure assembly and representative kernels.
- [ ] **L04 — place_all and proven-capacity direct writes** (medium, B/D).
  Old O2.2/O2.4: `runtime/detail/arrays.h`, `typed_arrays.h`.
  Acceptance: checked count/size contract and proven capacity before unchecked fill;
  repeated VyneValue fill is not memcpy of a single element. Coordinate P04/R05.
- [ ] **L05 — Slow operator dispatch branch ordering** (low, B).
  Old O2.3: `runtime/detail/operators.h`. Acceptance: mixed numeric/string/array/
  logical behavior unchanged; profile branch distribution first and benchmark more
  than ML alone. Preserve already-fast native paths.
- [ ] **L06 — Range analysis, uniqueness, and shape-aware tensor design** (medium, D).
  Old O3.1/O3.3/O3.4: range-typed induction, linear/unique in-place operations,
  unified Tensor CType. Acceptance: pursue internal range/provenance first; new
  syntax or ownership semantics need a separate design decision. Shapes alone do
  not prove lifetimes/noalias. Coordinate A01/C03/C07/C11/P02.
- [ ] **L07 — Fusion, typed default outputs, and stable statistics** (medium, B/D).
  Old O4.1/O4.2/O4.3: matmul+bias+activation, typed numerical storage, Welford.
  Audit existing vml fused kernels first; quantify avoided passes/bytes and error
  before adding APIs. Typed elements can halve payload size versus boxed elements,
  not necessarily halve application RSS. Statistical algorithms need edge contracts.
- [ ] **L08 — Portable baselines and explicitly deferred layout changes** (medium, D).
  Preserve portable reference comparisons alongside tiled/GPU candidates; the old
  paper-specific ban is not a ban on separately measured product optimization.
  No default fast-math/native ISA, unsafe packed VyneValue shrinking, or unproved
  SIMD/unchecked access. Layout repacking and new language features remain separate
  designs; record exact baseline flags and distinguish algorithm changes.

## Existing coverage to reuse and gaps to close

- `examples/transpiler/region_test.vy:69-116,188-202`: loop containing region,
  bare Array return; add inverse nesting and native typed return with reuse.
- `examples/array_abi_test.vy`, `examples/slice_reserve_test.vy`: valid native ABI,
  slice/reserve; add invalid index, metadata aliases, and escaping typed returns.
- `examples/transpiler/scratch_assign_test.vy`: equal distinct buffers; add short
  typed source, self-copy, and overlapping copy.
- `examples/safety/`: scratch bounds/direct boxed escapes; add native reference
  fields, indirect mutations, and robust expected-failure checking.
- `examples/transpiler/named_args_test.vy`, `native_zero_side_effects.vy`: literal
  reorder/discarded calls; add operand captures and lazy branches with side effects.
- `examples/transpiler/native_params_test.vy`, `try_test.vy`,
  `region_rules_smoke.vy`, `examples/test/float32_test.vy`: boxed entry/top-level
  try/normal policy exits/f32 scalars; add boxed f32, array returns, nested cleanup.
- `vyne/runtime/tests/alignment.c`, `poison.c`, `slab.c`, `tls_arena.c`,
  `persistent.c`: add poison+growth, zeroed+growth, f32, overflow/fallback alignment,
  aliased frees, nested promotion, maps/exceptions/shared readers/caches in threads.
- `examples/libs/vfft_bench.vy:28-35` prewarms plans outside loop regions;
  add first-plan-inside-region and real input validation.

## Results policy

Record before/after revisions, compiler/tool versions, OS/CPU/GPU/backend, flags,
workload/checksums/tolerance, warmups/repetitions, raw samples, and exact commands.
Separate compile/startup/kernel/application timing; exclude failures from successful
statistics. Record allocation bytes/count, peak logical arena usage, process memory,
and device memory with clear measurement limitations. Report safety, correctness,
user experience, reproducibility, overhead, and regressions even without speed gain.
Compare the original baseline to the latest verified revision directly; never sum
independent percentage gains. Missing metrics are **not measured**, never zero.

## Implementation ledger

### GitHub sub-issue index

All 72 tasks (70 original plus R20 and C17 follow-ups) are attached using
GitHub's native sub-issue relationship, not only textual references. Each issue
links back to its detailed entry above.

| Group | ID -> issue |
| --- | --- |
| Validation | V01 [#151](https://github.com/t2ncay/vyne/issues/151), V02 [#152](https://github.com/t2ncay/vyne/issues/152), V03 [#154](https://github.com/t2ncay/vyne/issues/154), V04 [#156](https://github.com/t2ncay/vyne/issues/156), V05 [#158](https://github.com/t2ncay/vyne/issues/158), V06 [#160](https://github.com/t2ncay/vyne/issues/160), V07 [#162](https://github.com/t2ncay/vyne/issues/162), V08 [#164](https://github.com/t2ncay/vyne/issues/164) |
| Compiler (1) | C01 [#166](https://github.com/t2ncay/vyne/issues/166), C02 [#168](https://github.com/t2ncay/vyne/issues/168), C03 [#170](https://github.com/t2ncay/vyne/issues/170), C04 [#173](https://github.com/t2ncay/vyne/issues/173), C05 [#175](https://github.com/t2ncay/vyne/issues/175), C06 [#177](https://github.com/t2ncay/vyne/issues/177), C07 [#179](https://github.com/t2ncay/vyne/issues/179), C08 [#181](https://github.com/t2ncay/vyne/issues/181) |
| Compiler (2) | C09 [#182](https://github.com/t2ncay/vyne/issues/182), C10 [#184](https://github.com/t2ncay/vyne/issues/184), C11 [#186](https://github.com/t2ncay/vyne/issues/186), C12 [#188](https://github.com/t2ncay/vyne/issues/188), C13 [#190](https://github.com/t2ncay/vyne/issues/190), C14 [#192](https://github.com/t2ncay/vyne/issues/192), C15 [#194](https://github.com/t2ncay/vyne/issues/194), C16 [#197](https://github.com/t2ncay/vyne/issues/197) |
| Compiler follow-up | C17 [#223](https://github.com/t2ncay/vyne/issues/223) |
| Runtime (1) | R01 [#199](https://github.com/t2ncay/vyne/issues/199), R02 [#201](https://github.com/t2ncay/vyne/issues/201), R03 [#203](https://github.com/t2ncay/vyne/issues/203), R04 [#205](https://github.com/t2ncay/vyne/issues/205), R05 [#207](https://github.com/t2ncay/vyne/issues/207), R06 [#209](https://github.com/t2ncay/vyne/issues/209), R07 [#211](https://github.com/t2ncay/vyne/issues/211), R08 [#213](https://github.com/t2ncay/vyne/issues/213), R09 [#215](https://github.com/t2ncay/vyne/issues/215) |
| Runtime (2) | R10 [#153](https://github.com/t2ncay/vyne/issues/153), R11 [#155](https://github.com/t2ncay/vyne/issues/155), R12 [#157](https://github.com/t2ncay/vyne/issues/157), R13 [#159](https://github.com/t2ncay/vyne/issues/159), R14 [#161](https://github.com/t2ncay/vyne/issues/161), R15 [#163](https://github.com/t2ncay/vyne/issues/163), R16 [#165](https://github.com/t2ncay/vyne/issues/165), R17 [#167](https://github.com/t2ncay/vyne/issues/167), R18 [#169](https://github.com/t2ncay/vyne/issues/169), R19 [#171](https://github.com/t2ncay/vyne/issues/171) |
| Runtime follow-up | R20 [#222](https://github.com/t2ncay/vyne/issues/222) |
| Performance (1) | P01 [#172](https://github.com/t2ncay/vyne/issues/172), P02 [#174](https://github.com/t2ncay/vyne/issues/174), P03 [#176](https://github.com/t2ncay/vyne/issues/176), P04 [#178](https://github.com/t2ncay/vyne/issues/178), P05 [#180](https://github.com/t2ncay/vyne/issues/180), P06 [#183](https://github.com/t2ncay/vyne/issues/183), P07 [#185](https://github.com/t2ncay/vyne/issues/185), P08 [#187](https://github.com/t2ncay/vyne/issues/187), P09 [#189](https://github.com/t2ncay/vyne/issues/189) |
| Performance (2) | P10 [#191](https://github.com/t2ncay/vyne/issues/191), P11 [#193](https://github.com/t2ncay/vyne/issues/193), P12 [#195](https://github.com/t2ncay/vyne/issues/195), P13 [#196](https://github.com/t2ncay/vyne/issues/196), P14 [#198](https://github.com/t2ncay/vyne/issues/198), P15 [#200](https://github.com/t2ncay/vyne/issues/200), P16 [#202](https://github.com/t2ncay/vyne/issues/202), P17 [#204](https://github.com/t2ncay/vyne/issues/204), P18 [#206](https://github.com/t2ncay/vyne/issues/206) |
| Architecture | A01 [#208](https://github.com/t2ncay/vyne/issues/208) |
| Legacy reconciliation | L01 [#210](https://github.com/t2ncay/vyne/issues/210), L02 [#212](https://github.com/t2ncay/vyne/issues/212), L03 [#214](https://github.com/t2ncay/vyne/issues/214), L04 [#216](https://github.com/t2ncay/vyne/issues/216), L05 [#217](https://github.com/t2ncay/vyne/issues/217), L06 [#218](https://github.com/t2ncay/vyne/issues/218), L07 [#219](https://github.com/t2ncay/vyne/issues/219), L08 [#220](https://github.com/t2ncay/vyne/issues/220) |

| ID | Status | Evidence/results | Commit |
| --- | --- | --- | --- |
| Setup | Documentation and 70 native sub-issues established | No implementation gain claimed | `1d80a09` |
| V01 | Merged via PR #221 (`00f1e7a`) | Windows baseline reproduced; Windows and Linux acceptance plus existing regressions pass; all six CI checks at `9180e90` passed | `9180e90` |
| V02 | Merged via PR #221 (`00f1e7a`) | Both baselines reproduced; 28 Windows and 38 Linux/WSL checks pass; all six CI checks at `86b6c1b` passed | `86b6c1b` |
| V03 | Merged via PR #221 (`00f1e7a`) | Empty/false-pass baselines reproduced; strict Windows/Linux core and runner regressions, compatibility entries, and all 21 Windows registered contracts pass; all six CI checks passed | `3d441c4` |
| V04 | Merged via PR #221 (`00f1e7a`) | Polling inflation and missed allocation burst reproduced; 17 measurement/export contracts pass per platform; Windows 21 registered contracts and Linux optimization/core/numerical suites pass; all six CI checks at `1b17ed6` passed | `1b17ed6` |
| R20 | Planned | LP64 Int64 format warnings exposed by V02 | Pending |
| C17 | Reproduced, planned solution | Boxed/native reassignment shadows outer binding; safe pending source retained | Pending |

All other IDs are planned until their issue and ledger entry say otherwise.
