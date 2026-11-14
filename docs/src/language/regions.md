---
id: regions
title: Memory & Regions
group: Language Reference
icon: fa-layer-group
order: 9
description: The arena model, every region policy, commit/move/drop, scratch arrays, and the target support matrix.
---

# Memory & Regions

Vyne's compiled backends allocate from a bump arena. A `region` takes a checkpoint on entry, every allocation in the body bumps the arena, and the closing brace rewinds to that checkpoint. There is no garbage collector, no reference counting, and no per-object free in the normal path.

```vyne
through epoch :: 1..N -> loop {
    region train_step {
        xs :: Array<Float64> := [1.0, 2.0, 3.0];
        # ... work ...
    };
    # xs is gone; the arena tip is back where it was
};
```

Peak memory stays flat regardless of iteration count. The same contract holds on the desktop (8 MB default blocks) and on the Arduino Uno (1 KB static arena).

## The three arenas

| Component | Purpose |
| :--- | :--- |
| **Main arena** | Bump allocator for ordinary values. Blocks are `mmap` / `VirtualAlloc` / `malloc` backed on desktop; a static `uint8_t[]` on AVR. |
| **Commit arena** | Survives ordinary region rewinds. `region.commit(x)` and `region.move(x)` publish values here. Freed only at program exit unless a policy says otherwise. |
| **Checkpoint stack** | Records `(block, offset)` pairs. Nested checkpoints form a stack; rewinding to slot `k` invalidates slots `k..top`. Desktop uses a variable-length stack; AVR uses 16 fixed slots. |

`vmem` exposes the accounting: `vmem.total_allocated()`, `peak_allocated()`, `commit_total_allocated()`, `checkpoint_depth()`, `block_count()`, and more. These are arena counters, not process RSS. On desktop the arena and checkpoint stack are thread-local, so concurrent threads do not share a bump pointer.

## Region policies

A region may carry **at most one memory policy**. `@layout`, `@backward`, `@implicit`, `@recompute`, and `@mixed` are composition attributes and stack on top of that policy. A second memory policy is `VNE-080`.

| Modifier | Category | Behaviour |
| :--- | :--- | :--- |
| *(none)* / `@bump` | Lifetime | Checkpoint on entry, bump-allocate in the body, rewind at the closing brace. |
| `@pool<T, N>` | Allocation | Fixed-capacity slot allocator. `pool_alloc()` returns a zero-filled slot; `pool_free(x)` returns it to the free list. `T` is `Float64` or `Int64`; `N` is elements per slot. |
| `@slab<T, MAX[, CAP]>` | Allocation | Power-of-two size-class allocator. `slab_alloc(n)` rounds up to the next class and recycles a slot; `slab_free(x)` returns it. Each class's backing is lazy. `MAX` is the largest single allocation; `CAP` is live slots per class (default 8). |
| `@ring<T, N, MAX>` | Lifetime + allocation | `N` rotating slots, each `MAX` elements wide. `ring_alloc(n)` returns the next slot and overwrites the oldest after `N` allocations. There is no free intrinsic; the bounded history *is* the contract. |
| `@streamed<T, MAX>` | Location | Draws from a separate page-locked host arena (`mlock` / `VirtualLock`, plain-memory fallback). `streamed_alloc(n)` consumes from a total budget of `MAX` elements. Desktop only. |
| `@persistent` | Lifetime | Routes the whole body to the commit arena and never rewinds it. Values survive the closing brace and any enclosing rewind until program exit. Desktop only. |
| `@device<backend>` | Location | Opens an accelerator frame and routes `device_*` intrinsics to the device arena. Backend is `cuda`, `auto`, or `cpu`. Desktop only. |
| `@speculative` | Lifetime + control | Runs the body, then commits only if `commit_if(pred)` fired; otherwise rewinds. The last decision wins. |
| `@bounded<N>` | Verification | Runtime-checked byte ceiling. Static scratch is counted at compile time (`VNE-101`); arena growth and runtime VLAs are checked at runtime (`VNE-102`). A violation halts with `VNE-102` and names the region. |
| `@zeroed` | Initialization | Every arena allocation in the region is zero-filled and scratch arrays are zero-initialized. The zero-mode counter nests. |
| `@deterministic` | Verification | Implies `@zeroed` and statically rejects time/RNG sources in the region body (`VNE-106`). The scan covers straight-line statements; runtime zeroing covers loop bodies. |
| `@realtime<ms>` | Verification | Records a monotonic timestamp on entry and checks the deadline at normal close and on every early return. Overrun halts with `VNE-109`; telemetry is exposed through `vcore.realtime_*` (desktop) or `arduino.realtime_*` (AVR). |
| `@layout<soa>` | Layout (composition) | Lowers struct `scratch` to one packed array per field instead of an array of structs. `@layout<aos>` is the explicit default; `@layout<struct_of_arrays>` is an alias. |
| `@backward` | AD (composition) | Scopes the reverse sweep and tape to the region body. Calls to `@differentiable` functions are recorded; `backward(seed)` consumes them. |
| `@implicit` | AD (composition) | Implies `@backward` and enables `newton_solve(F, x..., z0)`. `backward(root)` emits the Implicit Function Theorem adjoint instead of unrolling the solver. Desktop only in this release. |
| `@recompute<...>` | AD metadata (deferred) | Parsed and recorded as checkpoint/recompute metadata. Lowering is deferred; the compiler emits a metadata comment. |
| `@mixed<...>` | Precision metadata (deferred) | Parsed and recorded as mixed-precision intent. Lowering is deferred; the compiler emits a metadata comment. |

