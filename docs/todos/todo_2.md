# TODO 2 — Post-Paper-1 feature axes

**Last updated:** 2026-11-10

Originally drafted to start after Paper 1. Several axes have since landed, so
this is now a status ledger plus a plan for what remains — not a queued "do not
start" document.

The through-line: regions gave us **lifetime** as a compile-time fact. Every item below is a _different axis_ the compiler can see. Each is orthogonal to regions, each composes with regions, and each is a decision that a numeric language can move from runtime to compile time but that mainstream languages cannot.

**The design tenet for everything below: regions are the interface to the allocator, and there are two allocators — host and device. The region says which one. Nothing else changes.**

If a feature does not fit that tenet, it is not part of the paper.

---

## Status snapshot (2026-11-10)

Checked against the current tree (`vyne/compiler`, `vyne/runtime`,
`vyne/modules/external`, `examples/`, `README.md`).

| Item | State | What is actually in the tree |
| --- | --- | --- |
| F0 native array ABI | **Done** | Primitive + typed-array native variants; struct ABI in v0.4.0. |
| F1 OpenBLAS dispatch | **Done** | `--blas`, `cblas_dgemm`, runtime `blas_bridge.h`. |
| F2 device arena | **Done, redesigned** | `runtime/modules/vdevice.h`; dynamically loaded CUDA driver with CPU fallback. |
| F3 device regions | **Done, redesigned** | `@device<cuda\|auto\|cpu> region`; `device_alloc` / `upload` / `download` / `sync` intrinsics. |
| F4 explicit transfers | **Partial** | Explicit `device_upload` / `device_download` intrinsics, not typed `to_device` / `from_device` and not residency-aware assignment checking. |
| F5 cuBLAS dispatch | **Not done** | GPU ops use the in-tree PTX kernels; no `cublasDgemm` path. |
| F6 refinement / range types | **Not done** | Bounds checks are still runtime VNE-072 unless disabled. |
| F7 uniqueness / linear types | **Not done** | Boxed/copy semantics unchanged; `vlin` has explicit in-place helpers. |
| F8 shape types | **Not done** | Shapes remain runtime values; scratch carries shapes internally. |
| F9 effect typing | **Not done** | No `!alloc` lattice. |
| F10 units of measure | **Not done** | `vconvert` is a scalar conversion helper only. |
| F11 layout types | **Partial** | `@layout<soa>` applies to struct `scratch`; layout is not a general type property. |
| F12 mutability as a type | **Not done** | No `const` / read-only parameter type. |
| F13 precision types | **Partial** | `Float32` shipped with the CUDA backend (`0.0f`, `Array<Float32>`); `BFloat16` / `Float16` absent outside the deferred `@mixed` metadata. |
| F14 explicit value semantics | **Not done** | No copy-vs-share type property. |
| F15 bitwise operators | **Done** | Parsed and lowered with the intended precedence. |
| F16 scientific notation | **Done** | `1.5e-3`, `_` digit separators. |
| Region policies (`@pool`, `@ring`, `@scratch`, `@persistent`, `@speculative`, `@device`, `@streamed`) | **Mostly done** | `@pool<T,N>`, `@ring<T,N,MAX>`, `@persistent`, `@speculative`, `@device`, `@streamed` ship. `@scratch` is still a per-declaration keyword, not a region-wide policy. Extra policies landed: `@slab`, `@bounded`, `@zeroed`, `@deterministic`, `@realtime`, `@layout<soa>`. |
| AD (new axis, not in the original list) | **Partial** | `@differentiable`, `@backward`, `grad`; straight-line `Float64` and `Array<Float64>` reduction VJPs. Loops, branches, matmul VJPs, array locals, `@recompute`, `@mixed`, and implicit differentiation remain open. |

**Related landings not captured by the original axes:** early `drop()` reclamation
and zero-copy `region.move()`, thread-local arenas, executable contracts
(`requires` / `ensures` / `@invariant`), and `@realtime<ms>` deadline telemetry.

---

## The device arena decision

**Chosen: device values live in a device arena, not in raw `cudaMalloc`-ed buffers.**

Why it's the right choice:

- The same `arena_checkpoint` / `arena_rewind` pair works. A device region is a checkpoint into the device arena, exactly like a host region is a checkpoint into the host arena.
- Region-scoped lifetime semantics carry over unchanged. A buffer allocated in a `device region` is freed at the region's closing brace, on device, without any new discipline in the source.
- The paper's central claim — _lifetime is a lexical fact_ — extends to _location is also a lexical fact_ with the same mechanism.
- `@device` becomes a modifier on the region construct, not a new construct.

What this costs:

- A parallel arena in `detail/arena.h` (~200 lines), backed by `cudaMalloc` / `cudaFree`, exposing `device_arena_checkpoint` / `device_arena_rewind` / `device_arena_alloc`.
- A residency bit on `CType` (one bool).
- A residency stack in `C_Emitter` parallel to `regionStack` (~50 lines).
- Dispatch three ways in vlinalg lowering: host loop, `cblas_dgemm`, or `cublasDgemm` depending on operand residency.

What this forecloses:

- Nothing. If a future feature needs raw `cudaMalloc`, it can bypass the arena — it just won't get region semantics, and the paper will say so.

Write this decision down before typing the first line of device code. The emitter change depends on it, and the whole paper narrative depends on it.

---

## The residency rules

Three rules, written down before implementation, because every semantic question about the device model reduces to one of these:

