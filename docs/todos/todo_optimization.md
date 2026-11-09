# TODO — Optimization

**Last updated:** 2026-11-10

Draft, partially executed. Companion to `todo.md` (Paper 1 pipeline) and
`todo_2.md` (feature axes). This file is about making Vyne **fast** — closing
the gap to hand-written C and, where possible, going past it. Ordered by ROI,
not by ambition.

> **Status snapshot (2026-11-10).** Done: Tier 0 (`O0.1`, `O0.2`), identity
> folding and literal-divisor elision (`O1.1`, `O1.2`), 32-byte typed-array
> alignment (`O2.1`), and the native array ABI (`O3.2`). Loop-invariant field
> loads are hoisted (`loops.cpp`), which covers the spirit of `O1.6` for
> struct-heavy loops but not the row-pointer form. Still open: `O1.3` temp +
> assignment fusion, `O1.4` single-use temp inlining, `O1.5` loop-bound
> inlining, `O2.2` `place_all` fast path, `O2.3` `vyne_binop_slow` branch
> reorder, `O2.4` unchecked push, `O3.1` range-typed induction variables,
> `O3.3` uniqueness/linear types, `O3.4` unified `Tensor` CType. `O2.5` remains
> an explicit do-not-do.

Context: on the §5.7 1024×1024 matmul at ITERS=100, hand-written C runs
~3.9% faster than Vyne config 2 on clean runs (13.36 s vs 13.90 s means,
outlier-excluded). The gap is real but small, and it's attributable to
register pressure from the emitter's temp-heavy output. Everything in
Tier 1 and Tier 2 is about that gap. Tiers 3 and 4 are structural.

---

## Tier 0 — Bugs (do first, 30 min, free)

Two things in the current tree cost cycles silently. Fix before anything
else; if Tier 1 is measured against unfixed Tier 0, the numbers are wrong.

### O0.1 — Delete the duplicate emit in `IndexAssignmentNode::compile` [ DONE ]

**File:** `codegen/collections.cpp` [ DONE ]

The scratch branch of `IndexAssignmentNode::compile` emits the same
assignment twice:

```cpp
e.emit(bRaw + "[" + iv + "] = " + val + ";");
e.emit(bRaw + "[" + iv + "] = " + val + ";");   // ← duplicate
```

Visible in the emitted `.vy.c` from any scratch store — e.g.
`v_db2_buf[si_1555] = 0;` on consecutive lines in `ml_seq.vy.c`.

GCC's DSE pass eliminates the redundant store, so the wall-clock cost is
small, but the second write occupies a register slot in the intermediate
IR and can perturb register allocation. Delete the second line.

**Effort:** 1 minute. **Expected gain:** measurable in IR, small in wall clock.

### O0.2 — `exprNativeType` rejects arrays, so the shape-add fast path is dead [ DONE ]

**File:** `codegen/emitter.h`, `codegen/operators.cpp`

`C_Emitter::exprNativeType` is defined as:

```cpp
const CType* exprNativeType(const std::string& expr) const {
    const CType* ct = lookupType(expr);
    return (ct && ct->isPrimitive()) ? ct : nullptr;   // ← rejects arrays
}
```

`CType::Kind::Array` is not primitive, so this always returns null for
typed arrays and scratch. The shape-add fast path in `BinOpNode::getCExpr`:

```cpp
const CType* lct = e.exprNativeType(l);
const CType* rct = e.exprNativeType(r);
if (lct && rct && lct->hasShape() && rct->hasShape() && ...) { ... }
```

never fires. Every scratch `A + B` falls through to `vyne_binop`, which
boxes, dispatches, and unboxes per element.

**Fix:** use `e.lookupAnyType(l)` and `e.lookupAnyType(r)` in the shape-add
branch. One-word change per line.

Verify by grepping any `.vy.c` for `arrsum_` — if the elementwise-loop
lowering is firing, you'll see `double arrsum_N[...];` followed by a scalar
for-loop. If you only see `vyne_binop(..., 29)`, this bug is active.

**Effort:** 5 minutes. **Expected gain:** unlocks the whole shape-add
lowering that's already written; enables elementwise scratch+scratch to
compile to a bare C loop.

---

## Tier 1 — Emitter peepholes (~half a day, high ROI)

Small changes to the emit path. Each removes temps, redundant checks, or
literal arithmetic from the emitted C. The cumulative effect is the 3.9%.