**Not region policies.** `@differentiable` marks a function, `@invariant` marks a loop contract, and `scratch` is a declaration. `@scratch` is a future design idea and is not accepted by this release.

### Composition rules

- `@layout` composes with any policy.
- `@backward` composes with a memory policy: `@backward @bounded<...> region`.
- `@implicit` implies `@backward` and composes with a memory policy: `@implicit @bounded<...> region`. It is desktop-only in this release.
- `@recompute` and `@mixed` parse and compose with a policy, but only record metadata today.
- `@streamed`, `@device`, and `@persistent` are desktop-only and rejected on AVR.
- `@bounded`, `@zeroed`, `@deterministic`, and `@realtime` lower on the MCU runtime. `@pool`, `@slab`, `@ring`, and `@speculative` are desktop-backed in this release.
- Buffers from `@pool`, `@slab`, `@ring`, and `@streamed` are fixed-size; do not grow them with `push`.

## Policy examples

### `@bounded` and `@deterministic`

```vyne
@bounded<1_048_576>
@deterministic
region step {
    scratch grad :: Float64[64, 16];
    # deterministic + zeroed + bounded
};
```

An early `return` inside `@zeroed` / `@deterministic` is `VNE-107`; an early `return` inside `@bounded` is `VNE-108`. Move the return outside the region, or drop the policy.

### `@pool`

```vyne
@pool<Float64, 4> region slots {
    a :: Array<Float64> := pool_alloc();
    b :: Array<Float64> := pool_alloc();
    pool_free(a);
    c :: Array<Float64> := pool_alloc();   # reuses a's slot
};
```

### `@slab`

```vyne
@slab<Float64, 1024, 4> region buckets {
    small :: Array<Float64> := slab_alloc(16);    # 16-element class
    large :: Array<Float64> := slab_alloc(700);   # 1024-element class
    slab_free(small);
};
```

### `@ring`

```vyne
@ring<Float64, 4, 16> region history {
    through i :: 0..9 -> loop {
        frame :: Array<Float64> := ring_alloc(16);
        # frame is valid for the next 4 ring_alloc calls
    };
};
```

### `@streamed`

```vyne
@streamed<Float64, 1024> region staging {
    host :: Array<Float64> := streamed_alloc(256);
    out("pinned: " + string(vmem.stream_is_pinned()));
};
```