**Rule 1 — Residency is a property of the value, not of the region.** Once a value is device-resident, it stays device-resident until it is explicitly transferred. It does not silently become host-resident when a `device region` ends. This is what makes residency tractable as a type-level fact.

**Rule 2 — Operations require matching residency.** `vlinalg.multiply(a, b)` requires `a` and `b` to be on the same device. A mismatch is a compile error, not an implicit transfer. The user writes `to_device(b)` or `from_device(a)` at the region boundary.

**Rule 3 — Transfers are region-scoped.** `to_device` and `from_device` are legal only at region boundaries (or at the top level). Inside a `device region`, `from_device` is a compile error. Inside a host region, `to_device` is a compile error. This is what keeps the emitter from having to track residency changes mid-region.

**What these rules forbid, deliberately: arbitrary mid-region switching.** A device tensor and a host tensor cannot appear in the same expression. If the compiler allowed this, it would need to insert implicit transfers at every op, which is what PyTorch's dispatcher does and what makes it complex. Region-boundary switching gives you the same expressive power — a training loop still switches every iteration — while keeping the semantics static.

---

## The axes

| Axis           | Feature                        | What it eliminates                                   |
| -------------- | ------------------------------ | ---------------------------------------------------- |
| Lifetime       | Regions (Paper 1)              | Per-iteration allocation                             |
| **Location**   | **Device regions + arenas**    | **Manual cudaMalloc/free, host/device mixing**       |
| **Identity**   | Uniqueness / linear types      | Runtime alias checks, defensive copies               |
| **Shape**      | Shape types `Float64[M,N]`     | Runtime shape checks, dim mismatches                 |
| **Range**      | Refinement types `Int<0..N>`   | **All bounds checks** — VNE-072 becomes a type error |
| **Unit**       | Dimensional types `Float<m/s>` | Unit-conversion bugs                                 |
| **Layout**     | SoA/AoS as a type              | Cache misses, manual restructuring                   |
| **Effect**     | Purity / allocation typing     | "Does this allocate?" becomes a signature            |
| **Precision**  | f32/f64/bf16 as a type         | Manual precision tuning                              |
| **Mutability** | Read-only as a type            | Aliasing bugs, unintended mutation                   |

Pick any two; they compose. That's the design space.

The axes above are orthogonal properties the compiler can see. **Region policies** — the section that follows — are a different kind of thing: they parameterize the region construct itself along the **Lifetime** axis. Paper 1 ships exactly one policy: bump-allocate, rewind on close. The policies section names the other six, explains what each one buys, and shows how they compose with the axes below.

---

## Region policies — the parameterized region construct

`region` in Paper 1 is one thing: a lexical scope over the bump allocator, checkpointed on entry, rewound on exit. That is a _policy_, not the construct. The design space below generalizes the construct by making its allocator behavior a parameter. Every policy shares the same lexical discipline — same `region name { ... }` surface, same lifetime boundary at the closing brace, same interaction with the escape checker. What changes is what happens to the memory inside.

**The design tenet, restated in light of this section: regions are the interface to the allocator. Policies say which allocator, and how it behaves. Paper 1 established the interface. The policies are the strategies behind it.**

This does not contradict the two-arena model. "Host or device" is still what the _arena_ says. Policies are a second-level choice on top of that: the same bump allocator can be run as a pool, as a ring, as the C stack, or as a commit-by-default arena. The policies are orthogonal to the arena.

### Syntax

```vyne
@policy region name { ... }
```

The `@` form reads as metadata on a construct, which is what it is. An angle-bracket form (`region<policy> name { ... }`) is a future alternative if `@` collides with something else in the grammar.

Policies compose when the memory space and the strategy are compatible:

```vyne
@device @pool<Float64[256]> region kv_cache { ... }
@persistent @device region weights { ... }
@scratch @speculative region autotune { ... }
```

Incompatible pairs are compile errors (VNE-110 or the next free number).

### Summary

| Policy         | Strategy                 | Lifetime                      | Where it lives | State in tree |
| -------------- | ------------------------ | ----------------------------- | -------------- | ------------- |
| `@pool<T, N>`  | Fixed-slot free list     | Explicit free, or scope close | Any arena      | **Shipped** (`pool_alloc` / `pool_free`) |
| `@slab<T, MAX[, CAP]>` | Power-of-two size classes | Explicit free, or scope close | Any arena | **Shipped** (added after this draft) |
| `@ring<T, N, MAX>` | N-slot rotation      | Bounded by N steps            | Any arena      | **Shipped** (`ring_alloc`, no free) |
| `@scratch`     | C stack                  | Scope close                   | Host only      | **Not a region policy**; `scratch` remains a declaration keyword |
| `@persistent`  | Commit arena by default  | Whole program                 | Any arena      | **Shipped** |
| `@speculative` | Fork / rollback / commit | Predicate-dependent           | Any arena      | **Shipped** (`region.commit_if`) |
| `@device<backend>` | Device arena         | Scope close, deferred rewind  | Device only    | **Shipped** (`cuda` / `auto` / `cpu`; CPU fallback) |
| `@streamed<T, MAX>` | Pinned host memory  | Scope close                   | Both           | **Shipped** (desktop only) |
| `@bounded<N>`  | Runtime byte ceiling     | Scope close                   | Any arena      | **Shipped** (added after this draft) |
| `@zeroed`      | Zero-fill allocations    | Scope close                   | Any arena      | **Shipped** (added after this draft) |
| `@deterministic` | Zeroed + nondeterminism scan | Scope close              | Any arena      | **Shipped** (added after this draft) |
| `@realtime<ms>` | Deadline watchdog       | Scope close + early returns   | Any arena      | **Shipped** (added after this draft) |
| `@layout<soa>` | Field-split scratch layout | Composition attribute       | Host           | **Shipped** (added after this draft) |