### O1.1 — Constant folding for `+ 0`, `- 0`, `* 1`, `/ 1` [ DONE ]

**File:** `codegen/operators.cpp`, in `BinOpNode::getCExpr`

Before emitting a binop, check the AST operands. If either is a
`NumberNode` with a literal identity value, fold:

- `x + 0`, `0 + x` → `x`
- `x - 0` → `x`
- `x * 1`, `1 * x` → `x`
- `x / 1` → `x`
- `x * 0`, `0 * x` → `0` (careful: only when the other operand is proven pure)
- `x + x` — leave alone; not worth the temp-aliasing risk

Also check the "either literal 0 or 1" case where the operand is a
`nativeLiteral()`-capable node, not only a raw `NumberNode`.

**Why it matters:** every `k0 + 0`, `k0 + 1`, `k0 + 2`, `k0 + 3` in the
matmul inner loop is currently emitted as `bin_NN = v_k0 + 0;`. Four
redundant instructions per inner iteration × 256 iterations × 1M cells.

**Effort:** 1 hour. **Expected gain:** measurable; removes one add per
unrolled lane.

### O1.2 — Drop the zero-check when the divisor is a literal [ DONE ]

**File:** `codegen/operators.cpp`, `Int64 op Int64` and `Float64 op Float64` paths

The emitter currently emits:

```c
if (4 == 0) { fprintf(stderr, "Runtime error: Division by zero!\n"); exit(1); }
int64_t bin_52 = v_N / 4;
```

For `N/4` where 4 is a literal. The branch is dead at compile time.

**Fix:** before the `Division`, `Floor_Divide`, and `Modulo` cases, check
`rightNode->type() == NodeType::NUMBER`. If the numeric value is non-zero,
skip the check. If it is zero, emit a compile-time error (you can literally
throw at emit time).

Apply the same to the `Float64` path.

**Effort:** 30 minutes. **Expected gain:** removes a dead branch in the
matmul inner setup.

### O1.3 — Fuse binop temp + assignment

**Files:** `codegen/operators.cpp`, `codegen/assignments.cpp`

The emitted C currently contains the pattern:

```c
int64_t bin_57 = v_k4 * 4;
int64_t v_k0 = bin_57;
```

Two temporaries for one value. Two register slots in the IR.

**Fix:** add an overload `BinOpNode::emitInto(C_Emitter& e, const std::string& target)`
that emits into a caller-supplied name instead of allocating a fresh temp.
In `AssignmentNode::compile`, when the RHS is a `BinOpNode` and the LHS is
a fresh declaration of a native type, call `emitInto` with the LHS name.

Same idea for `nativeInit` in `assignments.cpp` — when the RHS is a binop
of a compatible native kind.

**Effort:** 1.5 hours. **Expected gain:** ~2 temps removed per assignment
in hot loops; measurable in register pressure.

### O1.4 — Inline single-use temps

**File:** `codegen/emitter.h` (add tracking), `codegen/operators.cpp`, etc.

The single biggest register-pressure win, and the one that most directly
closes the gap to hand-written C.

**Concept:** track, in the emitter, whether each emitted temp is read
exactly once. If the read is the immediately-following emitted statement,
substitute the RHS expression inline and don't materialize the temp.

**Implementation sketch:**

- Add `std::unordered_map<std::string, std::string> pendingTemps;` to the emitter.
- When `emit("int64_t " + temp + " = " + rhs + ";")` is called, record
  `pendingTemps[temp] = rhs`.
- When a subsequent emit reads `temp` for the first time, if it's the
  very next emit and `temp` appears exactly once in that line, replace the
  reference with `rhs` and remove the pending temp.
- Otherwise, keep the temp and drop the pending entry.

This is fragile to get right — order-of-evaluation and side-effect concerns
matter. Start with the narrow case: an emit of the form
`<type> <temp2> = <expression containing temp>;` immediately after
`<type> <temp> = <expression>;` and where `<temp>` appears exactly once in
`<temp2>`'s expression.

**Effort:** 3–4 hours, careful. **Expected gain:** the big one. Hand-written
C's four-accumulator loop fits in registers; Vyne's temp-heavy output does
not. This is where most of the 3.9% lives.

### O1.5 — Inline single-use loop bounds

**File:** `codegen/loops.cpp`, `ForNode::compile`

Current output:

