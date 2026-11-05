# Vyne

> A C-flavored scientific numerical programming language. Region-based
> deterministic memory, dual backend (interpreter + C transpiler), and a
> native AVR backend that runs the same source on an Arduino Uno R3.

[![CI](https://github.com/tuncaygafarli/vyne/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/tuncaygafarli/vyne/actions/workflows/c-cpp.yml)
[![Docs](https://github.com/tuncaygafarli/vyne/actions/workflows/pages/pages-build-deployment/badge.svg)](https://github.com/tuncaygafarli/vyne/actions/workflows/pages/pages-build-deployment)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++26](https://img.shields.io/badge/C%2B%2B-26-00599C.svg)](https://isocpp.org/)
[![Version](https://img.shields.io/badge/version-0.4.0-orange.svg)](CHANGELOG.md)
[![Platforms](https://img.shields.io/badge/platforms-linux%20%7C%20macOS%20%7C%20windows%20%7C%20avr-lightgrey.svg)](#build-from-source)

---

## Overview

Vyne is a statically-analyzable numerical programming language with
**three** execution backends sharing a single AST:

- a **tree-walking interpreter** for fast iteration and REPL use,
- a **C transpiler** that lowers the same AST to one self-contained C
  translation unit against a small arena-based runtime, and
- an **AVR transpiler** that targets AVR-GCC and produces firmware for
  the Arduino Uno R3 (ATmega328P, 2 KB SRAM) from the same `.vy` source.

The language is deliberately C-shaped: braces, semicolons, `fn` for
declarations, `if` / `while` / `through` for control flow, `::` for
type-annotated bindings. It adds the features that make a numerical
scripting layer pleasant to write — first-class functions, pipeline
operator, null coalescing, string interpolation, interface-based structs,
generics via monomorphization — without giving up the ability to run
without a garbage collector.

Every value the compiled backend produces lives in an arena. There is no
GC, no reference counting, no hidden allocator traffic inside numeric
loops. A `region` block scopes that arena lexically: the block takes a
checkpoint on entry, and every allocation made inside is reclaimed at the
closing brace. Programs that use regions have flat peak memory regardless
of iteration count — on the desktop *and* on a 2 KB microcontroller.

```vyne
# blinky.vy — one source, three targets
use native arduino;

arduino.serial_begin(115200);

through i :: 0..2 -> loop {
    arduino.digital_write(13, 1);
    arduino.delay(100);
    arduino.digital_write(13, 0);
    arduino.delay(100);
};

# region scopes the MCU arena (1 KB on Uno).
# `scratch` lowers to a raw C `double buf[16];` on the stack.
region probe {
    scratch buf :: Float64[16];
    through i :: 0..15 -> loop {
        buf[i] = arduino.sin(0.1 * i);
    };
    arduino.serial_print(buf.mean());
};

# The exact same file runs under the interpreter (interactive demo),
# the C transpiler (desktop), and `--arduino uno` (firmware).
```

```bash
vynec blinky.vy                                   # interpreter
vynec --compile blinky.vy                         # native binary
vynec --arduino uno --flash --monitor --port COM3 blinky.vy   # Uno R3
```

---

## Table of contents

- [Why Vyne](#why-vyne)
- [Quick start](#quick-start)
- [Language tour](#language-tour)
- [Memory model](#memory-model)
- [Standard library](#standard-library)
- [Arduino backend](#arduino-backend)
- [Flagship example: TinyML XOR on an Uno R3](#flagship-example-tinyml-xor-on-an-uno-r3)
- [External library system](#external-library-system)
- [Transpiler architecture](#transpiler-architecture)
- [Diagnostics](#diagnostics)
- [Performance](#performance)
- [CLI reference](#cli-reference)
- [Build from source](#build-from-source)
- [Project layout](#project-layout)
- [Editor support](#editor-support)
- [Limitations](#limitations)
- [Contributing](#contributing)
- [License](#license)
- [Acknowledgments](#acknowledgments)

---

## Why Vyne

Most numerical languages force a choice between two extremes: an
interpreter with a garbage collector and no path to a standalone binary,
or a compiled language where a quick edit means a full build cycle. Vyne
occupies the middle ground — and it goes one step further by extending
the same memory discipline from a workstation down to an 8-bit MCU.

**Three backends, one AST.** The interpreter, the C transpiler, and the
AVR transpiler consume the same parsed tree. A program that runs under
`vynec script.vy` behaves identically under `vynec --compile script.vy`.
The third form, `vynec --arduino uno script.vy`, produces AVR firmware
that links against a 1 KB MCU runtime and inherits the same region
semantics as the desktop build — the arena becomes 1 KB of SRAM instead
of 8 MB, but the checkpoint / rewind contract is identical.

**Deterministic memory, no GC.** Values live in a bump arena. `region`
blocks reclaim everything allocated inside them. There are no pauses, no
finalizers, and no inference about liveness in the compiled output. On
a microcontroller, that determinism is what keeps peak SRAM flat across
millions of training iterations; on the desktop, it keeps peak RSS flat
across millions of epochs.

**Native ABI where it matters.** The transpiler specializes functions
whose parameters and return type are all primitives or typed arrays. A
function with signature `(Array<Float64>, Int64) -> Float64` compiles to
the equivalent C function taking `double*` and `int64_t`, with no boxing,
no tag check, and no arena traffic inside the loop. On AVR, where a boxed
`VyneValue` is 16 bytes and a `double` is 4, avoiding the box *is* the
performance win. The kernel/wrapper split that appears throughout the
library ecosystem (`vlin`, `vfft`, `vml`) is a direct consequence of this
design.

**A standard library built for numerical work.** `vmath`, `vcore`,
`vmem`, `vfs` cover the basics. `vlin` (linear algebra), `vml` (neural
network primitives), `vfft` (radix-2 FFT with plan caching), `vrand`
(distributions), and `vbio` (nucleotide / protein sequences) are written
in Vyne itself, in the same kernel/wrapper pattern, and ship with a full
manual each. On AVR, the same numerical idioms run through a 60-function
`arduino` module: serial I/O, ADC, bit manipulation, EEPROM, watchdog,
and an unboxed math family (`_f64` variants) that the emitter picks at
call sites where every argument is a provable `Float64`.

---

## Quick start

```bash
git clone https://github.com/tuncaygafarli/vyne.git
cd vyne
make
./vynec examples/hello.vy
```

Transpile a program to a standalone binary:

```bash
./vynec --compile examples/logic_test.vy
# emits logic_test.vy.c and compiles it against runtime/vyne_runtime.h
# produces ./logic_test
```

Enable BLAS dispatch for matrix operations:

```bash
./vynec --compile examples/benchmark/matmul_1024_vlin.vy --blas
```

Run the flagship TinyML demo on an Arduino Uno R3 (requires `arduino-cli`
and the AVR toolchain — see [Build from source](#build-from-source)):

```bash
./vynec --arduino uno --flash --monitor --port COM3 \
    examples/arduino/tinyml_xor.vy
```

Start the REPL:

```bash
./vynec
```

---

## Language tour

### Bindings

```vyne
x = 42;                     # inferred
y :: Int64 = 42;            # explicit
const PI :: Float64 = 3.141592653589793;
```

Under `ruleset { type_check = "strict" }`, `x = 42` without a type is a
compile error. The default is dynamic binding with a warning.

### Primitive types

| Type       | Example           | Notes                                    |
| ---------- | ----------------- | ---------------------------------------- |
| `Int64`    | `42`, `0x7FFFFFFF`| 64-bit signed; hex literals supported    |
| `Float64`  | `3.14`            | IEEE-754 double                          |
| `String`   | `"hello"`         | UTF-8, immutable, interned               |
| `Bool`     | `true`, `false`   |                                          |
| `Null`     | `null`            |                                          |
| `Array`    | `[1, "two", 3.0]` | heterogeneous                            |
| `Array<T>` | `Array<Float64>`  | typed array, native ABI on the C backend |
| `Map`      | `{"key": 42}`     | string keys                              |

### Control flow

```vyne
if x > 0 {
    out("positive");
} else if x < 0 {
    out("negative");
} else {
    out("zero");
}

while running {
    step();
}

# range loop with a mode
squares = through i :: 1..10 -> collect { i * i };
evens   = through i :: 0..100 -> filter { i % 2 == 0 };
allPos  = through i :: list -> every { i > 0 };
uniq    = through i :: list -> unique { i };
```

The `through ... -> mode` form is the language's canonical iteration
construct. Modes are `loop` (side effects only), `collect`, `filter`,
`every`, and `unique`.

### Functions

```vyne
fn add(a :: Int64, b :: Int64) -> Int64 {
    return a + b;
}

fn max_of<T>(a :: T, b :: T) -> T {
    if a > b { return a; }
    return b;
}
```

Generic parameters (`<T>`) are monomorphized at code generation time;
calls like `max_of<Int64>(3, 5)` and `max_of<Float64>(3.0, 5.0)` produce
two distinct C functions.

### Interfaces

`interface` declares a struct as a set of typed fields plus methods that
close over them. An interface is also the constructor for values of that
type.

```vyne
interface Circle {
    r :: Float64;
    area() -> Float64 {
        return r * r * 3.141592653589793;
    }
}

c = Circle(4.0);
out(c.area());     # 50.26544
```

Interfaces are the mechanism behind every struct-typed value in the
standard library — `vlin.Types.Matrix`, `vfft.Plan`, `vml.Types.Dense`.
Their field layouts participate in the native-ABI dispatch: a struct-typed
parameter expands into one C argument per field, so the caller and callee
agree on a flat, unboxed calling convention. On AVR this is what keeps
a 4-field struct on registers instead of forcing a box round-trip through
the caller's arena.

### Groups

`group` namespaces functions and constants without introducing a type.

```vyne
group Geometry {
    fn area_of_circle(r :: Float64) -> Float64 {
        return r * r * 3.141592653589793;
    }
    const TAU :: Float64 = 6.283185307179586;
}

out(Geometry.area_of_circle(2.0));
```

### Enums

```vyne
enum Color {
    RED = 0,
    GREEN,
    BLUE,
}

out(Color.GREEN);   # 1
```

### Error handling

```vyne
try {
    parse(input);
} catch (err) {
    out("failed: " + string(err));
} finally {
    cleanup();
}
```

Exceptions are `longjmp`-based on the compiled backend
(`VYNE_MAX_EXC_FRAMES` = 64) and stack-unwound in the interpreter.
`throw` accepts any value.

### Operators

| Operator | Meaning                  | Example                  |
| :------- | :----------------------- | :----------------------- |
| `..`     | Range (inclusive)        | `0..10`                  |
| `:`      | Slice (in `[...]`)       | `s[1:3]`                 |
| `?:`     | Ternary                  | `ready ? "go" : "wait"`  |
| `??`     | Null coalesce            | `name ?? "anon"`         |
| `??=`    | Null coalesce assignment | `cfg.mode ??= "default"` |
| `\|>`    | Pipeline                 | `data \|> normalize`     |
| `in`     | Membership               | `x in [1, 2, 3]`         |
| `//`     | Floor division           | `7 // 2`                 |
| `**`     | Power                    | `2 ** 10`                |
| `&`      | Bitwise AND              | `flags & 0xFF`           |
| `\|`     | Bitwise OR               | `mode \| 0x80`           |
| `^`      | Bitwise XOR              | `a ^ b`                  |
| `~`      | Bitwise NOT (unary)      | `~x`                     |
| `<<`     | Shift left               | `1 << 8`                 |
| `>>`     | Shift right              | `x >> 2`                 |
| `^^`     | Reference parameter      | `fn bump(v :: Int64^^)`  |
| `$`      | Address-of               | `$target`                |

Bitwise operators are fully supported on both backends. `^^` binds a
reference parameter (the callee sees the caller's variable); `$` returns
its address as an `Int64` (interpreter only — the compiled backend
rejects address-of).

### Rulesets

`ruleset` declares compile-time and runtime policy. It can appear at any
scope; the effect applies from that point forward.

```vyne
ruleset {
    type_check       = "strict",
    memory_limit     = 256 * 1024 * 1024,
    warnings         = "all",
    warnings_ignore  = ["unused_variable"],
};
```

Available categories: type system (`type_check`, `implicit_casting`),
memory (`memory_limit`), warnings (`warnings`, `warnings_ignore`,
`strict`, `pedantic`, `verbose`), and profiling (`profiling`, `debug`,
`trace`). The full table is in
[`docs/content/rulesets.html`](docs/content/rulesets.html).

### Regions

A `region` block scopes arena lifetime lexically.

```vyne
through epoch :: 1..N -> loop {
    region train_step {
        scratch grad :: Float64[64, 16];
        # every allocation here is reclaimed at the closing brace
    };
};
```

Three region policies exist:

- `region name { ... }` — default bump-checkpoint/rewind behavior.
- `@pool<Float64, 64> region name { ... }` — fixed-capacity slot
  allocator for repeated same-sized buffers.
- `@speculative region name { ... }` — the rewind is conditional on
  `region.commit_if(predicate)`; the region's allocations survive if any
  commit fires.

Region escape analysis (`VNE-070`) rejects assignments that would write a
region-local pointer into a shallower variable, catching the most common
use-after-rewind bug at compile time.

### Shaped scratch arrays

```vyne
region kernel {
    scratch grad_w1 :: Float64[64, 16];
    scratch grad_b1 :: Float64[16];
    # lowered to `double grad_w1[1024];` on the C stack
}
```

`scratch` is codegen-only. The interpreter rejects it with a clear
diagnostic rather than silently emulating the semantics. On the Arduino
target, `scratch` lowers to a raw C array on the **AVR stack**, not the
arena — on a 2 KB SRAM chip, stack allocation for small scratch arrays
is the right tool, and the compiler enforces that it lives inside a
region.

---

## Memory model

Vyne's compiled backends use a bump arena with three components. The same
three components exist on both the desktop and AVR runtime; only the
sizes and the backing store differ.

### Main arena

`arena_alloc(n)` bumps a pointer in the current block. On the desktop,
blocks default to 8 MB and are backed by `mmap` with `MADV_HUGEPAGE` on
Linux, `VirtualAlloc` on Windows, and `malloc` elsewhere. On AVR, the
arena is a `static uint8_t g_mcu_arena[VYNE_MCU_ARENA_SIZE]` in `.bss`
with a default of 1024 bytes — configurable at compile time via
`-DVYNE_MCU_ARENA_SIZE=...`. Allocation is a pointer increment plus a
bounds check on both targets. On AVR, arena exhaustion halts the board
with `while (1) ;`, the standard firmware-halt idiom.

### Commit arena

`region.commit(x)` deep-clones `x` into a parallel arena that survives
region rewinds. The committed value stays valid after the enclosing region
closes. Committed storage is freed at program exit; escape analysis to
determine earlier free points is a planned feature. On AVR, the commit
arena currently lives inside the main arena's 1 KB window; typed-array
commits box and unbox around the commit call to satisfy `vmem_runtime_commit`'s
`VyneValue` signature.

### Checkpoints

`vmem.checkpoint()` records `(block, offset)` and returns an `Int64`
handle. `vmem.rewind(h)` frees every block created after that checkpoint
and resets the bump pointer. Nested checkpoints form a stack; rewinding
to slot `k` invalidates slots `k..top`. The desktop runtime uses a
variable-length checkpoint stack; the MCU runtime uses a fixed
`size_t g_mcu_cp_stack[16]` — sixteen nested regions is the practical
limit on AVR.

The region syntax is sugar over this. `region name { ... }` compiles to
a checkpoint, the body, and a rewind at the closing brace, on both
targets.

### Interning

Every identifier and map key is interned in a process-global string pool.
Symbol lookups compare `uint32_t` IDs instead of strings. The intern
table itself lives outside the arena (malloc-backed on the desktop,
statically-sized on AVR) so a rewind cannot invalidate it. On AVR, string
interning is bounded — `vyne_string_static` for literals does not
allocate; `vyne_string` copies into the arena and can therefore trigger
the halt path if the arena is full.

### Typed arrays

`Array<Float64>` and `Array<Int64>` are represented at runtime by
`VyneArray_f64` and `VyneArray_i64`, structs holding a raw `double*` or
`int64_t*` plus size and capacity. Boxing a typed array into a `VyneValue`
is O(1) — it wraps the pointer, it does not copy elements. Unboxing a
`V_F64_ARRAY` is likewise O(1). This is what makes `A.data[i]` compile to
a direct load. On AVR, this same optimization is what allows the `k_*`
numerical kernels to run in a loop without touching the arena.

---

## Standard library

Native modules are implemented in C++ (desktop) or `static inline` C
(AVR) and registered into the interpreter's symbol tables. The C
transpiler emits calls into a parallel set of functions in
`runtime/modules/` (desktop) or `arduino/detail/mcu_*.h` (AVR). Where a
module exists on both targets, the surface is identical.

### Core (desktop + AVR)

| Module  | Purpose                                                                                                              |
| :------ | :------------------------------------------------------------------------------------------------------------------- |
| `vcore` | System utilities: `now`, `now_ns`, `sleep`, `platform`, `input`, string builder, `memory_usage`, `pid`, process info. On AVR only `now`/`now_ns`/`sleep` are meaningful. |
| `vmath` | Trig, exp/log, rounding, sigmoid/relu, clamp, PCG32 random. On AVR the same surface, with `_f64` unboxed variants emitted where provable. |
| `vfs`   | Filesystem: read, write, lines, bytes, walk, glob, mkdir, path ops. Desktop only — AVR has no filesystem.             |
| `vmem`  | Arena checkpoints, peak tracking, live-state inspection, commit.                                                     |
| `arduino` | **AVR-only.** 60+ bindings covering serial, GPIO, ADC, EEPROM, bit manipulation, watchdog, interrupts, memory introspection. See [Arduino backend](#arduino-backend). |

### Built-in functions

```vyne
out(x)          # print
type(x)         # "Int64" | "Float64" | "String" | "Array" | "Map" | ...
sizeof(x)       # length of a string or array
string(x)       # any value -> String
int64(x)        # any value -> Int64
float64(x)      # any value -> Float64
sequence(lo, hi)# generate [lo, hi)
exit(code)      # terminate
free(x)         # no-op on the compiled backend (arena); releases object on the interpreter
```

On AVR, `out(x)` is not available — the equivalent is `arduino.serial_print(x)`.
The AVR runtime does not link the boxed print engine that `out` relies on.

### Modules retained from earlier releases

The project previously shipped `vglib` (2D/3D graphics on Raylib),
`vaudio` (DSP), `vserv` (HTTP/WebSocket), `vnet` (raw sockets), `vcv`
(computer vision), and `vurage` (embedded database). Those modules are
**no longer maintained** as the language's purpose has shifted to
scientific numerical computing with a first-class embedded target. The
code may still exist in `modules/common/` for archival purposes, but is
not part of the supported surface, is not tested in CI, and is not
advertised here. Users depending on them should pin to a pre-0.4 tag.

---

## Arduino backend

The Arduino Uno R3 target ships a 60+ function runtime under
`arduino/detail/`, split one header per concern. All headers are
**self-contained**: each pulls only from `<Arduino.h>` and `<avr-libc>`,
never from a bundled `arduino/libraries/*` header, so `avr-gcc` compiles
the runtime without the Vyne AVR build integration having to add library
`src/` directories to its include path.

| Header                  | Count | Contents                                                                                                    |
| :---------------------- | :---- | :---------------------------------------------------------------------------------------------------------- |
| `detail/mcu_serial.h`   | 7     | `available`, `read`, `read_line`, `parse_int`, `parse_float`, `println`, `flush`                            |
| `detail/mcu_timing.h`   | 2     | `pulse_width`, `timer_read`                                                                                 |
| `detail/mcu_math.h`     | 34    | 18 boxed transcendentals + 16 unboxed `_f64` variants; `constrain`, `fmap`, `lerp`, `smoothstep`, `radians`, `degrees`, `random`, `random_seed` |
| `detail/mcu_pins_ext.h` | 9     | `analog_read_voltage`, `analog_reference`, `analog_read_resolution`, `analog_write_resolution`, `analog_read_oversampled`, `pin_toggle`, `pulse_in`, `shift_out`, `pulse_count` |
| `detail/mcu_memory.h`   | 4     | `arena_peak`, `arena_free`, `free_ram`, `stack_free` (uses `__brkval`/`__heap_start`)                       |
| `detail/mcu_bits.h`     | 9     | `bit_set`, `bit_clear`, `bit_toggle`, `bit_read`, `low_byte`, `high_byte`, `make_word`, `popcount`, `crc8`   |
| `detail/mcu_eeprom.h`   | 4     | `eeprom_read_int`, `eeprom_write_int`, `eeprom_read_float`, `eeprom_write_float` — implemented against `<avr/eeprom.h>` directly |
| `detail/mcu_interrupts.h` | 8   | `attach_interrupt` / `detach_interrupt` (counter-based ISR trampoline for D2/D3), `interrupt_count` / `reset` / `fired`, `watchdog_enable` / `reset` / `disable` |

Two shared globals in `mcu_types.h` — `g_vyne_adc_shift` and
`g_vyne_pwm_bits` — let `arduino.analog_read_resolution(8)` and
`arduino.analog_write_resolution(4)` reconfigure the `analog_read` /
`analog_write` behaviour without an extra function dispatch per call.
Both default to the pre-existing behaviour (`shift = 0`, `bits = 8`), so
sketches that never call the setters emit identical code.

**What is not bound.** `dot`, `norm`, `argmax`, `argmin` are absent from
the `ARDUINO_MAP` table. Adding them requires `VyneArray_f64` /
`VyneArray_i64` container types in `mcu_types.h` plus `boxAny` lowering
for MCU targets — a separate, larger change. Calling these names from a
sketch fails at compile time with "not defined", which is clearer than a
runtime halt. `attach_interrupt` is counter-based, not callback-based: a
Vyne `fn` compiles to `VyneValue fn_X(int, VyneValue*)`, which cannot be
installed as an AVR `void(void)` ISR without compiler-side wrapper
emission. `Wire` (I2C), `SPI`, and `SoftwareSerial` are not bound: those
libraries have `.cpp` sources that would also need the Vyne AVR build
integration to add `arduino/libraries/*/src` to its include path and
link the corresponding objects.

**What is deliberately boxed on AVR.** `VyneValue` is 16 bytes; a `double`
is 4. The emitter's native fast paths run first: typed `Float64` locals,
arithmetic, comparisons, bitwise, and unary all lower to raw C. The box
only appears at dynamic boundaries — an untyped variable, a call into the
boxed runtime, or an expression whose type the emitter cannot prove.
Well-typed sketches keep everything unboxed; `VyneValue` never enters
the hot loop.

---

## Flagship example: TinyML XOR on an Uno R3

`examples/arduino/tinyml_xor.vy` — roughly 180 lines — trains a 2→3→1
fully-connected network from scratch, live on the board. Every weight is
a `Float64` scalar; all state fits in the C stack; one step = one sample.

```bash
./vynec --arduino uno --flash --monitor --port COM3 \
    examples/arduino/tinyml_xor.vy
```

The demo exercises the whole stack end to end:

- `use native arduino;` — the whole MCU runtime is one import
- boot banner and serial self-test of `sin`, `cos`, `sqrt`, `exp`, `log`, `pow`
- `region probe { scratch buf :: Float64[16]; ... }` — a memory-model
  proof: the arena is measured before and after the region and asserted
  to have drifted 0 bytes after rewind
- 30,000 training steps wrapped in `region step { ... }`, with the
  arena readout at every 1,000-step reporting interval
- forward pass, backprop, and SGD update, all in Vyne source
- ReLU hidden, linear output, MSE loss — deliberately avoids every
  transcendental so the demo runs on the vanilla MCU runtime with no
  extended math bindings required

Expected output (verbatim from a run on a genuine Uno R3, 16 MHz):

```
=== Vyne TinyML ===
XOR learner on Uno R3

arena_used at boot: 0 B

Basic math:
  sin(pi/2) = 1.00
  cos(0)    = 1.00
  sqrt(16)  = 4.00

Extended math:
  exp(0)    = 1.00
  log(e)    = 1.00
  pow(2,10) = 1024.00

Memory model:
  buf.mean() = 0.61
  drift      = 0 B

Initializing weights...
  done

Training (SGD, lr=0.05):
  iter  |  loss  |  acc  |  arena
  ------+--------+-------+-------
  1000 | 0.20 | 0.70 | 0 B
  ...
  30000 | 0.09 | 0.91 | 0 B
```

The 91% plateau is a real network-training artifact, not a runtime bug:
with 3 hidden units and ReLU, at least one unit dies permanently during
training, leaving 2 effective units that cannot shape XOR exactly. Leaky
ReLU, more hidden units, or sigmoid + cross-entropy would each break the
plateau and are each a one-line change to the demo. The choice of ReLU +
MSE is deliberate so the demo exercises the base MCU runtime with no
extended math bindings required.

**The important numbers are the ones that stay flat.** `arena_used` is
0 B at every reporting step across 30,000 iterations. `drift` is 0 B
after the probe region. The whole training run — model state, PRNG,
serial output, all of it — fits in the C stack; the arena is never
touched. That is the region-based memory model working on a chip with
2 KB of SRAM.

---

## External library system

Libraries written in Vyne itself live under `modules/external/`. A
facade `.vy` file re-exports the leaf modules via `use lib` and `module`.
The language's own numerical stack is written this way, and the same
pattern applies to any user library.

### Libraries in this repository

| Library    | Purpose                                                                 |
| :--------- | :---------------------------------------------------------------------- |
| `vjson`    | RFC 8259 parser and serializer, built on the vcore string builder       |
| `vlin`     | Dense linear algebra: matrices, matmul, reductions, element-wise ops    |
| `vml`      | Dense neural network primitives: `Dense`, `Sequential`, `SGD`, `Adam`   |
| `vfft`     | Iterative radix-2 Cooley-Tukey FFT with plan caching and bit-reversal   |
| `vrand`    | Distributions and sampling: normal, gamma, beta, Poisson, categorical   |
| `vbio`     | Nucleotide and protein sequence primitives: complement, translation, GC |
| `vcolors`  | ANSI color helpers                                                      |
| `vconvert` | Data format conversion                                                  |
| `vstring`  | String utilities                                                        |
| `vplot`    | Plotting                                                                |

Each library ships with a full manual in its directory. `vlin/README.md`,
`vml/README.md`, `vjson/README.md`, `vfft/README.md`, `vrand/README.md`,
and `vbio/README.md` are the reference for the user-visible surface.

### Kernel/wrapper pattern

Every performance-sensitive library in this repository splits into two
layers:

```
Kernels.vy    # loops over typed arrays; native ABI; no boxing
Ops.vy        # shape checks, allocation, dispatch to kernels
```

The kernels are functions of the form
`(Array<Float64>, Int64) -> Float64` or
`(Array<Float64>, Array<Float64>, Int64) -> Int64`. The transpiler
recognizes this shape and emits a direct C function; the wrappers handle
shape validation, scratch buffer allocation, and dispatch. The same
pattern is what would make a future `.vy` numerical library usable on
both the desktop and the MCU target, since the kernel shape is exactly
what the AVR backend's unboxed dispatch looks for.

### BLAS dispatch

`vlin.multiply` and `vlin.multiply_trans_b` lower to `cblas_dgemm` when
the compiler is invoked with `--blas`. The dispatch table lives in
`compiler/codegen/blas_dispatch.h`; the runtime bridge is
`runtime/detail/blas_bridge.h`. Source is unchanged between the two
modes — only the lowering of the matched calls differs. `--blas` is a
desktop-only flag; the AVR backend has no BLAS path.

---

## Transpiler architecture

### Pipeline

```
source.vy
    │
    ▼
tokenize()          compiler/lexer
    │
    ▼
Parser              compiler/parser
    │  recursive descent, scope tracking, type resolution
    ▼
ASTNode tree        compiler/ast
    │
    ├──► Interpreter    evaluate(env, scopeId) against SymbolContainer
    │
    └──► C_Emitter      compiler/codegen
            │
            ├──► desktop target ──► single .c file, links runtime/vyne_runtime.h
            │
            └──► arduino target ──► single .ino.cpp, links arduino/vyne_arduino.h
```

The emitter is parameterized on target. `C_Emitter::setArduinoTarget(true)`
changes three things: the include preamble
(`#include <Arduino.h>` + `#include "vyne_arduino.h"` instead of
`#include "vyne_runtime.h"`), the entry-point shape (`vyne_setup()` +
`vyne_loop()` instead of `int main(void)`), and the panic handler for
scratch bounds-check failures (`vyne_panic_bounds` which prints over
Serial and halts, instead of `fprintf(stderr, ...) + exit(1)`). Every
AST node lowering path is identical on both targets — the same fast
paths, the same boxing rules, the same region/scratch machinery.

### Codegen structure

The codegen directory is split per feature, not per file-organization
convenience. Each `.cpp` owns one category of AST node and its lowering:

| Source                                              | Responsibility                                          |
| :-------------------------------------------------- | :------------------------------------------------------ |
| `literals.cpp`                                      | Number, string, bool, null                              |
| `assignments.cpp`                                   | Variables, assignment, native init, region escape       |
| `operators.cpp`                                     | Binary, unary, postfix; native fast paths for both      |
| `builtins.cpp`                                      | `out`, `string`, `int64`, `sizeof`, ...                 |
| `control_flow.cpp`, `loops.cpp`                     | `if`, `while`, `through`, `return`, `break`, `continue` |
| `functions.cpp`, `function_calls.cpp`               | Definitions, calls, native dispatch                     |
| `collections.cpp`                                   | Arrays, indexing, slicing, ranges                       |
| `members.cpp`, `interfaces.cpp`, `method_calls.cpp` | Fields and methods                                      |
| `groups_modules.cpp`, `imports.cpp`                 | Groups, modules, imports                                |
| `language_features.cpp`                             | Enums, defer, coalescing, membership, pipeline          |
| `exceptions.cpp`                                    | Try/catch/finally/throw                                 |
| `regions.cpp`                                       | `region`, `@pool`, `@speculative`, `region.commit`      |
| `maps_strings_scratch.cpp`                          | Maps, interpolated strings, shaped scratch arrays       |
| `program.cpp`                                       | Program, block, ternary                                 |
| `native_dispatch.cpp`                               | Native-ABI call lowering                                |
| `blas.cpp`                                          | BLAS lowering under `--blas` (desktop only)             |

`emitter.h` holds all per-compilation state: output streams, scope stack,
static type tables, native variant registry, region stack, pool stack,
speculative stack, monomorphization cache, Arduino target flag. Every
`C_Emitter` method is either a query against that state or a mutation of
it; the AST nodes themselves are immutable after parsing.

### Name mangling

Groups, interfaces, and function names mangle deterministically:

```
Master.Element.getName()      →  fn_Master_Element_getName
vlin.Types.Matrix             →  struct_vlin_Types_Matrix
vlin.cross_entropy            →  fn_vlin_Reductions_cross_entropy
arduino.serial_print          →  arduino_serial_print
```

The mangling scheme is the interface between the compiler and the runtime
method table (`vyne_register_method`). `VYNE_MAX_METHODS` is 4096; the
compiler emits a diagnostic when a program exceeds it. On AVR, the method
table is not emitted at all — the `arduino` module is looked up by name
in the codegen-side `ARDUINO_MAP` table and lowers directly to a `static
inline` C function.

### Native ABI

A function is a candidate for native lowering when:

- every parameter is a primitive (`Int64`, `Float64`, `Bool`), a typed
  array (`Array<Float64>` or `Array<Int64>`), or a struct with a
  registered field layout,
- the return type is a primitive, a typed array, or such a struct, and
- the body does not contain a top-level `defer` or `try`.

Such a function is emitted twice: once with the standard boxed signature
`(int arg_count, VyneValue* args) -> VyneValue`, and once with the native
signature, e.g.

```c
double fn_vlin_k_dot(double* a, double* b, int64_t n);
```

Call sites prefer the native variant when every argument matches the
declared parameter type. Struct-typed parameters expand into one C
argument per field, in field-declaration order. Array-typed parameters
pass `.data` directly. Both targets share this machinery — on AVR, the
native variant is what keeps numerical kernels off the 1 KB arena.

### Monomorphization

Generic functions with explicit type arguments are instantiated per
distinct type tuple. The cache is keyed on
`<mangled-fn-name>__<type-tuple>` and stores the emitted C name. Recursive
instantiation is detected and terminates cleanly.

### Region escape analysis

`checkRegionEscape` in `assignments.cpp` rejects the pattern

```vyne
outer = allocate_inside_region();
```

where `outer` was declared at shallower region depth than the current one
and the RHS is not provably primitive. The diagnostic (`VNE-070`) names
the variable, both depths, the line, and three fixes. This is deliberately
syntactic; the regions paper documents what the check cannot see, and the
current implementation does not attempt to close those gaps. The same
check runs on both the desktop and AVR targets; on AVR the consequences
of a missed escape are more severe (a rewind frees 1 KB of SRAM that
something else may have aliased), so the check is not optional.

---

## Diagnostics

The compiler produces colorized, source-annotated diagnostics with stable
codes. `runtime/diagnostics.h` owns the emission engine; every site in
`compiler/` uses `emitError` / `emitWarning` / `emit`.

```
Error [VNE-070]: variable 'out' is declared outside the current region
    (depth 0) but is being assigned a value allocated inside a region
    (current depth 1) at line 42.
  Suggestions:
    - declare 'out' inside the region, or
    - copy the value out with region.commit(tmp) and assign from tmp, or
    - move the assignment to before the region.
   42 |     out = make_buffer();
      |     ^
```

Selected codes:

| Code      | Meaning                              |
| :-------- | :----------------------------------- |
| `VNE-001` | Unexpected token                     |
| `VNE-002` | Missing semicolon                    |
| `VNE-003` | Unknown type name                    |
| `VNE-005` | Unresolved import                    |
| `VNE-006` | Malformed numeric literal            |
| `VNE-007` | Integer literal overflow             |
| `VNE-070` | Region escape                        |
| `VNE-071` | Scratch shape mismatch               |
| `VNE-072` | Scratch index out of bounds          |
| `VNE-080` | Invalid region policy argument       |
| `VNE-082` | Pool region exhausted                |
| `VNE-083` | Pool intrinsic misuse                |
| `VNE-084` | `region.commit` on typed-array local |
| `VNE-085` | `commit_if` outside `@speculative`   |
| `VNE-090` | `analog_write_resolution` > 8        |
| `VNE-091` | Pin has no external interrupt        |
| `VNE-100` | Method table overflow                |

The full list is in `docs/content/errcodes.html`. On AVR, `VNE-072`
(scratch index out of bounds) lowers to `vyne_panic_bounds` instead of
`fprintf + exit`, since neither is available on the microcontroller —
the emitted firmware prints the diagnostic over Serial and halts.

---

## Performance

### Recursive Fibonacci (n = 30)

Both numbers are wall time of the fib(30) call, measured in-process.
Process startup and codegen/compile time are excluded.

| Backend      | Time     | Relative     |
| :----------- | :------- | :----------- |
| Interpreter  | 54.52 ms | 1.00x        |
| C transpiler | 19.87 ms | 2.74x faster |

Measured on i7-14700, Windows 11. Transpiler build: `--blas --native`,
GCC `-O3 -march=native`. Source is untyped-recursion `fn fib(n)`;
adding `:: Int64` annotations does not change the transpiler result
(the native-variant path is already taken) but does improve the
interpreter by roughly 2x.

### Dense matmul (1024x1024, 100 iterations)

The same Vyne source is compiled three ways:

| Build                             | Kernel                               |
| :-------------------------------- | :----------------------------------- |
| `vynec --compile`                 | Emitted C triple loop                |
| `vynec --compile --blas`          | `cblas_dgemm`                        |
| `vynec --compile --blas --native` | `cblas_dgemm` + host `-march=native` |

See `examples/benchmark/matmul_1024_vlin.vy`. The source is byte-identical
across the three modes; only the lowering of `vlin.multiply` differs.

### End-to-end ML workload (desktop)

`examples/training/ml_seq.vy` trains a 64→16→12→1 classifier on 240 RNA
sequences for 1000 epochs. With regions scoping per-iteration allocations,
peak RSS stays flat regardless of epoch count.

### TinyML XOR (Arduino Uno R3)

The `tinyml_xor.vy` example trains a 2→3→1 MLP for 30,000 online SGD
steps on an ATmega328P at 16 MHz:

| Metric                                | Value                  |
| :------------------------------------ | :--------------------- |
| Training time (30,000 steps)          | ~18–25 s               |
| Arena peak during training            | 0 B                    |
| Stack peak during training            | < 200 B                |
| Final XOR accuracy (10 held-out)      | 91%                    |
| Serial output per 1,000 steps         | 1 line (~40 bytes)     |

All arithmetic is 32-bit `double` (AVR's `double` is a 32-bit float; the
mantissa is 24 bits, and beyond ~4096 averaging iterations further
accumulation stops helping). The 91% plateau is a ReLU dead-unit
artifact, not a runtime issue — see the demo's own header comment and
the [Flagship example](#flagship-example-tinyml-xor-on-an-uno-r3) section.

---

## CLI reference

```
vynec [options] <file.vy>
```

| Flag                 | Effect                                                          |
| :------------------- | :-------------------------------------------------------------- |
| *(none)*             | Run under the interpreter                                       |
| `--compile`          | Transpile to C and invoke the host C compiler                   |
| `--arduino <board>`  | Transpile for an Arduino target; currently only `uno` is defined |
| `--flash`            | After `--arduino`, invoke `arduino-cli upload`                  |
| `--monitor`          | After `--flash`, invoke `arduino-cli monitor`                   |
| `--port <device>`    | Serial port for `--flash` / `--monitor` (e.g. `COM3`)           |
| `--baud <rate>`      | Serial baud rate for `--monitor`; default `115200`              |
| `--interp`           | Force the interpreter even for scripts that ship compiled       |
| `--blas`             | Lower matched matrix calls to BLAS (desktop only)               |
| `--native`           | Add `-march=native` to the host compiler invocation             |
| `--emit-c`           | Emit the C source without invoking the host compiler            |
| `--out <name>`       | Set the output binary name                                      |
| `--quiet`            | Suppress notes and warnings                                     |
| `--strict`           | Enable strict type checking                                     |

Diagnostics are written to stderr. Exit codes are `0` on success, `1` on
compilation or runtime error, `2` on CLI misuse.

For the Arduino target, `--flash` requires `arduino-cli` and the
`arduino:avr` core installed; `--monitor` requires `arduino-cli` and a
connected board. Both flags also accept `--fqbn <string>` to override
the board identifier (default `arduino:avr:uno`).

---

## Build from source

### Prerequisites

- C++26 compiler (GCC 14+ with `-std=c++26`, or equivalent) for the
  compiler and the desktop runtime
- [xmake](https://xmake.io/) — `scoop install xmake` on Windows,
  `brew install xmake` on macOS, `curl -fsSL https://xmake.io/shget.text | bash` on Linux
- A C11 compiler for the generated output (default: the same compiler)
- **For the Arduino target:** `avr-gcc` and `avr-libc` (part of the
  AVR toolchain), plus `arduino-cli` with the `arduino:avr` core
  installed (`arduino-cli core install arduino:avr`). `arduino-cli` is
  only required for `--flash` and `--monitor`; `--arduino uno` alone
  produces the `.hex` given the AVR toolchain.

Vendored dependencies under `vendor/`: OpenBLAS (optional, for `--blas`).

### Build

```bash
xmake
```

Produces `vynec` in the repository root. On Windows, `build.bat` performs
the equivalent.

### Arduino toolchain check

Verify the AVR toolchain is on `PATH` before the first `--arduino uno`
invocation:

```bash
avr-gcc --version            # expect "avr-gcc (GCC) ..."
avr-objcopy --version
arduino-cli core list        # expect "arduino:avr  ... installed"
```

The Vyne AVR build integration invokes `avr-g++`/`avr-gcc` and
`arduino-cli` directly. If those binaries are missing, `--arduino uno`
reports a clear diagnostic instead of a mid-compile `command not found`.

### Test

```bash
xmake test
```

Runs the suite under `tests/`, including compiler tests, numerical
library tests, region-safety harness under `examples/safety/`, and
(if the AVR toolchain is present) a host-side test of the emitted MCU
C translation unit — full firmware verification requires an actual
board and is not part of CI.

---

## Project layout

```
vyne/
├── main.cpp                       # entry point
├── xmake.lua                      # build script
├── compiler/
│   ├── lexer/                     # tokenizer, string interpolation, hex literals
│   ├── parser/                    # recursive descent, type resolution
│   ├── ast/                       # AST nodes, Value type, SymbolContainer
│   │   ├── ast.h / ast.cpp
│   │   ├── ast_helpers.h
│   │   └── value.h / value.cpp
│   ├── codegen/                   # C transpiler (split per feature)
│   │   ├── emitter.h              # per-compilation state (desktop + Arduino)
│   │   ├── ctype.h                # VType -> C type mapping
│   │   ├── native_maps.h          # native module symbol tables
│   │   ├── native_dispatch.cpp    # native-ABI call lowering
│   │   ├── blas.cpp, blas_dispatch.h
│   │   ├── regions.cpp            # region/pool/speculative lowering
│   │   ├── linker.cpp             # import graph, topological order
│   │   └── *.cpp                  # per-feature emitters
│   └── types.h                    # VType enum
│
├── runtime/                       # desktop C runtime, linked into compiled output
│   ├── vyne_runtime.h
│   ├── diagnostics.h              # shared with compiler
│   ├── detail/                    # arena, arrays, maps, strings, structs,
│   │                              # operators, equality, exceptions
│   └── modules/                   # C mirror of each native module
│       ├── vcore.h, vfs.h, vmath.h, vmem.h
│       └── vyne_pool_runtime.h
│
├── arduino/                       # AVR C runtime, linked into firmware
│   ├── vyne_arduino.h             # entry point: setup() / loop() wrapper
│   └── detail/
│       ├── mcu_types.h            # VyneValue, type tags, boxing helpers
│       ├── mcu_arena.h            # 1 KB bump arena, 16-entry checkpoint stack
│       ├── mcu_hardware.h         # base pin/digital/analog
│       ├── mcu_serial.h           # serial input stack (7)
│       ├── mcu_timing.h           # pulse_width, timer_read (2)
│       ├── mcu_math.h             # 18 boxed + 16 _f64 transcendentals (34)
│       ├── mcu_pins_ext.h         # ADC extensions, pulse, shift, toggle (9)
│       ├── mcu_memory.h           # arena_peak/free, free_ram, stack_free (4)
│       ├── mcu_bits.h             # bit ops, popcount, crc8 (9)
│       ├── mcu_eeprom.h           # EEPROM via <avr/eeprom.h> (4)
│       └── mcu_interrupts.h       # interrupts + watchdog (8)
│
├── modules/
│   ├── common/                    # native C++ module bindings
│   │   ├── vcore/                 # system utilities
│   │   ├── vmath/                 # scalar math
│   │   ├── vfs/                   # filesystem
│   │   ├── vmem/                  # memory
│   │   └── vml/                   # ML native bindings
│   └── external/                  # libraries written in Vyne
│       ├── vjson/                 # JSON parser and serializer
│       ├── vlin/                  # linear algebra
│       ├── vml/                   # neural network primitives
│       ├── vfft/                  # FFT with plan caching
│       ├── vrand/                 # distributions and sampling
│       ├── vbio/                  # bioinformatics
│       ├── vcolors.vy, vconvert.vy, vplot.vy, vstring.vy
│
├── cli/                           # REPL, file handler, packager
├── editors/
│   ├── vscode/lsp/                # VS Code extension + LSP backend
│   └── nvim/                      # Neovim plugin
│
├── examples/                      # language and library examples
│   ├── arduino/                   # AVR firmware examples (tinyml_xor.vy, ...)
│   ├── benchmark/                 # matmul, recursion, ML training
│   ├── network/                   # vnet (archival, no longer maintained)
│   ├── safety/                    # region escape negative tests
│   ├── training/                  # vlin/vml training scripts
│   └── transpiler/                # codegen coverage tests
│
├── models/                        # pre-trained weights (.dat)
├── tests/                         # test suite
├── docs/                          # documentation site
├── scripts/                       # build and test scripts
└── vendor/                        # third-party libraries
    └── openblas/
```

---

## Editor support

### VS Code

A language server and extension are under `editors/vscode/lsp/`. The
extension ships syntax highlighting (TextMate grammar), a language
configuration file, and a client for the LSP backend in `backend/`.
Prebuilt VSIX packages are checked in.

### Neovim

A minimal plugin under `editors/nvim/` provides file-type detection
(`ftdetect/vyne.lua`), an LSP client (`lua/vyne_lsp.lua`), and a syntax
file (`syntax/vyne.vim`).

### Doxygen

The compiler and runtime are Doxygen-annotated. `doxygen Doxyfile`
generates HTML documentation covering the AST hierarchy, module
bindings, and call graphs.

---

## Limitations

The project is at an early alpha. The following constraints are known and
tracked; they are documented here so users and contributors have accurate
expectations.

**AVR target is Arduino Uno R3 only.** The `--arduino` flag currently
accepts `uno`. Other ATmega boards (Nano, Mega) share the same core
runtime but their pin maps, memory sizes, and interrupt counts differ;
they are not yet wired up. The `VYNE_MCU_ARENA_SIZE` macro is the only
tunable at present.

**AVR runtime does not bind Wire, SPI, or SoftwareSerial.** Those
libraries ship `.cpp` sources; binding them requires the AVR build
integration to add their `src/` directories to the include path and link
the corresponding objects. The runtime currently ships self-contained
headers under `arduino/detail/` for everything it does expose, and
`mcu_eeprom.h` uses `<avr/eeprom.h>` directly rather than pulling the
bundled Arduino EEPROM library.

**No `dot`, `norm`, `argmax`, `argmin` on AVR.** These would require
`VyneArray_f64` / `VyneArray_i64` containers in `mcu_types.h` plus
`boxAny` lowering for MCU targets. Calling these names from a sketch
fails at compile time with "not defined".

**`attach_interrupt` is counter-based on AVR.** A Vyne `fn` compiles to
`VyneValue fn_X(int, VyneValue*)`, which cannot be installed as an AVR
`void(void)` ISR without compiler-side wrapper emission. The runtime
installs an edge-counting trampoline; sketches poll `interrupt_count` /
`interrupt_fired` / `interrupt_reset`.

**`out()` is not available on AVR.** Use `arduino.serial_print()`.

**No integer overflow checking.** `int64(text)` saturates silently on
out-of-range input. Arithmetic wraps. On AVR, `Int64` is a 64-bit
emulation over 32-bit hardware and is slow — prefer `Float64` for
numerical work, or explicit `int64_t`-clean native paths.

**No GC.** The interpreter relies on arena lifetime for the compiled
backend and on `shared_ptr` for interpreter-only object graphs. Programs
that accumulate long-lived values without region scoping will grow
linearly. `vmem` is the intended tool.

**Single-process RNG.** `vmath`'s PCG32 state is process-global. Two
independent streams require a state-carrying RNG type that has not yet
landed. On AVR the same caveat applies; the TinyML demo avoids
`arduino.random` and implements its own LCG in Vyne source precisely so
its trajectory is reproducible across boots.

**Native-to-native array calls are rejected.** A function whose body calls
another function taking an array argument must inline that call or opt out
of native registration. This is a codegen constraint and it is documented
at the call sites that care.

**Codegen is not yet complete in every corner.** The generic-array
element-type inference has a fallback path that keeps `vml.forward_all`
returning a boxed array. `relu_prime` in `vlin` produces null elements
under the current `ForNode::getCExpr` emission. These are tracked; the
user-visible workarounds are documented in the affected library manuals.

**Modules from earlier releases** — `vglib`, `vaudio`, `vserv`, `vnet`,
`vcv`, `vurage` — are no longer maintained as the language's purpose has
shifted to scientific numerical computing with a first-class embedded
target. Users depending on them should pin to a pre-0.4 tag. See
[Standard library](#standard-library).

---

## Contributing

Contributions are welcome. Before opening a pull request:

1. Fork the repository and create a feature branch.
2. Follow the existing code style: feature-split `.cpp` files in
   `compiler/codegen/`, `static inline` functions in `runtime/detail/`,
   `.vy` leaf modules with a facade under `modules/external/`, and
   self-contained `static inline` headers under `arduino/detail/`
   (no `#include <EEPROM.h>`-style dependencies on bundled libraries).
3. Add tests. New language features belong under `examples/transpiler/`
   or `tests/`; new library functions belong in the library's own test
   file and, where applicable, in `examples/`. New Arduino bindings
   belong in the matching `arduino/detail/mcu_*.h` and the corresponding
   entry in `compiler/codegen/native_maps.h`.
4. Update the relevant README under `modules/external/<lib>/` if the
   public surface changed.
5. Run `xmake test` and ensure everything passes.

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for the full guide. Bug reports
and feature requests use the templates under `.github/ISSUE_TEMPLATE/`.

---

## License

Vyne is dual-licensed: the compiler is AGPL-3.0-only, the runtime is MIT.
See [LICENSE](LICENSE), [LICENSE-MIT](LICENSE-MIT), and
[TRADEMARK.md](TRADEMARK.md) for details.

---

## Acknowledgments

- **OpenBLAS** for the matmul kernels dispatched under `--blas`.
- **avr-gcc** and **avr-libc** for the toolchain that makes the
  Arduino target possible.
- **The Arduino project** for the ATmega328P core that the Vyne AVR
  runtime's `VyneValue` layout and `Serial` printing conventions are
  modelled on.
- Everyone who has filed issues, contributed patches, or shipped code in
  Vyne.
