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