`MAX` is a total element budget, not a per-allocation size. Exceeding it is `VNE-082`. `vmem.stream_total_allocated()`, `stream_peak_allocated()`, `stream_block_count()`, and `stream_is_pinned()` report the pinned arena.

### `@persistent`

```vyne
@persistent region weights {
    model :: Array<Float64> := load_weights();
};
# model survives the region and any enclosing rewind
```

`region.commit(x)` inside `@persistent` is a no-op: the value is already in the commit arena. Nested `@persistent` regions share storage. Peak memory is the programmer's responsibility because there is no automatic reclamation.

### `@speculative`

```vyne
@speculative region attempt {
    candidate :: Array<Float64> := expensive_search();
    ok :: Bool := candidate.mean() < threshold;
    region.commit_if(ok);
};
```

If no `commit_if` fires, the region rewinds. `commit_if` outside `@speculative` is `VNE-085`.

### `@realtime`

```vyne
@realtime<5> region control_tick {
    controller.step(reading);
};
out("worst ms: " + string(vcore.realtime_worst_ms()));
out("overruns: " + string(vcore.realtime_overruns()));
```

This is a watchdog, not a worst-case-execution-time proof. It measures every exit path including early returns.

### `@layout<soa>`

```vyne
interface Particle {
    x :: Float64, y :: Float64, z :: Float64, mass :: Float64,
}

@layout<soa> @bounded<400_000> region physics {
    scratch particles :: Particle[10_000];
    particles[i].x = 1.0;     # writes v_particles_x directly
};
```

Struct scratch without `@layout<soa>` is `VNE-062`; reading a whole SoA element as a struct is `VNE-063`. Only primitive fields (`Int64`, `Float64`, `Float32`, `Bool`) are supported. Multi-dimensional struct scratch flattens row-major.

## Committing, moving, and dropping

### `region.commit`

```vyne
last :: String := "initial";

through epoch :: 1..4 -> loop {
    region scratch {
        msg :: String := "epoch:" + string(epoch);
        last = msg;
        scratch.commit(last);      # qualified: innermost region is `scratch`
    };
    out(last);
};
```

The unqualified form `region.commit(x)` works and targets the innermost open region. The qualified `<name>.commit(x)` / `<name>.commit_if(pred)` form is recommended because it documents intent and rejects committing across a nesting level (`VNE-086`). Commit is a deep clone into the commit arena; committing in a hot loop grows the commit arena linearly.

### `region.move`

`region.move(x)` is the zero-copy sibling. When `x` is a typed array whose payload occupies a dedicated arena block (larger than the 8 MiB block size), the block is unlinked from the region arena and linked into the commit arena in O(1). Smaller values, boxed values, maps, and strings fall back to a commit-arena copy.

```vyne
persistent_model :: Array<Float64> := [];

@bounded<64_000_000>
region train {
    local_model :: Array<Float64> := model_parameters();
    persistent_model = region.move(local_model);   # expression form
};
```

Moving a value allocated *before* the current region is rejected: it would move bytes the region does not own and skew `@bounded` accounting. Use `region.commit` for values that already outlive the rewind.

### `drop`

`drop(x)` releases a typed-array payload early when it is the most recent allocation in the arena's current block. It is a pure pointer rewind, so the `@bounded` live counter decreases immediately and later allocations can reuse the space. If `x` is not the arena tail, `drop` is a no-op and the normal rewind reclaims it.

```vyne
@bounded<12_000>
region frame {
    big :: Array<Float64> := make(1000);   # ~8 KB
    use(big);
    drop(big);                             # freed now; size and cap become 0
    next :: Array<Float64> := make(1000);  # fits in the same bound
};
```

`drop` supports `Array<Float64>`, `Array<Float32>`, and `Array<Int64>`. Stack-shaped `scratch` arrays are rejected.

## Shaped scratch arrays

```vyne
region kernel {
    scratch grad_w1 :: Float64[64, 16];
    scratch grad_b1 :: Float64[16];
    # lowered to `double grad_w1[1024];` on the C stack
};
```