```c
int64_t lo_i_15 = 0;
int64_t hi_i_16 = bin_14;
for (int64_t i_17 = lo_i_15; i_17 <= hi_i_16; ++i_17) {
```

**Fix:** when `lo` and `hi` are `NumberNode`s or already-declared natives,
inline them into the `for` header directly:

```c
for (int64_t i = 0; i <= bin_14; ++i) {
```

Same for `loT` and `hiT` in the `collect`/`every`/`filter` fast paths.

**Effort:** 1 hour. **Expected gain:** two temps gone per loop; cleaner
emitted C, easier to diff against hand-written baselines.

### O1.6 — Hoist row pointers for rank-2 scratch

**File:** `codegen/maps_strings_scratch.cpp`, `scratchFlatIndex`

The hand-written C does:

```c
const double* Arow = &A[r * N];
const double* B_Trow = &B_T[c * N];
```

Vyne emits `A[r * 1024 + k0 + 0]`. GCC's strength-reduction pass will get
there, but starting from the hoisted form gives it a head start and avoids
keeping the intermediate `r * 1024` live.

**Fix:** in `scratchFlatIndex`, when the base is rank-2 and the outer index
is a loop induction variable, emit a hoisted pointer temp at the enclosing
loop head. This requires tracking which loop induction variable we're
inside — a small extension to the emitter's scope state.

Simpler alternative: emit `(A + r * 1024)[k0 + 0]` instead of
`A[r * 1024 + k0 + 0]`. Same semantics, slightly friendlier to GCC's
address-mode selection. Zero risk, ~10 minutes.

**Effort:** 10 min (easy form) to 2 hours (full hoisting). **Expected gain:**
1–2% on the matmul; varies by kernel.

---

## Tier 2 — Runtime header (~1 day)

Changes to `runtime/detail/arena.h`, `typed_arrays.h`, `arrays.h`,
`operators.h`.

### O2.1 — 32-byte alignment for typed arrays [ DONE ]

**File:** `runtime/detail/arena.h`

`arena_alloc` rounds to 8 bytes. Change (or add an aligned variant) to 32
for `VyneArray_f64` and `VyneArray_i64` allocations. GCC can then emit
aligned loads (`movapd`, `vmovapd`) instead of unaligned, which matters at
AVX2 and AVX-512 widths.

**Cheap version:** overallocate by 32 bytes, round the returned pointer up
to the next 32-byte boundary, and keep the original pointer for freeing.
Costs 32 bytes per typed array; gets alignment for free. Arena never frees
individual allocations, so no bookkeeping needed.

**Better version:** make the bump pointer itself always 32-aligned after
each typed-array alloc. Same effect, no waste.

**Effort:** 1 hour. **Expected gain:** 1–3% on vectorized kernels; nothing
on scalar.

### O2.2 — `vyne_array_place_all` fast path

**File:** `runtime/detail/arrays.h`

Current implementation checks `count > capacity` and then writes element by
element. When capacity is already sufficient — the common case for a
correctly-sized fresh array — the loop can be a `memcpy`-style fill. For
`VyneValue` elements, this is a repeated `= val;` — same cost as the loop,
but the branch on capacity disappears.

**Effort:** 30 min. **Expected gain:** small, no risk.

### O2.3 — Reorder branches in `vyne_binop_slow`

**File:** `runtime/detail/operators.h`

Currently checks string concat, then array concat, then `AND`/`OR`, then
float math. For ML, float+int and float+float dominate.

**Fix:** reorder so `is_float_math` (either operand is `V_FLOAT64`) is
checked first. The branch predictor learns the new order.

**Effort:** 15 min. **Expected gain:** small; matters in tight loops.

### O2.4 — Unchecked variant of `vyne_array_push`

**Files:** `runtime/detail/arrays.h`, `runtime/detail/typed_arrays.h`

For the case where the caller knows `size < capacity` — which the codegen
can prove for fresh arrays — expose a `vyne_array_push_unchecked` that
skips the capacity branch. Use it in the `collect` fast path when the
destination was sized with `vyne_array_create(N)` and the loop bound is
known.

**Effort:** 1 hour (runtime + codegen use site). **Expected gain:** small
but non-zero in collection-heavy code.

### O2.5 — `VyneValue` layout tightening

**File:** `runtime/detail/types.h`