Six of the original seven policies landed (`@pool`, `@ring`, `@persistent`, `@speculative`, `@streamed`, plus `@device`), and `@slab` was added along the way, with the actual argument syntax `@pool<T, N>`, `@ring<T, N, MAX>`, `@streamed<T, MAX>`, and `@slab<T, MAX[, CAP]>`. `@scratch` as a _region-wide_ policy is the one item here that did not ship; the per-declaration `scratch` keyword remains. Verification and layout attributes (`@bounded`, `@zeroed`, `@deterministic`, `@realtime`, `@layout<soa>`) also landed and compose with the allocation policies. `@recompute` and `@mixed` are parsed as metadata but not lowered.

### `@pool<size>`

**Status: shipped** as `@pool<T, N>` with `pool_alloc()` / `pool_free(x)`.

```vyne
@pool<Float64[64]> region attention_step {
    k = alloc_key();      # from a free list of 64-element buffers
    v = alloc_value();    # same slot pool
    # ... use; slots return on scope close
};
```

**Mechanism.** Allocations of the tagged size come from a per-region free list. Frees happen explicitly or at scope close. Reuse is O(1); every allocation is the same shape.

**Use case.** Transformer KV-cache. Fixed-shape RNN state. Anywhere the workload has one canonical tensor size that repeats.

**Why it's interesting.** This is the first policy where rewind isn't the only free path. Per-object free is O(1), not O(blocks). The allocator _knows the shape_, so it packs slots tightly and drops the per-allocation header. Peak memory is `N_live × slot_size`, provable, with no runtime check. This is the policy that turns bounded memory from a measured property into a provable one.

**Cost.** Memory waste if the pool size is wrong. Requires the allocation site to be statically known to match the pool's shape; dynamic shapes disqualify the region.

### `@ring<N>`

**Status: shipped** as `@ring<T, N, MAX>` with `ring_alloc(n)`; there is no free intrinsic, by design.

```vyne
@ring<4> region sliding_window {
    # Every allocation overwrites the oldest of the 4 slots.
    # No rewind, no free. The discipline is "no value lives
    # longer than N steps."
};
```

**Mechanism.** Allocations walk through N preallocated slots in order, overwriting the oldest when full. The region never grows; the scope close is a no-op.

**Use case.** Sliding-window attention. Streaming inference with bounded history. Producer-consumer pipelines where the consumer lags the producer by at most N steps.

**Why it's interesting.** A bounded-lifetime discipline that doesn't use rewind. The region _proves to the compiler_ that no value outlives N iterations — exactly the invariant a streaming workload needs. Ring buffers are everywhere in systems code, but I've not seen one as a language-level scoping construct. It's the natural companion to `@pool`: pool says "many, same shape"; ring says "few, cyclic."

**Cost.** N must be compile-time. Any allocation whose lifetime exceeds N is a bug the compiler can't diagnose without lifetime inference. Initial version: document the invariant, trust the programmer, add the check when there's time.

### `@scratch`

**Status: not shipped as a region policy.** `scratch` remains a per-declaration
keyword. The region-wide check described below is still an open design.

```vyne
@scratch region forward_pass {
    # Every allocation inside must have compile-time-known shape.
    # They become C stack arrays, not arena blocks.
};
```

**Mechanism.** Paper 1's `scratch` keyword turns individual declarations into C stack arrays. `@scratch` extends this to the region: _every_ allocation inside must be scratch-allocable, or the region is a compile error.

**Use case.** Fixed-batch networks. Fixed-window convolutions. Anywhere the shapes are constants.

**Why it's interesting.** It turns "I think this region is stack-only" into a compile-time guarantee. Today the programmer writes `scratch` per buffer, and any non-scratch allocation silently falls through to the arena. A region-wide policy means the compiler checks _every_ allocation and rejects the region if any of them can't be scratch.

**Cost.** Dynamic shapes disqualify the region. Deep recursion blows the stack. The stack-size budget has to be checked statically — a small analysis on top of the existing scratch code, but a real one.

### `@persistent`

**Status: shipped.** Commit-arena lifetime for the whole region; desktop only for now.

```vyne
@persistent region weights {
    W1 = xavier_init(64, 16);
    W2 = xavier_init(16, 12);
    # Both survive every later rewind, without an explicit commit per value.
};
```

**Mechanism.** Paper 1 has `region.commit(x)` for individual values. `@persistent` makes the commit arena the default for the whole region.

