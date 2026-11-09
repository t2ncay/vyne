# Vyne C runtime — split headers

`vyne_runtime.h` remains the public entry point. It includes the internal headers
in `detail/`; generated C and runtime modules **do not need changing**. Place
this directory at `<installation>/vyne/runtime/`, replacing the old
`vyne_runtime.h` while keeping `modules/` beside it. The supplied `modules/`
contains unchanged copies of the three attached modules (`vcore`, `vmath`,
`vmem`); retain any other modules already present in your installation.

## Layout

| Header | Contents |
|---|---|
| `arena.h` | Standard includes, compiler hints, both arenas and checkpoints |
| `types.h` | Tagged values, arrays, maps, structs, functions, method registry, forward declarations |
| `values.h` | Value constructors and string character pool |
| `maps.h` | Map operations, generic index access, dismiss hook |
| `strings.h` | String methods |
| `arrays.h` | Dynamic arrays, cloning, sorting |
| `typed_arrays.h` | Numeric arrays and boxed conversions |
| `collections.h` | Slicing, generic delete/clear, ranges |
| `structs.h` | Fields and method invocation |
| `printing.h` | Truthiness and printing |
| `conversions.h` | Conversions, type names, sizeof and stringification |
| `equality.h` | Value equality |
| `operators.h` | Binary, membership and unary operators |
| `modules.h` | Module deployment hook |
| `interpolation.h` | Interpolated strings |
| `exceptions.h` | Try/catch frame storage and throw |

Each detail header includes its predecessor and uses `#pragma once`; this
preserves the original declaration/definition order (including forward
references) while allowing a detail header to be included directly. Runtime
state remains `static` per translation unit, just as in the original header.
No function bodies, signatures, or behavior have been intentionally changed.

## Check

```sh
cc -std=c11 vyne/runtime/tests/smoke.c -lm -o smoke && ./smoke
```

The attached project is partial, so a full transpiler build cannot be run
from these files alone. The supplied modules were copied verbatim. In strict
C11, `vcore.h` already requires platform support for `usleep`/`useconds_t`
and `vyne_getpid`; these unrelated existing module issues are not altered by
this split. The runtime itself and all individual detail headers pass C11
syntax checks.

## Typed-array alignment and @slab

Typed-array payloads come from `arena_alloc_aligned(..., VYNE_TYPED_ARRAY_ALIGN)`.
The default alignment is 32 bytes; override with
`-DVYNE_TYPED_ARRAY_ALIGN=64` for wider SIMD kernels. Small values and boxed
structs keep the 8-byte bump alignment.

`modules/vyne_slab_runtime.h` backs the `@slab<T, MAX[, CAP]>` region policy: a
power-of-two size-class allocator with O(1) alloc/free, lazy per-class backing,
and zero-fill on allocation. The region rewind reclaims every class's backing.

`modules/vyne_ring_runtime.h` backs `@ring<T, N, MAX>`: N fixed-size slots that
rotate in order and overwrite the oldest after N allocations. It never grows and
has no free intrinsic; the region rewind reclaims the backing.

`modules/vyne_stream_runtime.h` backs `@streamed<T, MAX>`: a separate bump arena
backed by page-locked host memory (`VirtualAlloc` + `VirtualLock` on Windows,
`mmap` + `mlock` on Linux/macOS, plain `malloc` elsewhere). `streamed_alloc(n)`
draws from it under a total byte budget, and the region leave reclaims the pinned
blocks. `vmem.stream_*` reports live/peak bytes, block count, and whether the
provider actually pinned the pages.

`@persistent` uses the commit arena in `vmem.h`: `vyne_persistent_enter` /
`vyne_persistent_leave` save the enclosing arena, route allocations to
`g_commit_arena`, and publish the advanced state on the outermost leave. Values
live until `arena_free_all()` at program exit; nested persistent frames share the
commit arena.

## Thread-local arenas and the shared commit arena

`g_arena`, its bump pointers, the `@zeroed` counter, the checkpoint stack, and
the `@persistent` frame stack are `VYNE_THREAD_LOCAL`. A thread that allocates
gets its own block chain and never contends with another thread's region
allocations. The commit arena (`g_commit_arena` / `g_commit_cur` / `g_commit_end`)
is intentionally process-global: it is the hand-off point between threads. Every
access to it — `region.commit`, `region.move`'s copy fallback, stats and peak
queries — takes a recursive spinlock (`vyne_commit_lock_acquire/release`).
`@persistent` holds that lock for the body of the outermost region, because it
uses the shared commit arena as its bump arena.

Worker threads must call `vmem_runtime_thread_cleanup()` (or
`arena_free_thread()`) before exiting; it frees only the calling thread's region
arena. `arena_free_all()` / `vmem_runtime_reset()` are process-teardown paths and
also free the shared commit arena.

`@parallel` lowering is a separate future feature; this slice makes the arena
layer safe for it and for user-managed OS threads.

Tests: `tests/alignment.c`, `tests/slab.c`, `tests/ring.c`, `tests/streamed.c`,
`tests/persistent.c`, and `tests/tls_arena.c`.

```sh
cc -std=c11 vyne/runtime/tests/alignment.c -lm -o alignment && ./alignment
cc -std=c11 vyne/runtime/tests/slab.c -lm -o slab && ./slab
cc -std=c11 vyne/runtime/tests/ring.c -lm -o ring && ./ring
cc -std=c11 vyne/runtime/tests/streamed.c -lm -o streamed && ./streamed
cc -std=c11 vyne/runtime/tests/persistent.c -lm -o persistent && ./persistent
cc -std=c11 -pthread vyne/runtime/tests/tls_arena.c -lm -o tls_arena && ./tls_arena
```

## Debug: poison-on-rewind

`arena_rewind` and `arena_try_reclaim` can overwrite the bytes they release
with a quiet-NaN word (`0x7FF8DEADBEEFCAFE`) plus an `0xA5` byte remainder, so a
stale read into a rewound window shows up as `NaN` in a `Float64` result (or as
a legible sentinel in a hex dump) instead of a plausible leftover value.

The mode is off by default and compiles to one predictable branch when
disabled. Enable it per process with `VYNE_POISON_ARENA=1`, from C with
`vyne_arena_set_poison(1)`, or unconditionally at compile time with
`-DVYNE_ARENA_POISON_DEFAULT=1`. It is a desktop-runtime feature; the MCU arena
in `arduino/detail/mcu_arena.h` is intentionally unchanged.

The runtime test is `vyne/runtime/tests/poison.c`:

```sh
cc -std=c11 vyne/runtime/tests/poison.c -o poison && ./poison
```