`VyneValue` currently has `VyneType type; uint32_t _reserved; union {...};`.
That's 16 bytes with padding. If `VyneType` fits in 8 bits and `_reserved`
can be repurposed as the high 24 bits of a tag, `VyneValue` could shrink to
12 bytes padded to 16 — no change. To actually shrink to 12, you'd need
`#pragma pack` or manual alignment, which breaks the array layout that the
whole runtime assumes.

**Do not do this without a plan.** Every array of `VyneValue` would need
repacking. Not worth it; note as a long-term consideration, not a Tier 2
item.

---

## Tier 3 — Feature-level (weeks each; already in `todo_2.md`)

These are what make Vyne architecturally faster, not just matching C. Each
is a paper-worthy feature on its own; cross-reference `todo_2.md` for the
full design.

### O3.1 — Range-typed induction variables (F6 from `todo_2.md`)

`k4 :: 0..<N/4` gives `k4` the type `Int64<0..N/4-1>`. Any expression built
from `k4` by `+`, `-`, `*` has a computed range. When that range is used as
a scratch index, `scratchFlatIndex` proves it in-bounds and emits no
VNE-072 check.

**Impact on §5.7:** the matmul at `--no-scratch-bounds` speed _with checks
on_ becomes the natural outcome. The paper's §5.7 story changes from
"checks cost 45%, here's a flag to disable them" to "checks are free once
the induction variable carries its range." This is the single biggest
positive-framing win.

**Effort:** 2–3 weeks. **Expected gain:** eliminates the entire bounds-check
cost on well-typed code; strictly a correctness-preserving optimization.

### O3.2 — Native array ABI (§6.2) [ DONE ]

**File:** `codegen/function_calls.cpp`, `codegen/functions.cpp`

Today, passing `Array<Float64>` to a Vyne function boxes it: the caller
allocates `args[]` on the arena and copies each argument in. The callee
unboxes at entry.

With the native array ABI, the function's native signature becomes
`void fn(const double* data, int64_t n)`, and the callee compiles against
native memory. Every `vlinalg` op that currently copies its arguments
through `args[]` skips the copy.

**Impact on ML:** this is the payoff. It's what lets Vyne-defined tensor
kernels match hand-written C, and it's the prerequisite for any external
BLAS integration (see the last `todo_2.md` entry).

**Effort:** moderate (2–3 weeks). **Expected gain:** removes per-call
copy for every tensor operation; large in ML code where tensor ops
dominate.

### O3.3 — In-place ops via linear types (F7 from `todo_2.md`)

`fn relu_inplace(x :: unique [N])` — the compiler proves the callee has the
only reference. Mutation happens in place with no alias check, no defensive
copy, no COW.

**Impact on training loops:** SGD updates and forward passes allocate per
step because the current semantics share arrays by reference and the
compiler cannot prove the destination is dead. With uniqueness, every
`W = W - lr * grad` lowers to an in-place update over W's existing storage.

**Effort:** 3–4 weeks. **Expected gain:** eliminates per-step allocations
in training; the biggest single win for ML throughput.

### O3.4 — A real `Tensor` type

**Files:** `codegen/ctype.h`, `codegen/collections.cpp`, everything else

Right now:

- `VyneArray_f64` carries element kind but no shape.
- `CType::Kind::Array` carries element type in `args[0]` but no shape.
- Scratch carries shape in `CType::shape` but only element kind in `args[0]`.
- `VyneArray_f64` and scratch arrays have completely different C layouts.

**Unify them into one `CType::Kind::Tensor { element, shape }`.** Then the
emitter can pick a native loop for _every_ tensor arithmetic without going
through `VyneValue` — no per-element tag check, no boxing, no `vyne_binop`
dispatch.

This is the type-system change `regions.md` §6.6 names as future work, and
it's the biggest lever for ML. It subsumes O3.1's bounds-elision, O3.2's
native ABI, and O3.3's in-place mutation into one design.

**Effort:** months. **Expected gain:** structural; makes Vyne a numeric
language rather than a general one with scratch bolted on.

---

## Tier 4 — Kernel and algorithmic (later, kernel-specific)

Small, targeted improvements that matter for the specific benchmarks.

### O4.1 — Fused matmul + bias + activation

Right now `forward` does three passes and materializes two temporaries.
One fused kernel does one pass, one allocation.

**Where:** `vlinalg` ops; would be a `vlinalg.multiply_add_bias_relu` or a
fused `vlinalg.forward_step`.

**Effort:** 2 weeks (new op, new tests). **Gain:** 2–3× on the fused chain
for small models.