`scratch` is a codegen-only allocation on the native C stack. It is not an arena value and it must live inside a region. Multi-dimensional scratch is row-major. Runtime-length scratch (`Float64[n]`) lowers to a VLA on desktop; on AVR, scratch is also stack-allocated, but the stack has only about 1.5 KB of headroom.

## Escape analysis

The compiler rejects assignments that would write a region-local value into a shallower variable:

```vyne
outer :: Array<Float64> := [];

region inner {
    tmp :: Array<Float64> := make_buffer();
    outer = tmp;        # VNE-070
};
```

The diagnostic names the variable, both region depths, the line, and three fixes: declare the variable inside, commit/move the value, or move the assignment before the region. The check is deliberately syntactic. It cannot see a pointer stored inside a struct field that later outlives the region, and it does not track device handles (which are integers). Persistent regions are excluded from the check because their values already outlive the rewind.

## Target support

| Policy | Desktop | Arduino Uno |
| :--- | :---: | :---: |
| `@bump`, `@bounded`, `@zeroed`, `@deterministic`, `@realtime` | yes | yes |
| `@pool`, `@slab`, `@ring` | yes | no (desktop-backed) |
| `@speculative` | yes | no (desktop-backed) |
| `@layout<soa>` | yes | scratch only |
| `@streamed`, `@persistent`, `@device` | yes | rejected |
| `@backward` | yes | desktop-oriented AD |
| `@implicit` | yes | rejected |
| `@recompute`, `@mixed` | parsed, metadata only | parsed, metadata only |

## Observability

```vyne
before :: Int64 := vmem.total_allocated();

through i :: 1..1000 -> loop {
    region r {
        xs :: Array<Float64> := [1.0, 2.0, 3.0, 4.0];
    };
};

out(vmem.total_allocated() - before);   # 0 if the region works
```

Useful `vmem` calls: `total_allocated`, `peak_allocated`, `peak_reset`, `commit_total_allocated`, `commit_peak_allocated`, `checkpoint_depth`, `checkpoint_remaining`, `block_count`, `current_block_used`, `is_balanced`, `assert_balanced`, `dump`, `stats`.

## What not to rely on

- **Exact byte counts.** Allocations round up to alignment boundaries and block bookkeeping reserves space. Use the counters to verify flatness, not to measure process RSS.
- **Aliasing across a rewind.** Any pointer into region memory is invalid after the region closes. Commit or move values that must survive.
- **Commit-arena reclamation.** Committed storage is freed at program exit. `@persistent` has no automatic reclamation.
- **Early exits in checked regions.** `return` inside `@zeroed` / `@deterministic` / `@bounded` is rejected (`VNE-107` / `VNE-108`). `break` and `continue` are unaffected.
- **Slices of runtime VLAs.** `buf[lo:hi]` on runtime-length scratch is not supported yet.
- **Poison-on-rewind is debug-only.** Enable with `VYNE_POISON_ARENA=1` or `-DVYNE_ARENA_POISON_DEFAULT=1` to fill reclaimed memory with a quiet NaN pattern.

## Borrowed slices and preallocated arrays

`Slice<T>` is a pointer-and-length borrow, not an owning allocation. Creating a
view adds no payload bytes, but the view becomes invalid when its owner's region
rewinds or its payload is dropped/replaced. Create long-lived owners outside
short-lived frame regions, and keep borrowed views within the owner's lifetime.
Escape analysis is not a borrow checker and does not prove pointer stability.

`Array<T>.reserve(n)` reserves capacity and `resize(n, fill)` changes logical
length. Both can grow the payload and consume the active arena budget. Reserve
before taking views; reuse capacity inside repeated operations when practical.
Shrinking logical size does not return capacity to the arena.
See [Values & Types](page:types) and [Built-ins & Collections](page:builtins).

## See also

- [Automatic Differentiation](page:autodiff)
- [CUDA & Devices](page:device)
- [Arduino & AVR](page:arduino)
- [vmem](page:vmem)
