# Memory and regions

**Since:** v0.0.1-alpha (documentation target; release tag to confirm)

## Purpose
A region bounds the lifetime of arena-allocated values created while the region is active.

## Syntax
```text
region-statement ::= "region" identifier "{" {statement} "}" ";"
commit-statement ::= "region" "." "commit" "(" expression ")" ";"
move-statement   ::= "region" "." "move"   "(" expression ")" ";"
drop-statement   ::= "drop" "(" identifier ")" ";"
```

## Semantics
The compiled backend saves an arena checkpoint when it enters a region and rewinds when it leaves. A committed value is copied into a second arena that survives the rewind. Commit storage currently persists until global arena cleanup, so commits in an unbounded loop can grow memory. `vmem.total_allocated()` reports arena accounting; do not interpret it as exact process memory usage.

`region.move(x)` is the zero-copy sibling of `region.commit(x)`. When `x` is a typed array whose payload occupies a dedicated arena block (any allocation larger than the 8 MiB block size), the block is unlinked from the region arena and linked into the commit arena in O(1); no payload bytes are copied and peak RSS does not double. Smaller values, boxed values, maps and strings fall back to a commit-arena copy. The source variable is rebound to the moved value and becomes committed, so it remains safe to read inside the region. `move` is also an expression:

```vyne
persistent := region.move(model);   # declaration before the region
```

Moving a value that was allocated *before* the current region is rejected: it would move bytes the region does not own and skew `@bounded` accounting. Use `region.commit` for values that already outlive the rewind.

`drop(x)` releases a typed-array payload early when it is still the most recent allocation in the arena's current block. Because the arena is a bump allocator this is a pure pointer rewind — the `@bounded` live counter decreases immediately and the freed space is available to later allocations in the same region. If `x` is not the arena tail, `drop` is a no-op; the memory is reclaimed normally by the region rewind. A successful drop empties the container (`size` and `cap` become 0), so a stale read cannot observe reclaimed bytes. `drop` supports `Array<Float64>`, `Array<Float32>` and `Array<Int64>`; stack-shaped `scratch` arrays are rejected.

## Example
```vyne
region temporary {
    out("inside region");
};
```

> **Caution:** A value referring to memory allocated inside a region may dangle after rewind. Do not assume an alias escapes safely; commit the value when it must survive.

## Policies
A region may carry one policy attribute: `@name<args> region name { ... };`.

| Policy | Meaning |
| --- | --- |
| *(none)* / `@bump` | Default checkpoint/rewind bump region. |
| `@pool<T, N>` | Fixed-slot pool: `pool_alloc()` / `pool_free(x)`. `T` is `Float64` or `Int64`. |
| `@slab<T, MAX[, CAP]>` | Power-of-two size-class allocator: `slab_alloc(n)` / `slab_free(x)`. Each class's backing is allocated lazily and reclaimed by the region rewind. `CAP` defaults to 8. |
| `@ring<T, N, MAX>` | Rotating fixed slots: `ring_alloc(n)` returns the next of `N` slots (each `MAX` elements wide), overwriting the oldest after `N` allocations. There is no free intrinsic. |
| `@streamed<T, MAX>` | Pinned host memory: `streamed_alloc(n)` draws from a page-locked arena with a total budget of `MAX` elements (`MAX * sizeof(T)` bytes). Desktop only; the AVR target rejects it. |
| `@persistent` | Commit-arena region: allocations survive the closing brace and any enclosing rewind for the whole program. No automatic reclamation; nested `@persistent` regions share the commit arena. Desktop only for now. |
| `@speculative` | Runs the body, then commits or rewinds based on `region.commit_if(pred)`. |
| `@bounded<N>` | Runtime-checked byte ceiling for the region. |
| `@zeroed` | Every allocation in the region is zero-filled. |
| `@deterministic` | `@zeroed` plus a non-determinism scan (rejects time/RNG sources). |
| `@realtime<ms>` | Runtime-checked deadline: records a monotonic timestamp on entry and checks the elapsed time on every exit path. Overrun halts with `VNE-109`. |

Policies are compiled-backend features. `slab_alloc(n)`, `ring_alloc(n)`, and
`streamed_alloc(n)` zero-fill the requested `n` elements; their buffers are
fixed-size and must not be grown with `push`. A `@ring` value must not be
retained longer than `N` subsequent `ring_alloc` calls — the bounded-history
contract the policy exists to express.

`@streamed` draws from a separate pinned arena; `MAX` is a total budget across
all allocations in the region, and exceeding it is a `VNE-082` runtime error.
The platform reports whether the memory was actually page-locked via
`vmem.stream_is_pinned()`; without page-locking support the arena falls back to
ordinary memory. The region also keeps the normal checkpoint/rewind lifetime for
ordinary allocations. `@streamed` is desktop-only and is rejected on AVR.
`vmem.total_allocated()` reports the main arena; use `vmem.stream_total_allocated()`
for the pinned arena.

`@persistent` routes the body's allocations to the commit arena and does not
rewind it. The values are freed only by program-exit arena cleanup, so peak
memory is the programmer's responsibility. Because persistent allocations
already outlive the region, the escape checker does not treat a persistent
region as a deeper region: assigning a persistent value to an outer variable is
allowed, and `region.commit(x)` inside `@persistent` is a no-op. Nested
`@persistent` regions share the commit arena.

`@realtime<ms>` measures wall-clock time from region entry to every exit,
including early `return`s. The check is a runtime watchdog, not a static WCET
proof: it records the worst observed time and total overruns, exposed as
`vcore.realtime_*` on desktop and `arduino.realtime_*` on AVR. A region that
exceeds its deadline halts with `VNE-109`.

The same policy applies for the whole region; a second policy means a second
region.

## Edge cases
The parser requires `;` after the closing region brace. Regions are a compiled-code feature; the interpreter's behavior must be verified before promising support. Returns, breaks and continues use special cleanup paths.

### See also
[vmem](../modules/vmem.md) · [Control flow](control-flow.md) · [Errors](errors.md)