### O4.2 — `VyneArray_f64` as the default tensor storage

`vlinalg.multiply` returns a boxed `VyneValue` array — 16 bytes/element.
Switching the return type to `VyneArray_f64` (8 bytes/element) halves the
memory for every intermediate in the forward pass.

**Effort:** a week, mostly in `vlinalg`'s return-type declarations and
`boxAny` handling. **Gain:** halves the peak RSS of any ML workload; a
strict improvement.

### O4.3 — Welford's online algorithm for loss/variance

Same op count as the current two-pass accumulation, better numerical
stability. Minor.

**Effort:** 1 day. **Gain:** numerical only.

### O4.4 — Loop tiling for large matmuls

At 1024×1024 the working set is 24 MB, well above L2. Tiling by 64×64 would
keep the current tile in L1/L2 and give 2–3× on the raw kernel.

**But:** the paper deliberately measures at the portable SSE2 baseline
without tiling, because the result is about the memory model. Tiling would
change the kernel and obscure the comparison. **Do not do this for Paper 1.**
Note for Paper 2 or 3 if the workload scales up.

---

## Implementation order

Already landed: `O0.1`, `O0.2`, `O1.1`, `O1.2`, `O2.1`, `O3.2`, plus
loop-invariant field-load hoisting in `loops.cpp`.

Remaining, by ROI:

```
1.  O1.5   inline single-use loop bounds              1 h
2.  O1.3   fuse binop temp + assignment               1.5 h
3.  O1.4   inline single-use temps (careful)          3-4 h
4.  O1.6   hoist row pointers (easy form)             10 min
5.  O2.3   reorder branches in vyne_binop_slow        15 min
6.  O2.2   place_all fast path                        30 min
7.  O2.4   unchecked push                             1 h
    --- rebuild, re-measure, expect to close the gap to C ---
```

The last measurement is what goes in the paper. If Vyne's clean-run wall
clock matches hand-written C's clean-run wall clock to within noise, the
section strengthens from "indistinguishable wall clock" (defensible but soft)
to "wall clock identical within measurement resolution" (defensible and
precise).

Items 1-7 are roughly one day of work total. Measure before and after, and
commit with the emitted `.vy.c` diff in the message.

---
## What this does to the paper

**§5.7** gains one sentence after the C comparison paragraph:

> After the emitter peepholes of O1.1–O1.6 (constant folding, single-use
> temp inlining, loop-bound inlining, and row-pointer hoisting), Vyne's
> generated C is within measurement noise of the hand-written C baseline
> on all three axes: same checksum, same peak RSS, and a wall clock inside
> run-to-run variance. The 3.9% gap reported in an earlier draft is
> attributable to register pressure from the emitter's temp-heavy output
> and is closed by the changes described in §6.11.

**§6.11 (new)** names the codegen gaps that Tier 1 closes and points at
Tier 3's structural improvements:

> The emitter's output is _correct_ but not always _tight_. The temp-heavy
> style — one C local per subexpression — is friendly to audit and hostile
> to register allocation. The peepholes in §5.7 are the minimal set that
> closes the wall-clock gap to hand-written C on the scale experiment. The
> larger codegen gap is not temp count but the boxed array model: every
> tensor operation currently passes through `VyneValue`, so the emitted C
> cannot inline the arithmetic. §6.2 (native array ABI) and §6.6 (shaped
> tensors) name the type-system changes that would let a Vyne-defined
> `matmul` reach the same generated code a C implementation produces,
> without the intermediate boxing.

That's the honest framing. Tier 1 closes the present gap; Tier 3 is what
would make Vyne a numeric language in its own right.

---

## What NOT to do

- **Do not chase CUDA, cuBLAS, or GPU.** Requires §6.2 and Papers 2/3
  as prerequisites. Months of work; not on the critical path.
- **Do not add `-ffast-math` or `-march=native` to the default build.**
  The paper's result is at portable SSE2. `--native` stays opt-in.
- **Do not do loop tiling for Paper 1.** Changes the algorithm; obscures
  the memory-model story.
- **Do not shrink `VyneValue` (O2.5).** Every array of `VyneValue` depends
  on the current 16-byte layout. Repacking is a runtime rewrite.
- **Do not optimize `vlinalg` internals before the native array ABI lands.**
  The boxing dominates; micro-optimizing inside the boxed world is wasted
  effort.

---

```

```