**Use case.** Weight matrices. Optimizer state (Adam's m and v). Anything that lives for the whole program.

**Why it's interesting.** Removes a class of VNE-070 false positives — a `@persistent` region has no rewind to escape from, so the checker has nothing to reject. Also removes the "which of these do I commit?" question; the region answers it.

**Cost.** No automatic reclamation. The programmer takes responsibility for peak memory. Correct for a region named `weights`; a bug for one named `per_epoch_cache`.

### `@speculative`

**Status: shipped** with `region.commit_if(pred)`.

```vyne
@speculative region autotune {
    cfg = pick_config();
    result = run_config(cfg);
    region.commit_if(result.valid);   # else rewind everything
};
```

**Mechanism.** Takes a checkpoint, runs the body, and either commits (all allocations survive) or rolls back (all allocations are freed) based on an explicit predicate.

**Use case.** Autotuning — try config A, roll back if invalid, try B. Speculative decoding — draft tokens, verify, roll back on disagreement. Transactional model updates. Anywhere "try, check, maybe undo" is the shape.

**Why it's interesting.** The natural dual of the escape checker. Instead of only _preventing_ escapes, the language gains a first-class way to _allow_ them at well-defined points. Regions become the unit of speculation, which is what inference serving systems build by hand. Compose with `@scratch` and you get zero-arena-cost autotuning.

**Cost.** The predicate must be expressible in the source language. Rollback must be safe against aliases that escaped — the escape checker already handles this, so it composes cleanly. Interaction with `@device` requires the deferred-rewind path to support conditional commit; not for v1.

### `@device`

**Status: shipped, with a different surface.** The implemented form is
`@device<cuda|auto|cpu> region` plus explicit `device_alloc`, `device_upload`,
`device_download`, `device_sync`, and per-op intrinsics (`device_add`,
`device_matmul`, ...). There is no residency bit on `CType` and no
`to_device` / `from_device` pair; the device arena is dynamically backed by the
CUDA driver with a CPU fallback.

```vyne
@device region gpu_train {
    A = vlin.multiply(x_gpu, w_gpu);
    # allocations go to the device arena; host can't dereference them
};
```

**Mechanism.** The region opens a device frame. Device memory is obtained with
`device_alloc`, filled with `device_upload`, read back with `device_download`,
and freed by the frame close before the host arena rewinds. Host code cannot
dereference device handles directly. (The original `region.commit` +
`from_device` / `to_device` design was superseded by these explicit intrinsics.)

**Use case.** GPU training and inference. Shipped; see `examples/device/` and
`examples/test/device_test.vy`.

**Why it's interesting.** This is the whole point of the two-arena design. Same source, same region semantics, different backend.

**Cost.** Asynchronous rewind (deferred until the last event fires). Host/device boundary crossings are explicit and expensive, which is the correct semantic — you want the programmer to see them.

### `@streamed`

**Status: shipped** as `@streamed<T, MAX>` with `streamed_alloc(n)`; desktop only.

```vyne
@streamed region input_pipeline {
    batch = load_next();
    # allocated in pinned memory: host can write, device can read, no explicit copy
};
```

**Mechanism.** Allocations go to host-pinned device memory (`cudaMallocHost` / `cudaHostAlloc`). Both host and device can access directly, without an explicit `cudaMemcpy`.

**Use case.** Overlapping data loading with compute. The classic double-buffered input pipeline where the next batch is being loaded while the current one is being consumed.

**Why it's interesting.** This is the case that breaks the two-arena story and shows the model generalizes. It's a _third_ kind of memory that doesn't fit "CPU or GPU." Making it a policy on the region is the clean way to express it — and it proves that "which allocator" is a parameter, not a fixed count of two.

**Cost.** Pinned memory is slower than device-local on the device and slower than regular RAM on the host. Only worth it for the overlap. The pinned-memory quota is small (a few hundred MB on consumer GPUs), so peak bounds matter more here than anywhere else.

### Composition rules

**What shipped instead of the original pair matrix.** A region takes **at most
one** allocation, lifetime, or verification policy. A second one is a compile
error; two policies mean two regions. `@layout<soa>` is the one attribute that
composes with any policy, because it changes layout rather than choosing an
allocator. `@backward`, `@recompute`, and `@mixed` are metadata/composition
attributes, and `@recompute` / `@mixed` lowering is deferred.

Consequences for the pairs below:

- `@device @pool` and `@persistent @device` are **not** currently expressible as
  one region; use nested regions (outer location, inner allocator) if the need
  appears.
- `@ring @streamed`, `@ring @persistent`, `@pool @scratch`, and
  `@scratch @device` are rejected today by the single-policy rule.
- `@scratch @speculative` is not expressible because `@scratch` is not a region
  policy.

The original matrix is retained above as design intent, not as implemented
behaviour.

### Composition with the axes

The policies multiply with the axes above:

- **`@pool` + F8 (shape types)** — the pool's slot size becomes a shape variable. `@pool<Float64[M]>` for a type variable `M` in scope. The concrete-size version is the fallback.
- **`@ring` + F9 (effect typing)** — `@ring<4> region X !alloc { ... }` proves the region never allocates _and_ has bounded reuse. The combined contract is "bounded memory, no allocation, cyclic reuse" — the signature every streaming kernel wants.
- **`@speculative` + F7 (uniqueness)** — the rollback path is safe only if no unique value escaped. The escape checker already proves this for regions; extending it to speculation is a small change.
- **`@device` + F4 (transfers)** — device regions and `to_device`/`from_device` are two faces of the same thing.
- **`@streamed` + F4** — the pinned-memory case is exactly the "overlap load and compute" pattern the transfer rules are for.

### Where this belongs in the paper sequence

`@pool`, `@ring`, `@speculative`, `@streamed` carry a paper of their own — **Paper 4: Region policies: parameterizing the memory model.** The claim: seven lifetime disciplines, one construct, all checked at compile time.

`@device` and `@streamed` are evidence for Paper 3 (the GPU story) that the design generalizes past the two-arena count.

`@scratch` and `@persistent` are the endpoints that make the design space complete. Paper 1 already has them as keywords; generalizing them is a small extension.

---

## Ranked features

### Tier 0 — Prerequisites for everything else

---

#### ~~F0. Native array ABI~~ [ DONE ]

**What.** Extend the native-variant dispatcher (already built for primitives in §4.5) to functions whose parameters are `Array<Float64>` / `Array<Int64>` of statically-known element type. The native signature gains one `double*` (or `int64_t*`) per array parameter; the caller passes `.data` from the boxed `VyneArray_f64`.

```vyne
fn scale_add(a :: Array<Float64>, b :: Array<Float64>, n :: Int64) -> Float64 {
    s :: Float64 = 0.0;
    through i :: 0..n-1 -> loop { s = s + a[i] * b[i]; };
    return s;
};
```

emits, alongside the boxed variant:

```c
double fn_scale_add_native(double* a, double* b, int64_t n);
```

**Why it's first.** It is the load-bearing step for every numeric feature that follows. Without it, every array parameter pays a runtime tag check per element, and GCC cannot vectorize the loop body even when the element type is statically known. With it, the emitted C is what a C programmer would write by hand.

**What lands.**

- `CType::Kind::RawArrayPtr` — an unboxed `T*` carrying its element type.
- Native signature emission for array parameters, in the same `emitNativeFunctionBody` dispatcher §4.5 introduced.
- Element-type propagation from parser through `Parameter::arrayElemType` to `CType::args[0]`.
- Index lowering for `RawArrayPtr` in `IndexAccessNode` / `IndexAssignmentNode`.
- Call-site dispatch that recognises a typed-array argument and passes `.data`.

**Effort.** 3–5 days.

**Paper.** This is §6.2 of Paper 1, promoted from "future work" to "implemented." Paper 2's first contribution.

**Depends on.** Nothing.

---

#### F1. OpenBLAS dispatch [ DONE ]

**What.** When `vlinalg.multiply(A, B)` has statically-known `Float64` operands, lower to `cblas_dgemm` instead of the emitted C loop. Same result to the last digit, different code.

**Why second.** It validates the dispatch machinery with a library you can debug, on CPU, before you add device code. If the numbers don't match, it's a row-major/transpose bug, not a device bug. And the dispatch you build here is exactly what cuBLAS needs — the device version is _same code, different function name_.

**What lands.**

- `-lopenblas` added to the driver's link line (one line in `run.sh`).
- A `native_f64_blas` variant on the vlinalg `multiply` entry.
- A dispatch rule in `MethodCallNode::getCExpr`: if operands are `Float64` typed arrays and both shapes are runtime-known, emit `cblas_dgemm`; otherwise fall through to the C loop.
- A driver flag `--blas` so the dispatch can be turned off for comparison.

**Effort.** 1 week.

**Paper.** Section in Paper 2. "The same source, same result, BLAS-backed where it matters."

**Depends on.** F0. The array ABI is what lets the raw pointers reach BLAS.

---

### Tier 1 — The device story

---

#### F2. Device arena [ DONE — redesigned ]

**What.** A second arena, structurally identical to the host arena, backed by `cudaMalloc` / `cudaFree`. Same block-chain, same checkpoint/rewind, same accounting.

**Why.** This is the design tenet. Everything device-related composes through it.

**What lands.**

- `VyneDeviceArena` struct and helpers in `detail/arena.h`: `device_arena_alloc`, `device_arena_checkpoint`, `device_arena_rewind`, `device_arena_free_all`.
- `vmem_runtime_checkpoint_device` / `vmem_runtime_rewind_device` in `runtime/modules/vmem.h`.
- No change to the emitter yet. The device arena exists but nothing allocates into it.

**Effort.** 2–3 days. Mostly copy-paste from the host arena with `cudaMalloc` / `cudaFree` substituted.

**Paper.** Section of Paper 3.

**Depends on.** F0 and F1 to have validated the dispatch pipeline.

---

#### F3. Device regions [ DONE — redesigned ]

**What.** A region modifier that selects the device arena as the allocator for the region's body. This is the concrete implementation of the `@device` policy above.

```vyne
@device region inference {
    scratch is illegal here          // C stack conflicts with device memory
    Array allocations go to device
    vlinalg.multiply dispatches to cuBLAS
};
```

**Why.** This is the syntactic surface. Everything inside a `device region` allocates on device, dispatches to device libraries, and is freed at the region's closing brace by the device arena's rewind. The user writes one modifier to move a computation from host to device.

**What lands.**

- Lexer: `@device` becomes a policy token (or `device` a modifier keyword).
- Parser: `@device region name { ... }` accepted as a variant of `region name { ... }`.
- `C_Emitter`: a residency stack parallel to `regionStack`. `pushRegion` takes an optional residency parameter; the top of the stack is the current context.
- Allocation sites in the emitter check the residency stack and emit calls to `device_arena_alloc` vs. `arena_alloc`.
- `scratch` inside a `@device region` is a compile error (VNE-new): "C-stack scratch is not valid inside a device region."
- `region.commit` on a device value is a compile error for v1: "committing a device value requires an explicit `from_device` first."

**Effort.** 1 week.

**Paper.** §3 of Paper 3. "The same region discipline applies to device memory; the policy says which allocator."

**Depends on.** F2.

---

#### F4. Explicit transfers at region boundaries [ PARTIAL ]

**What.** Two built-ins: `to_device(x)` and `from_device(x)`. Both emit a `cudaMemcpy`. Both are legal only at region boundaries or at the top level.

```vyne
region setup {                     # host
    weights = load_weights();
    batch = load_batch();
};

to_device(weights);                # both become device-resident
to_device(batch);

@device region forward {           # device
    activations = model(weights, batch);
    loss = cross_entropy(activations, batch.targets);
};

from_device(loss);                 # loss crosses back
from_device(activations);

region update {                    # host
    weights = sgd(weights, loss_grad);
};
```

**Why.** This is what the user's supervisor asked for. The switch is dynamic — the training loop crosses back and forth every iteration. But every crossing is at a region boundary, so the compiler knows exactly where transfers happen, and the residency rules (Rule 3) make mid-region crossing a compile error.

**What lands.**

- `to_device` / `from_device` as built-ins. `BuiltInCallNode::getCExpr` handles them.
- A residency bit on `CType`. Set on declaration from the current context; propagated through assignments, calls, and returns.
- Assignment consistency check: a value whose residency bit is `device` cannot be assigned to a variable declared in a host context, unless the RHS is a `from_device` call. Symmetric for `to_device`.
- Operation consistency check: `vlinalg.multiply(a, b)` requires `a.residency == b.residency`. Mismatch is VNE-new.
- Transfer emission: `cudaMemcpy(dst, src, n, cudaMemcpyDeviceToHost)` or the reverse.

**Effort.** 1 week.

**Paper.** §4 of Paper 3. "Residency as a type property, transfers as region-boundary operations."

**Depends on.** F3.

---

#### F5. cuBLAS dispatch [ NOT DONE ]

**What.** Same rule as F1, but when the operands' residency bit is `device`, emit `cublasDgemm` instead of `cblas_dgemm`. The dispatch is three-way now: host loop / `cblas_dgemm` / `cublasDgemm`.

**Why.** This is the payoff. The same source-level `vlinalg.multiply(A, B)` runs on CPU or GPU depending on where its operands live. The user changes one modifier — `@device` on the enclosing region — and the backend changes.

**What lands.**

- `cublasDgemm` bindings in a new `runtime/detail/cublas_bridge.h`.
- The dispatch rule extended in `MethodCallNode::getCExpr`.
- The driver links `-lcublas -lcudart` only when the source contains `@device region` — scanned at parse time, or gated behind a `--cuda` flag.
- A `cudaFree` clean-up path wired into `arena_free_all`.

**Effort.** 1 week.

**Paper.** §5 of Paper 3. "The dispatch pipeline, extended to a second backend."

**Depends on.** F1 (the dispatch path), F3 (device regions).

---

### Tier 2 — The pure axes (Paper 5 and Paper 6 candidates)

---

#### F6. Refinement types for ranges

**What.** Give `Int64` a range annotation: `Int64<0..N>`. The compiler proves each arithmetic result stays in range, and elides the runtime bounds check when it can.

```vyne
through k4 :: 0..N/4-1 -> loop {
    k0 :: Int64<0..N-4> = k4 * 4;
    // A[r, k0 + i] is provably in-bounds. No VNE-072 emitted.
};
```

**Why it's strong.** The §5.7 measurement shows a 45% wall-clock cost from bounds checks in the hot loop (2.25 s checks-on vs. 1.54 s checks-off at ITERS=10, on the same kernel). The reason GCC couldn't hoist those checks was that it couldn't see the range of `k0`. The reason Vyne couldn't either was that `Int64` carried no range. Range refinement is the direct fix, and the paper writes itself: "we measured the cost; range refinement eliminates it statically; here's the proof."

**What lands.**

- `Int64<lo..hi>` as a type, with `lo` and `hi` either literals or expressions in scope.
- Refinement propagation through `+`, `-`, `*`, `/`, `%` with conservative interval arithmetic.
- Loop induction variable types inherit their range from the `through` bounds.
- A bounds-check fallback for values whose range can't be proven — today's VNE-072, unchanged.
- Interaction with scratch: if all indices to a scratch access are proven in-range, `scratchFlatIndex` emits no check.

**Effort.** 2–3 weeks.

**Paper.** Paper 5 (competing with F8 as its core) or Paper 6.

**Depends on.** Nothing.

---

#### F7. Uniqueness / linear types

**What.** A `unique` annotation on a value means the callee holds the only reference. The compiler can then mutate in place, with no runtime alias check and no defensive copy.

```vyne
fn relu_inplace(x :: unique Float64[N]) {
    through i :: 0..N-1 -> loop {
        if x[i] < 0 { x[i] = 0; };
    };
};
```

**Why.** It is the feature that makes vlinalg fast without leaving the language. PyTorch has `out=` parameters but can't enforce they're used correctly. JAX forbids in-place mutation outright. A linear type system enforces it at compile time and gives you the in-place mutation for free.

**What lands.**

- `unique` as a type modifier, orthogonal to shape.
- Linearity checking: a value declared `unique` can appear in exactly one place at a time.
- `unique` + `&` (borrow parameters, §6.1) — a borrowed unique gives in-place mutation through a reference.
- Interaction with device: a `unique` device tensor can be mutated by a device kernel with no host-to-device round trip.
- Interaction with `@speculative`: the rollback path is safe only if no unique value escaped.

**Effort.** 3–4 weeks.

**Paper.** Paper 6 — the biggest of the pure axes.

**Depends on.** F8 (effect typing) for the `!alloc`-checked version.

---

#### F8. Shape types

**What.** Dimensions as part of the type:

```vyne
fn matmul(A :: Float64[M, K], B :: Float64[K, N]) -> Float64[M, N] {
    // M, K, N are type variables, unified at the call site.
};
```

**Why.** Futhark and SaC have this. Nobody has combined it with a region-based memory model or with a lexical-scratch storage class. It is the natural complement to regions: regions say when memory dies, shapes say how much it is.

**What lands.**

- Shape variables at the type level, with unification at call sites.
- Arithmetic on shape expressions (`M * K`, `M + N`) with simplification.
- Shape inference for vlinalg return types.
- Interaction with scratch: `scratch buf :: Float64[M, N]` for type variables `M`, `N` in scope. Turns the runtime shape check of §3.4's third case into a compile-time check.
- Interaction with `@pool`: `@pool<Float64[M]>` for a type variable `M` in scope.
- Interaction with `@device`: bodies inside `@device region` that use shape-typed operands get compile-time-known cuBLAS arguments.

**Effort.** 4–6 weeks.

**Paper.** Paper 5 or 6.

**Depends on.** Nothing strictly, but composes tightly with F6.

---

#### F9. Effect typing for allocation (and purity)

**What.** A `!alloc` effect marker on functions and regions that asserts the body performs no `arena_alloc` call.

```vyne
fn process_block(x :: Float64[512]) -> Float64[512] !alloc {
    // Compiler proves: no allocation in this body.
};
```

**Why.** Not because it's weak — it's the contract form of everything you've been building. The audio-callback rule ("never allocate in the callback") becomes a checkable signature instead of a convention. It's the smallest of the four, but it's the one that turns the memory-model work into a verifiable promise rather than a measured property.

**What lands.**

- A small effect lattice: `!alloc`, `!io`, `!block`, `!throw`, with `pure = !alloc + !io + !block + !throw`.
- Effect inference: a function's effects are the union of its callees'.
- Effect checking at annotation sites.
- Interaction with regions: `@ring<4> region X !alloc { ... }` proves bounded reuse and no allocation together.

**Effort.** 1 week.

**Paper.** Section, not a paper.

**Depends on.** Nothing.

---

### Tier 3 — The long tail

---

#### F10. Units of measure

`Float64<meters> * Float64<seconds> → Float64<meters*seconds>`. F# has this; nobody else does. Niche but the community that wants it wants it badly. **Effort.** 2 weeks. **Paper.** Section.

---

#### F11. Layout types [ PARTIAL — `@layout<soa>` only ]

`Float64[N]` stored `AsAoS` vs. stored `AsSoA`. Compile-time layout choice; same source-level operations. Big cache-performance win. **Effort.** 3 weeks. **Paper.** Maybe. Depends on F8 for the interesting version.

---

#### F12. Mutability as a type

`x :: const Float64[N]` — read-only parameter. C++ has `const`, Rust has `&`. The interesting version is the interaction with F7: `const unique` means "you own it, but you can't mutate it." **Effort.** 1 week. **Paper.** Section.

---

#### F13. Precision types [ PARTIAL — `Float32` shipped, `BFloat16` not ]

`Float32`, `Float64`, `BFloat16` as first-class types, with automatic promotion rules. **Effort.** 2 weeks for the types; 6+ weeks for the interesting version (precision-error tracking). **Paper.** Maybe.

---

#### F14. Explicit value semantics

Make it a type property whether an assignment copies or shares. **Effort.** 2 weeks. **Paper.** Section. Overlaps F7.

#### ~~F15. Add bitwise operators~~ [ DONE ]

From lowest to highest binding (top binds loosest, bottom binds tightest):

```text
||                    logical or
&&                    logical and
|                     bitwise or
^                     bitwise xor
&                     bitwise and
== !=                 equality
< <= > >=             relational
<< >>                 shift
+ -                   additive
* / %                 multiplicative
```

---

#### F16. Add scientific notation for numbers in lexer [ DONE ]

## How they compose

These features multiply. The interesting interactions:

- **F6 + F8** (range + shape) → every scratch access is provably in-bounds. VNE-072 disappears entirely for well-typed code.
- **F7 + F9** (uniqueness + effect) → `!alloc` functions can only mutate what they uniquely own. In-place ops with zero-allocation proof, statically.
- **F7 + F8** (uniqueness + shape) → `matmul(A: unique [M,K], B: [K,N]) -> unique [M,N]`. A correct-by-construction BLAS.
- **F3 + F8** (device regions + shape) → compile-time-known cuBLAS arguments. The device path sees exactly the arguments cuBLAS wants.
- **F3 + F4 + F5** (device regions + transfers + cuBLAS) → a training loop that crosses host and device every iteration, with the compiler inserting transfers at region boundaries and dispatching to cuBLAS inside.
- **F6 + F8 + F9** → a numeric kernel whose signature proves: no allocation, in-bounds access, correct shapes, in-place mutation. That's the whole pitch of the language in one function header.

Region policies stack on top of all of these:

- **`@pool` + F8** → `@pool<Float64[M]>`. The pool's slot size becomes a shape variable.
- **`@ring` + F9** → `@ring<4> !alloc`. Bounded memory, cyclic reuse, no allocation — the streaming-kernel contract.
- **`@speculative` + F7** → speculative execution with unique tensors. The rollback is safe because the escape checker already proved no unique value escaped.
- **`@device` + F4** → `@device` is F3; F4 is the transfers. Same story, two faces.
- **`@streamed` + F4** → pinned-memory regions with explicit transfers at boundaries. The overlap pattern.

---

## Paper sequence

```
Paper 1:  Regions + scratch + compile-time peak bounds
Paper 2:  Array ABI + OpenBLAS + in-place ops (F7 still open)
Paper 3:  Device arena + device regions + residency + cuBLAS
          (arena + @device shipped with explicit intrinsics; cuBLAS still open)
Paper 4:  Region policies (pool, slab, ring, persistent, speculative,
          streamed)
          (all but the @scratch region policy shipped)
Paper 5:  Shape types + range refinement + static bounds
Paper 6:  Uniqueness + linear tensors
Paper 7+: Effects, units, layout, precision - the long tail
```

Each paper is independent of the ones after it. Each is a legitimate contribution on its own.

**The device story (Paper 3) has already started, with a different design.**
F0–F3 landed; transfers are explicit intrinsics rather than typed residency,
and F5 (cuBLAS) is the main remaining piece. The original estimate was six
weeks from array ABI to a working GPU demo; the tree now has a CUDA backend
with a CPU fallback, `@device` regions, FP32/F64 element-wise and matmul
kernels, and tests under `examples/device/` and `examples/test/device_test.vy`.
What remains before that paper is complete: cuBLAS dispatch, a residency-checked
transfer model (F4), and the write-up.

**Region policies (Paper 4) mostly shipped as implementation, not yet as a
paper.** `@pool`, `@slab`, `@ring`, `@persistent`, `@speculative`, `@streamed`,
and the verification/layout attributes are in the tree. `@scratch` as a
region-wide policy is the one design item still open.

---

## What to pick next

F0/F1/F2/F3 are done. The highest-value open items now are:

**Bet 1 - Range refinement (F6) and shape types (F8).** F6 still has the
cleanest demo: it removes the measured bounds-check tax statically instead of
asking users to pass `--no-scratch-bounds`. F8 is the natural companion for
`vlin`/`vdecomp` and for proving scratch shapes.

**Bet 2 - Finish the device story (F4 then F5).** Add a residency-aware
transfer surface and cuBLAS dispatch so `vlin.multiply` can select host loop /
`cblas_dgemm` / `cublasDgemm` from operand location. The arena and region
mechanism already exist.

**Bet 3 - The numerical libraries the axis work needs.** `voptim`, `vsparse`,
`vdecomp`, and `vnum` are still absent (see `todo_libraries.md`). F6/F7/F8 are
easier to validate once those consumers exist.

**Why these three.** They are the remaining axes with (a) real user-visible
demos, (b) existing infrastructure to build on, and (c) composition with the
region model.

---

## What NOT to do

- **Multi-stage programming.** Orthogonal to the pitch. Staging is about generating code at compile time; the pitch is about proving properties. They compose but don't reinforce each other.
- **A general-purpose language.** The pitch is numeric code with compile-time proofs. Don't chase Python or Go.
- **GPU codegen as the main path.** The tree now carries a small hand-written PTX kernel set for the shipped element-wise/matmul ops, but the strategic ask is still interop. Do not grow that into a general Triton-style kernel compiler. Prefer cuBLAS (F5) for dense linear algebra.
- **Arbitrary mid-region switching.** A device tensor and a host tensor in the same expression forces implicit-transfer insertion and makes residency intractable. The shipped design sidesteps this with explicit `device_upload` / `device_download` intrinsics; keep transfers explicit.
- **Raw `cudaMalloc` instead of a device arena.** The shipped `vdevice.h` keeps a tracked device arena and frees by frame; do not regress to untracked allocations.
- **All of Tier 2 and Tier 3 at once.** Pick two. Land them. Then pick two more.
- **Shipping every remaining policy for symmetry.** Most policies already shipped. `@scratch` as a region policy is intentionally still open; it needs the region-wide stack-budget analysis to be useful.
- **Policy inference.** Do not try to infer the policy from the region body. The programmer knows the workload's shape; the compiler doesn't. Policies are declared.
- **Mid-region policy switching.** A region opens with one policy and closes with the same one. Two policies means two regions. This preserves the symmetry between checkpoint and rewind, and keeps the compiler's residency inference tractable.
- **Policies that aren't backed by a real allocator.** A policy should map to an actual allocator in `detail/arena.h`, not to a codegen pattern. `@scratch` maps to the C stack; `@pool` maps to a free list; `@device` maps to `cudaMalloc`. If the policy doesn't have a concrete allocator behind it, it's not a policy, it's a wish.

---

## Sequencing note

The original Paper 1 gate has been overtaken by events: F0-F3 and most region
policies shipped while the paper was still in progress. Read the list below as
history.

What is actually still open, in rough order:

1. F4 (residency-checked transfers) - the explicit intrinsics exist; the typed
   `to_device` / `from_device` model does not.
2. F5 (cuBLAS) - the main missing piece of the device story.
3. F6 (range refinement) - removes the bounds-check tax statically.
4. F8 (shape types) - validates scratch shapes and enables `vdecomp`/tensor
   APIs.
5. F7/F9 (uniqueness/effects) - the remaining axes that change what library
   implementations can prove.
6. `@scratch` as a region-wide policy - only after the stack-budget analysis.
7. Everything else (units, const, precision beyond `Float32`, value semantics)
   - after the above.

`voptim`, `vsparse`, `vdecomp`, and `vnum` remain absent and are tracked in
`todo_libraries.md`; they are prerequisites for validating several axes.

---

```

```
