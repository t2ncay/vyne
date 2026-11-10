---
id: vmem
title: VMem
group: Standard Library
icon: fa-memory
order: 4
description: Arena checkpoints, peak and live accounting, commit/stream statistics, and diagnostics.
---

# VMem

`vmem` exposes the arena. Import it with `use native vmem;`. Most code uses `region` syntax instead of raw checkpoints; `vmem` is for instrumentation and for code that needs manual lifetime control.

## Checkpoints

| Function | Result | Notes |
| :--- | :--- | :--- |
| `vmem.checkpoint()` | `Int64` | Records `(block, offset)` and returns a handle. |
| `vmem.rewind(handle)` | `Int64` | Frees blocks created after the checkpoint and resets the bump pointer. |
| `vmem.pop()` | `Int64` | Pop the innermost checkpoint without rewinding. |
| `vmem.pop_n(n)` | `Int64` | Pop `n` checkpoints. |
| `vmem.rewind_freed(handle)` | `Int64` | Rewind and report the bytes freed. |
| `vmem.verify_checkpoint(handle)` | `Bool` | Check that a handle still names a live slot. |

```vyne
before :: Int64 := vmem.checkpoint();

xs :: Array<Float64> := make_scratch();
use(xs);

vmem.rewind(before);    # xs is invalid now
```

Nested checkpoints form a stack. Rewinding to slot `k` invalidates slots `k..top`. The desktop stack is variable-length; AVR has 16 fixed slots.

## Accounting

| Function | Result | Notes |
| :--- | :--- | :--- |
| `vmem.total_allocated()` | `Int64` | Bytes handed out by the main arena. |
| `vmem.peak_allocated()` | `Int64` | High-water mark of the main arena. |
| `vmem.peak_reset()` | `Int64` | Reset the high-water mark. |
| `vmem.commit_total_allocated()` | `Int64` | Bytes in the commit arena. |
| `vmem.commit_peak_allocated()` | `Int64` | Commit-arena high-water mark. |
| `vmem.stream_total_allocated()` | `Int64` | Bytes in the `@streamed` pinned arena. |
| `vmem.stream_peak_allocated()` | `Int64` | Pinned-arena high-water mark. |
| `vmem.stream_is_pinned()` | `Bool` | Whether the platform actually page-locked the streamed arena. |
| `vmem.stream_block_count()` | `Int64` | Backing blocks in the pinned arena. |

## Live state

| Function | Result |
| :--- | :--- |
| `vmem.checkpoint_depth()` | Current checkpoint depth. |
| `vmem.checkpoint_capacity()` | Maximum depth. |
| `vmem.checkpoint_remaining()` | Free slots. |
| `vmem.block_count()` | Number of arena blocks. |
| `vmem.current_block_used()` | Bytes used in the current block. |
| `vmem.current_block_capacity()` | Capacity of the current block. |
| `vmem.is_balanced()` | Whether the checkpoint stack is balanced. |
| `vmem.assert_balanced()` | Panic if the checkpoint stack is unbalanced. |

## Diagnostics

| Function | Result |
| :--- | :--- |
| `vmem.dump()` | Print a human-readable arena summary. |
| `vmem.stats()` | Print counters and block state. |
| `vmem.reset()` | Drop all arena storage. Development use only. |

> **Caution:** `reset()` invalidates every arena-backed value. `total_allocated()` is not process RSS; it is arena bookkeeping.

## Verify a region is flat

```vyne
use native vmem;

before :: Int64 := vmem.total_allocated();

through i :: 1..1000 -> loop {
    region r {
        xs :: Array<Float64> := [1.0, 2.0, 3.0, 4.0];
    };
};

out("drift = " + string(vmem.total_allocated() - before));
```

The result is `0` when the region rewinds correctly.

## Related language operations

These are not `vmem` methods:

- `region.commit(x)` / `<region>.commit(x)` — deep-clone into the commit arena.
- `region.move(x)` — zero-copy move for large typed-array payloads.
- `drop(x)` — early release when the value is the arena tail.
- `vmem_runtime_*` — internal symbols emitted by codegen; do not call them from Vyne.

## AVR

The MCU runtime aliases the same checkpoint/rewind entry points to its single 1 KB arena. `total_allocated`, `arena_used`, and the bounded counter are available; the richer diagnostics are desktop-oriented.

## See also

- [Memory & Regions](page:regions)
- [VCore](page:vcore)
- [Built-ins](page:builtins)
