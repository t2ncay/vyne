# Vyne AVR Runtime

> The C runtime that ships with every Vyne-generated Arduino Uno R3
> firmware. Single-header-per-concern, ~60 native bindings, deterministic
> memory, zero dependencies on the bundled Arduino libraries.

[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](../../LICENSE-MIT)
[![C++11](https://img.shields.io/badge/C%2B%2B-11-00599C.svg)](https://isocpp.org/)
[![Target](https://img.shields.io/badge/target-ATmega328P-orange.svg)](#target)

---

## Overview

This directory contains the entire Arduino runtime that a
Vyne-generated firmware image links against. It is a single translation
unit (`vyne_arduino.h`) that pulls in a set of small, self-contained
headers under `detail/`.

The runtime has two responsibilities:

1. **Bridge the Arduino entry-point contract.** `vyne_arduino.h` defines
   the standard `setup()` and `loop()` symbols that the Arduino core
   invokes, and dispatches them into the user program's
   `vyne_setup()` / `vyne_loop()`.
2. **Provide the Vyne-side `arduino.*` namespace.** Every function
   reachable from a Vyne sketch via `use native arduino;` is a
   `static inline` C function defined in one of the `detail/mcu_*.h`
   headers. The codegen side (`compiler/codegen/native_maps.h`)
   resolves the Vyne name to the corresponding C symbol.

The runtime is intentionally small — about 4 KB of flash when idle — and
has **no dependency on any bundled Arduino library**. Every header pulls
from `<Arduino.h>` and `<avr-libc>` only. This is not a stylistic
preference; the Vyne AVR build integration adds only
`arduino/cores/arduino/` to the compiler's include path, so
`#include <EEPROM.h>` (which lives at `arduino/libraries/EEPROM/src/`)
would fail to link. `mcu_eeprom.h` therefore implements EEPROM access
directly against `<avr/eeprom.h>`, which ships with avr-libc and is
always present.

The same rule applies to every future binding: **if a feature needs a
bundled library, either reimplement it against avr-libc or teach the
build integration to compile the library.** The first is usually the
smaller change; the second is a separate piece of work.

---

## Table of contents

- [Quick reference](#quick-reference)
- [Target](#target)
- [Design constraints](#design-constraints)
- [Architecture](#architecture)
- [Memory model](#memory-model)
- [API reference](#api-reference)
  - [Core I/O — `mcu_hardware.h`](#core-io--mcu_hardwareh)
  - [Serial — `mcu_serial.h`](#serial--mcu_serialh)
  - [Timing — `mcu_timing.h`](#timing--mcu_timingh)
  - [Math — `mcu_math.h`](#math--mcu_mathh)
  - [Extended pins — `mcu_pins_ext.h`](#extended-pins--mcu_pins_exth)
  - [Memory — `mcu_memory.h`](#memory--mcu_memoryh)
  - [Bits — `mcu_bits.h`](#bits--mcu_bitsh)
  - [EEPROM — `mcu_eeprom.h`](#eeprom--mcu_eepromh)
  - [I2C — `mcu_i2c.h`](#i2c--mcu_i2ch)
  - [SPI — `mcu_spi.h`](#spi--mcu_spih)
  - [Interrupts — `mcu_interrupts.h`](#interrupts--mcu_interruptsh)
  - [Watchdog — `mcu_interrupts.h`](#watchdog--mcu_interruptsh)
  - [Typed arrays — `mcu_types.h`](#typed-arrays--mcu_typesh)
  - [Arena — `mcu_arena.h`](#arena--mcu_arenah)
- [Common patterns](#common-patterns)
- [Extending the runtime](#extending-the-runtime)
- [What is not bound and why](#what-is-not-bound-and-why)
- [Testing](#testing)
- [Limitations](#limitations)
- [Contributing](#contributing)
- [License](#license)

---

## Quick reference

| Header | Functions | Namespace | Purpose |
| :--- | :---: | :--- | :--- |
| `detail/mcu_hardware.h` | 14 | `arduino.*` | Pin mode, digital and analog I/O, delays, base trig, `arena_used` |
| `detail/mcu_serial.h` | 7 | `arduino.*` | Available, read, read_line, parse, println, flush |
| `detail/mcu_timing.h` | 2 | `arduino.*` | Pulse width, Timer1 read |
| `detail/mcu_math.h` | 34 | `arduino.*` | Full transcendental family (boxed + `_f64` unboxed variants) |
| `detail/mcu_pins_ext.h` | 9 | `arduino.*` | ADC voltage/reference/resolution, oversampling, toggle, pulse, shift, count |
| `detail/mcu_memory.h` | 4 | `arduino.*` | Arena peak/free, free RAM, stack free |
| `detail/mcu_bits.h` | 9 | `arduino.*` | Bit set/clear/toggle/read, byte split/merge, popcount, CRC-8 |
| `detail/mcu_eeprom.h` | 4 | `arduino.*` | Read/write Int64 and Float64 across resets |
| `detail/mcu_i2c.h` | 8 | `arduino.*` | I2C master via `Wire`: byte/register read-write, clock |
| `detail/mcu_spi.h` | 6 | `arduino.*` | SPI master via `SPI`: settings, byte/word transfer |
| `detail/mcu_interrupts.h` | 8 | `arduino.*` | Counter-based external interrupts (D2/D3) + watchdog |
| `detail/mcu_types.h` | 23 | internal | `VyneValue`, typed-array containers, box/unbox |
| `detail/mcu_arena.h` | 6 | internal | Bump allocator, checkpoint stack, `arena_alloc` |

Total exposed to Vyne source: **105 functions** (91 core + 14 peripheral).

---

## Target

The runtime is written specifically for the **ATmega328P** as found on
the Arduino Uno R3. Characteristics that shaped every design decision:

| | |
| :--- | :--- |
| Flash | 32 KB |
| SRAM | 2 KB |
| EEPROM | 1 KB |
| Clock | 16 MHz |
| ADC | 10-bit, 6 channels |
| PWM | 8-bit physical, 6 capable pins |
| External interrupts | 2 (D2, D3) |
| Timers | 3 (0, 1, 2) |

The `--arduino` flag currently accepts only `uno`. Other ATmega boards
(Nano, Mega) share the same core runtime but their pin maps, memory
sizes, and interrupt counts differ; they are not yet wired up. The
`VYNE_MCU_ARENA_SIZE` macro is the only tunable at present.

---

## Design constraints

The runtime is a strict subset of what a desktop Vyne program can call.
The subset is not arbitrary; it is the set of operations whose semantics
fit in 2 KB of SRAM and 32 KB of flash while keeping the region/scratch
memory model intact.

### Memory discipline

- **The arena is 1 KB.** Every `arena_alloc` call bumps a pointer
  through `g_mcu_arena[1024]`. Exhaustion halts the board with
  `while (1) ;`, the standard firmware-halt idiom. There is no recovery;
  the correct fix is to wrap the offending code in a `region { ... }`
  block so that the arena is reclaimed at the closing brace.
- **`scratch` is stack-allocated.** `scratch buf :: Float64[16]` lowers
  to `double buf[16];` on the AVR stack. The stack has roughly 1.5 KB of
  headroom after the frame pointers and interrupt return addresses are
  placed. Scratch arrays are the correct tool for small fixed-size
  numerical buffers; the arena is the correct tool for anything that
  needs to outlive the enclosing block.
- **`Array<Float64>` and `Array<Int64>` are typed.** An `Array<Float64>`
  of length `N` costs `N * 4` bytes plus a 12-byte container header. An
  untyped `Array` of the same length costs `N * 16` bytes plus the same
  header. On a 1 KB arena that factor-of-four difference is the
  difference between fitting and not fitting. **Use the annotated form
  whenever possible on AVR.**
- **Growth allocates; the rewind reclaims.** `arr.push(x)` grows by
  allocating a fresh buffer (1.5×, minimum 8 elements) and copying. The
  old buffer remains live in the arena until the enclosing region
  rewinds. Repeating push without a region will exhaust the arena. This
  is documented at each `push` call site in `mcu_types.h`.

### Numeric discipline

- **`double` is 32-bit.** AVR's `double` type is a 4-byte IEEE-754
  single-precision float. It has a 24-bit mantissa, an 8-bit exponent,
  and roughly 7 decimal digits of precision. It is not a
  double-precision float. `Float64` in Vyne source lowers to this type
  on AVR, matching the desktop semantics at the source level but not at
  the bit level. If your algorithm depends on 53-bit precision, it will
  not produce the same result on both targets.
- **`int64_t` is emulated.** AVR has no 64-bit arithmetic unit. Every
  `int64_t` operation compiles to a sequence of 32-bit operations
  through libgcc's `__muldi3`, `__divdi3`, `__udivdi3`, etc. It is
  correct but slow — roughly 8–20× the cost of a 32-bit operation. The
  `arduino.*` namespace offers `Int64` where the Arduino API requires
  it (millis, micros, EEPROM, interrupt counts) but the numerical
  fast-paths prefer `Float64`.
- **No integer overflow checking.** Arithmetic wraps silently. This is
  consistent with the desktop compiled backend.

### Timing discipline

- **No timer-interrupts exposed.** The Vyne `attach_interrupt` binding
  is counter-based (an edge-counting trampoline on D2/D3), not
  callback-based. A Vyne `fn` compiles to
  `VyneValue fn_X(int, VyneValue*)`, which cannot be installed as an
  AVR `void(void)` ISR without compiler-side wrapper emission — that is
  a separate, larger feature. `arduino.timer_read()` exposes Timer1's
  raw counter for sketches that configure the timer themselves via
  avr-libc registers, but no high-level timer API exists.
- **`delay()` and `delayMicroseconds()` block.** They call the standard
  Arduino core functions, which busy-wait. They do not yield to the
  runtime. If a sketch needs to respond to input during a delay, it
  must use `millis()` polling instead.

### Flash discipline

- **Every boxed `VyneValue` costs code.** A `VyneValue` is 16 bytes on
  AVR. Constructing one costs a struct copy plus a type-tag write.
  Reading one back into a native scalar costs a branch on `.type` and
  a union read. The emitter's native fast paths skip this when the
  operand is provably typed, but mixed or dynamic expressions pay the
  cost. The first time a sketch's flash usage looks surprising, look
  for places where a `Float64` local became an untyped variable.
- **The printing engine is not linked on demand.** `_vyne_print_internal`
  is a single `if`-chain over the six value tags. It is small. But if a
  sketch only uses `arduino.serial_print` and never `out`, the
  `String` printing branch is dead code that the linker may or may not
  strip depending on `--gc-sections`. In practice the difference is
  under 200 bytes.

---

## Architecture

### Entry point

`vyne_arduino.h` is the file every sketch's generated `.ino.cpp`
includes first. It performs three jobs:

```cpp
extern void vyne_setup();
extern void vyne_loop();

void setup() {
    ::Serial.begin(115200);
    arena_free_all();
    vyne_setup();
}

void loop() {
    vyne_loop();
}
```

The code generator emits `vyne_setup()` and `vyne_loop()` — **not**
`setup()` and `loop()` — because `vyne_arduino.h` already defines those
two symbols and a firmware that defined them twice would not link.

`Serial.begin(115200)` happens before `vyne_setup()` so that any print
statement in the user's top-level code is visible in the serial monitor
from the first iteration.

`arena_free_all()` resets the bump pointer and the checkpoint stack.
It is idempotent; a boot that immediately exits `vyne_setup()` still
has a clean arena for the first `loop()` call.

### Header layout

```
arduino/
├── vyne_arduino.h          # entry point, brings in every detail header
└── detail/
    ├── mcu_arena.h         # bump allocator + checkpoint stack
    ├── mcu_types.h         # VyneValue, typed-array containers, boxing
    ├── mcu_hardware.h      # base pin / digital / analog + region shims
    ├── mcu_serial.h        # serial input stack
    ├── mcu_timing.h        # pulse_width, timer_read
    ├── mcu_math.h          # transcendental family
    ├── mcu_pins_ext.h      # ADC extensions, pulse, shift, toggle
    ├── mcu_memory.h        # arena_peak/free, free_ram, stack_free
    ├── mcu_bits.h          # bit ops, popcount, crc8
    ├── mcu_eeprom.h        # EEPROM via <avr/eeprom.h>
    ├── mcu_i2c.h           # I2C master via bundled <Wire.h>  (opt-in)
    ├── mcu_spi.h           # SPI master via bundled <SPI.h>    (opt-in)
    └── mcu_interrupts.h    # counter-based interrupts + watchdog
```

`mcu_i2c.h` and `mcu_spi.h` are included only when the sketch actually
calls an `arduino.i2c_*` / `arduino.spi_*` binding. The emitter records
that use while lowering the body and emits `VYNE_USE_I2C` /
`VYNE_USE_SPI` before including `vyne_arduino.h`, so a blink sketch does
not pull in the `Wire` / `SPI` globals or their flash cost.

Headers are included in dependency order:

- `mcu_arena.h` — pure C, no Vyne types
- `mcu_types.h` — depends on `mcu_arena.h`, defines `VyneValue`
- every other `mcu_*.h` — depends on `mcu_types.h` only

No header has an include cycle. Every header compiles standalone if you
include it directly in a test file. That is what makes it possible to
unit-test the runtime on the desktop (see [Testing](#testing)).

### Naming conventions

Two namespaces coexist for every binding:

| Layer | Convention | Example |
| :--- | :--- | :--- |
| Vyne source | `arduino.<name>(...)` | `arduino.pin_mode(13, 1)` |
| C symbol | `arduino_<name>(...)` | `arduino_pin_mode(pin, mode)` |
| Unboxed variant | `arduino_<name>_f64(...)` | `arduino_sin_f64(x)` |
| Variadic | `arduino_<name>_variadic(...)` | `arduino_random_variadic(argc, argv)` |

The `_f64` variants are picked by the emitter when every call-site
argument is a provably-`Float64` native expression. The boxed form is
the fallback. The two are functionally identical — the `_f64` version
just skips the `VyneValue` construction round-trip.

The `_variadic` form exists when the Arduino API is overloaded (e.g.
`random(max)` and `random(min, max)`). C has no overloading, so the
dispatch happens at runtime on `argc`.

### Region shims

`codegen/regions.cpp` emits `vmem_runtime_checkpoint()` and
`vmem_runtime_rewind(cp)` on every target. On desktop, `vmem.h` provides
those symbols. On AVR, `vmem.h` is deliberately not included — the MCU
runtime aliases the pair onto the single MCU arena:

```cpp
static inline VyneValue vmem_runtime_checkpoint(void);
static inline void vmem_runtime_rewind(VyneValue cp);
```

The checkpoint handle is a `VyneValue` carrying the byte offset as
`.as.i64`, matching the desktop signature the emitter expects. The
implementation just wraps `arena_checkpoint()` / `arena_rewind()`.

---

## Memory model

The AVR memory model mirrors the desktop model with two differences:
the sizes are smaller, and the arena lives in `.bss` instead of on the
heap.

### Arena

```cpp
#define VYNE_MCU_ARENA_SIZE 1024

static uint8_t  g_mcu_arena[VYNE_MCU_ARENA_SIZE]; // in .bss
static size_t   g_mcu_cur  = 0;
static size_t   g_mcu_peak = 0;
```

`arena_alloc(n)`:

1. Rounds `n` up to the next 8-byte boundary (`(n + 7) & ~7`).
2. Checks `g_mcu_cur + n > VYNE_MCU_ARENA_SIZE`.
3. On overflow: `while (1) ;` — the board halts. There is no recovery
   path. The arena is a trust boundary: if a program overflows it, the
   program is wrong.
4. Otherwise: returns `&g_mcu_arena[g_mcu_cur]` and advances
   `g_mcu_cur`. Updates `g_mcu_peak` if the new offset is a new
   high-water mark.

The peak counter is what `arduino.arena_peak()` returns. It is not
reset until `arena_free_all()`.

### Checkpoint stack

```cpp
static size_t g_mcu_cp_stack[16];
static int    g_mcu_cp_top = 0;
```

Sixteen is the nesting depth limit. A program with more than sixteen
nested `region` blocks will silently overwrite the stack. The
`RegionNode::compile` path does not check this on AVR; it is a
documented limitation of the current runtime.

A checkpoint records `g_mcu_cur` as a `size_t` offset. `arena_rewind(cp)`
sets `g_mcu_cur = cp.offset`. Any `g_mcu_peak` update persists across
the rewind (peak is high-water, not current).

### Typed-array containers

Three container types exist, distinguished by `VyneValue.type`:

| Container | Tag | Element | Cost per element |
| :--- | :--- | :--- | :--- |
| `VyneArray` | `V_ARRAY` | `VyneValue` | 16 bytes |
| `VyneArray_f64` | `V_F64_ARRAY` | `double` | 4 bytes |
| `VyneArray_i64` | `V_I64_ARRAY` | `int64_t` | 8 bytes |

All three have their `size` and `cap` fields at the same offsets in
their respective structs:

```cpp
struct VyneArray     { VyneValue* elements; int64_t size; int64_t cap; };
struct VyneArray_f64 { double*    data;     int64_t size; int64_t cap; };
struct VyneArray_i64 { int64_t*   data;     int64_t size; int64_t cap; };
```

So `v.as.arr->size` reads the correct integer regardless of which kind
was boxed. Only `.elements` vs `.data` differ, and every read of
`.elements` is guarded by a `.type == V_ARRAY` branch. This is what
makes boxing O(1) — it is a pointer pun, not a copy.

### String interning

`vyne_string_static(s)` returns a `VyneValue` pointing into `.rodata`.
No allocation. `vyne_string(s)` copies `s` into the arena. On AVR the
string pool is not a separate structure — there is one arena, and string
literals either live in flash (static form) or in the arena (dynamic
form). The distinction matters for region escape: a static string
cannot be invalidated by a rewind; a dynamic one can.

---

## API reference

Every function below is available in Vyne source via
`use native arduino;`. The Vyne signature is shown; the C symbol is
in the code block for reference but should never be called directly
from Vyne.

### Core I/O — `mcu_hardware.h`

```vyne
arduino.pin_mode(pin :: Int64, mode :: Int64)
```

Configure a GPIO pin.

| Parameter | Values |
| :--- | :--- |
| `pin` | 0–13 (digital), 0–5 (analog pins used as digital 14–19) |
| `mode` | `0` = INPUT, `1` = OUTPUT, `2` = INPUT_PULLUP |

Returns `null`.

```vyne
arduino.digital_write(pin :: Int64, val :: Int64)
```

Drive a pin low or high. `val` is `0` for LOW and any non-zero value for
HIGH. Returns `null`.

```vyne
arduino.digital_read(pin :: Int64) -> Int64
```

Read a pin. Returns `0` or `1`.

```vyne
arduino.analog_read(pin :: Int64) -> Int64
```

Read the ADC. By default returns `0..1023` (10-bit). If
`arduino.analog_read_resolution(bits)` has been called, the raw value
is right-shifted by `10 - bits` before returning, so the range becomes
`0..(2^bits - 1)`.

```vyne
arduino.analog_write(pin :: Int64, val :: Int64)
```

Write a PWM value. By default accepts `0..255` (8-bit). If
`arduino.analog_write_resolution(bits)` has been called with `bits < 8`,
the input is scaled up to `0..255` before reaching the hardware. Values
outside the current range are clamped. Returns `null`.

```vyne
arduino.delay(ms :: Int64)
```

Busy-wait for `ms` milliseconds. Blocks. Returns `null`.

```vyne
arduino.delay_microseconds(us :: Int64)
```

Busy-wait for `us` microseconds. Uses
`delayMicroseconds()`, which is cycle-accurate at the requested
resolution but does not yield to interrupts consistently below ~2 µs.
Returns `null`.

```vyne
arduino.millis() -> Int64
```

Milliseconds since boot. Wraps at 2^32 (about 49.7 days). Returns `Int64`.

```vyne
arduino.micros() -> Int64
```

Microseconds since boot. Wraps at 2^32 (about 71 minutes). Returns
`Int64`.

```vyne
arduino.serial_begin(baud :: Int64)
```

Configure the hardware UART. Standard rates: 9600, 19200, 38400, 57600,
115200. The Vyne entry point already calls this with 115200 before
`vyne_setup()`; a second call from Vyne source is idempotent. Returns
`null`.

```vyne
arduino.serial_print(v)
```

Write `v` to the serial port. `v` may be `Int64`, `Float64`, `String`,
`Bool`, or `null`. Uses the same formatting as `out(v)` on desktop. No
trailing newline. Returns `null`.

```vyne
arduino.sin(x :: Float64) -> Float64
arduino.cos(x :: Float64) -> Float64
arduino.sqrt(x :: Float64) -> Float64
```

Single-precision versions of the standard library. These are also
available in the extended math family below with the same signature;
the duplicates here exist so that a sketch using only the base runtime
does not need to include `mcu_math.h`.

```vyne
arduino.arena_used() -> Int64
```

Bytes currently in use in the MCU arena. The intended use is to measure
`drift = after - before` around a region body to verify that the rewind
fired. Returns `Int64`.

The following symbols are internal — emitted by the code generator,
never called from Vyne source:

```c
VyneValue vmem_runtime_checkpoint(void);
void      vmem_runtime_rewind(VyneValue cp);
void      vyne_panic_bounds(const char* dimLabel, int shapeBound,
                            int line, int64_t got);
```

`vyne_panic_bounds` replaces `fprintf(stderr, ...) + exit(1)` on
desktop. It prints the diagnostic over Serial and halts. This is the
MCU substitute for the scratch bounds-check failure path (`VNE-072`).

---

### Serial — `mcu_serial.h`

```vyne
arduino.serial_available() -> Int64
```

Bytes in the RX buffer. `0` when empty. Non-blocking.

```vyne
arduino.serial_read() -> Int64
```

Read one byte. Returns `-1` when the RX buffer is empty. Non-blocking.

```vyne
arduino.serial_read_line() -> String
```

Read a line. Blocks up to 1 second waiting for `\n`. Returns the line
without the trailing newline or carriage return. Buffer limit is
63 bytes; longer lines are truncated at 63 characters and the remainder
is discarded. The result is copied into the arena, so a call from
inside a `region { }` block is reclaimed at the closing brace.

```vyne
arduino.serial_parse_int() -> Int64
```

Read the next integer from the serial stream using
`Serial.parseInt()` with its default 1-second timeout. Blocks.
Returns `0` on timeout.

```vyne
arduino.serial_parse_float() -> Float64
```

Read the next float. Same blocking / timeout behaviour as
`serial_parse_int`.

```vyne
arduino.serial_println(v)
```

Same as `serial_print(v)` followed by a newline. Returns `null`.

```vyne
arduino.serial_flush() -> null
```

Wait for the transmit buffer to drain. Returns `null`.

---

### Timing — `mcu_timing.h`

```vyne
arduino.pulse_width(pin :: Int64) -> Int64
```

Measure the HIGH portion of the next pulse on `pin`, in microseconds.
Returns `0` on timeout (default 1 second, matching `pulseIn`). For an
active-LOW sensor, call `arduino.pulse_in(pin, 0, timeout_us)` instead.

```vyne
arduino.timer_read() -> Int64
```

Read Timer1's raw 16-bit counter (`TCNT1`). Returns `0` until the
sketch configures Timer1 via avr-libc registers (`TCCR1A`, `TCCR1B`,
`OCR1A`, prescaler). Wraps at 65536 counts; the tick interval depends on
the prescaler. This is a low-level escape hatch; most sketches should
not call it.

---

### Math — `mcu_math.h`

The math family exposes both a boxed form (works on any argument kind)
and an unboxed `_f64` form. The emitter selects the unboxed form when
every call-site argument is a provably-`Float64` native expression. The
two forms are functionally identical.

#### Transcendentals

```vyne
arduino.tan(x)        -> Float64
arduino.asin(x)       -> Float64
arduino.acos(x)       -> Float64
arduino.atan(x)       -> Float64
arduino.atan2(y, x)   -> Float64
arduino.log(x)        -> Float64
arduino.log10(x)      -> Float64
arduino.exp(x)        -> Float64
arduino.pow(x, y)     -> Float64
arduino.fmod(x, y)    -> Float64
```

Unboxed forms: `arduino_tan_f64`, `arduino_asin_f64`, ...,
`arduino_fmod_f64`. Prefer writing the call with `Float64` arguments so
the emitter picks the unboxed form.

Domain / range behaviour follows avr-libc's `<math.h>`:

- `tan` and `atan` are defined for all inputs.
- `asin` and `acos` return NaN for arguments outside `[-1, 1]`.
- `log` and `log10` return `-inf` for `0`, NaN for negative arguments.
- `exp` saturates to `+inf` at large positive inputs.
- `pow` returns NaN for `pow(negative, non-integer)`.

The caller is responsible for domain clipping. `arduino.constrain` is
usually the right tool.

#### Interpolation and clamping

```vyne
arduino.constrain(x, lo, hi) -> Float64
```

Returns `lo` if `x < lo`, `hi` if `x > hi`, otherwise `x`. If `lo > hi`,
the result is `hi` (matching the desktop `vmath.clamp` behaviour).

```vyne
arduino.fmap(x, in_lo, in_hi, out_lo, out_hi) -> Float64
```

Linear remap. `out_lo + (x - in_lo) * (out_hi - out_lo) / (in_hi - in_lo)`.
This is the Arduino `map()` function for floats — the function every
ADC-to-physical-unit conversion goes through. Division by zero on a
degenerate input range is not guarded; NaN propagates to the use site.

```vyne
arduino.lerp(a, b, t) -> Float64
```

`a + (b - a) * t`. `t` is not clamped; extrapolation is the caller's
choice.

```vyne
arduino.smoothstep(a, b, x) -> Float64
```

Hermite ramp from `a` to `b`, evaluated at `x`. Clamped to `[0, 1]` so
out-of-range `x` produces a plateau, matching GLSL semantics. Standard
for fading LEDs and soft sensor thresholds.

```vyne
arduino.radians(deg) -> Float64
arduino.degrees(rad) -> Float64
```

Angle unit conversion. `VYNE_PI` (defined as `3.14159265358979323846`
in `mcu_math.h`) is used, not `<math.h>`'s `M_PI`.

#### Random

```vyne
arduino.random(max)        -> Int64
arduino.random(min, max)   -> Int64
arduino.random_seed(seed)
```

Thin wrappers around the Arduino core's `random()` and `randomSeed()`.
Because C has no overloading, the dispatch is variadic at runtime
(`arduino_random_variadic(argc, argv)`).

- `arduino.random(max)` returns `0..max-1`.
- `arduino.random(min, max)` returns `min..max-1`.
- `arduino.random_seed(seed)` reseeds the LCG. Passing `0` is a no-op,
  matching Arduino.

The TinyML example deliberately does **not** use these — it implements
its own LCG in Vyne source so that the trajectory is reproducible across
boots and the demo depends on nothing beyond the base runtime. For a
sketch that just wants "some randomness", these are the right call.

---

### Extended pins — `mcu_pins_ext.h`

```vyne
arduino.analog_read_voltage(pin :: Int64) -> Float64
```

Convenience wrapper. Returns `analogRead(pin) * 5.0 / 1023.0`. This is
the ADC value in volts assuming the default 5 V reference and 10-bit
resolution. If the sketch has changed the reference via
`arduino.analog_reference()` or the resolution via
`arduino.analog_read_resolution()`, this function does not account for
either — the caller must scale manually.

```vyne
arduino.analog_reference(mode :: Int64)
```

| `mode` | Effect on Uno R3 |
| :---: | :--- |
| `0` | DEFAULT — 5 V |
| `1` | INTERNAL — 1.1 V (small signals, high precision) |
| `2` | EXTERNAL — external AREF pin |

Subsequent `analog_read` calls use the new reference. Returns `null`.

```vyne
arduino.analog_read_resolution(bits :: Int64)
```

Set the ADC resolution. `bits` is clamped to `1..10`. The value is
stored as a right-shift amount in `g_vyne_adc_shift`; subsequent
`analog_read` calls return `raw >> g_vyne_adc_shift`.

The raw ADC is always 10-bit. Asking for 8 bits does not improve
hardware precision — it only reduces the reported range. It does save
a few cycles per read and reduces downstream `VyneValue` boxing costs
by keeping values inside the `0..255` window. Returns `null`.

```vyne
arduino.analog_write_resolution(bits :: Int64)
```

Set the PWM input range. AVR's PWM hardware is 8-bit; asking for more
than 8 is clamped to 8 with a `VNE-090` warning. Asking for fewer
scales the input up to `0..255` before writing.

`arduino.analog_write_resolution(4)` followed by
`arduino.analog_write(pin, 15)` produces a 100% duty cycle; `0` produces
0%. This is the correct way to expose a coarse-grained control surface
(LED brightness in 16 steps, motor throttle in 16 steps) without
incurring the full 8-bit resolution cost. Returns `null`.

```vyne
arduino.analog_read_oversampled(pin :: Int64, n :: Int64) -> Float64
```

Average `n` consecutive reads and return the mean as `Float64`. SNR
improves by roughly `sqrt(n)`; on the Uno's 10-bit ADC, `n = 16` recovers
about 2 effective bits, `n = 64` about 3.

Two caveats. First, the accumulation saturates the 24-bit mantissa of
AVR's `double` beyond roughly `n = 4096`; further averaging does not
help. Second, the function blocks for `n * 100 µs` (the ADC conversion
time), so `n = 64` blocks for ~6.4 ms. In a hot loop, sample in the
background via `analogRead` and average with a rolling window instead.

```vyne
arduino.pin_toggle(pin :: Int64)
```

Equivalent to `digitalWrite(pin, !digitalRead(pin))`. Saves a function
call and a variable. Returns `null`.

```vyne
arduino.pulse_in(pin :: Int64, state :: Int64, timeout_us :: Int64) -> Int64
```

Measure the duration of a pulse on `pin`. `state` selects which level to
measure (`0` = LOW portion, `1` = HIGH portion). Returns the pulse width
in microseconds, or `0` on timeout. Uses the Arduino core's
`pulseIn()`, which blocks.

The maximum measurable pulse width is limited by `timeout_us`. The
default in `pulseIn()` is 1 second; pass a smaller value if the sketch
has other work to do.

```vyne
arduino.shift_out(data_pin :: Int64, clk_pin :: Int64,
                  bit_order :: Int64, val :: Int64, nbits :: Int64)
```

Bit-bang `nbits` bits out of `val` on `data_pin`, clocking on `clk_pin`.
`bit_order` is `0` for LSB-first, non-zero for MSB-first.

The built-in `shiftOut()` only handles 8 bits; this implementation
open-codes the loop so it works for any width from 1 to 32. Returns
`null`.

```vyne
arduino.pulse_count(pin :: Int64, window_ms :: Int64) -> Int64
```

Count LOW-to-HIGH transitions on `pin` over `window_ms` milliseconds.
Polling-based; the achievable rate on a 16 MHz Uno is roughly 5 kHz. For
higher rates, use `arduino.attach_interrupt` on D2 or D3 plus
`arduino.interrupt_count`.

Returns the count as `Int64`. Note the transition definition: it counts
rising edges, not pulses. A 50% duty cycle signal at 1 kHz produces
1000 counts per second.

---

### Memory — `mcu_memory.h`

```vyne
arduino.arena_peak() -> Int64
arduino.arena_free() -> Int64
```

Peak arena usage (high-water mark, persists across rewinds) and
remaining free bytes in the arena before the next `arena_alloc` halts
the board. These are diagnostics; calling them in a loop is a mistake
because their return values do not change until an allocation occurs.

```vyne
arduino.free_ram() -> Int64
arduino.stack_free() -> Int64
```

Free SRAM between the top of the heap and the current stack pointer.
These are the numbers that answer "am I about to crash?". They use the
standard avr-libc `__brkval` / `__heap_start` trick: `__brkval` is `0`
until the first heap allocation, so on a fresh boot the free-RAM
computation falls back to `__heap_start`.

Both names are exposed because some developers prefer the explicit
stack framing. On AVR without stack-painting instrumentation, there is
no separate "stack watermark" — the two functions return the same
value.

**Calling these from inside a hot loop is a mistake.** `free_ram` is
five instructions but the value changes slowly; polling it in a loop
tells you nothing new. Call it once at boot, once at exit, and once
around any suspicious allocation.

---

### Bits — `mcu_bits.h`

All bit-manipulation functions operate on `Int64`. On AVR the `Int64`
type is emulated; for values that fit in 32 bits, cast down before
calling to save libgcc cycles.

```vyne
arduino.bit_set(x, n)    -> Int64   # x | (1 << n)
arduino.bit_clear(x, n)  -> Int64   # x & ~(1 << n)
arduino.bit_toggle(x, n) -> Int64   # x ^ (1 << n)
arduino.bit_read(x, n)   -> Int64   # (x >> n) & 1
```

`n` is not bounds-checked. `n >= 64` is undefined behaviour (it happens
to work on AVR because the shift amount is masked to 6 bits, but do not
rely on it).

```vyne
arduino.low_byte(x)      -> Int64   # x & 0xFF
arduino.high_byte(x)     -> Int64   # (x >> 8) & 0xFF
arduino.make_word(hi, lo)-> Int64   # ((hi & 0xFF) << 8) | (lo & 0xFF)
```

Byte extraction and assembly. Useful for 16-bit payloads over I2C/SPI,
register manipulation, and EEPROM serialization.

```vyne
arduino.popcount(x) -> Int64
```

Number of set bits. Uses Kernighan's algorithm — one loop iteration per
set bit. For dense data (few zero bits) a 256-byte lookup table is
faster, but it costs flash and requires a PROGMEM read; for the small
bitmaps typical of a sketch, Kernighan is the right default.

```vyne
arduino.crc8(data, seed) -> Int64
```

Dallas/Maxim CRC-8 (polynomial `0x31`, MSB-first, initial value
typically `0`). One byte per call so the caller can chain:

```vyne
crc :: Int64 := 0;
through i :: 0..n-1 -> loop {
    crc = arduino.crc8(frame[i], crc);
};
```

The implementation uses an 8-iteration loop rather than a 256-byte
lookup table. On AVR, for a frame of a few dozen bytes, the loop is
faster than table lookup because it avoids the PROGMEM read overhead.

---

### EEPROM — `mcu_eeprom.h`

One kilobyte of persistent storage, byte-addressable, ~100,000 write
cycles per byte. Access is via `<avr/eeprom.h>`'s `eeprom_read_byte` /
`eeprom_update_byte`, not the bundled Arduino `EEPROM` library.

```vyne
arduino.eeprom_read_int(addr :: Int64) -> Int64
arduino.eeprom_write_int(addr :: Int64, val :: Int64)
```

Read or write 8 bytes (`int64_t`) starting at `addr`. The `write`
variant uses `update_byte`, which only writes when the new byte differs
from the current — extending cell life. If you write the same value
repeatedly, the EEPROM is not touched.

```vyne
arduino.eeprom_read_float(addr :: Int64) -> Float64
arduino.eeprom_write_float(addr :: Int64, val :: Float64)
```

Read or write 4 bytes (`double`, which is 32-bit single-precision on
AVR). Same update-only-if-changed semantics.

Address range is `0..1023`. **Bounds are not checked.** A write past
1023 wraps to address 0 (the address register is 10 bits wide), so a
runaway loop can clobber the entire EEPROM. The correct pattern is to
reserve a small block for your sketch and hardcode the addresses.

Integer and float slots must not overlap. A sketch that stores an `Int64`
at address 0 and a `Float64` at address 4 will work; a sketch that stores
an `Int64` at 0 and a `Float64` at 2 will corrupt both. Plan the layout
in a comment at the top of the sketch.

---

### I2C — `mcu_i2c.h`

I2C/TWI master bindings backed by the bundled Arduino `Wire` library.
On the Uno R3, SDA is A4 and SCL is A5. The build integration compiles
`vendor/arduino/libraries/Wire/src` (including `utility/twi.c`) and adds
it to the include path.

| Binding | Returns | Notes |
| :--- | :--- | :--- |
| `arduino.i2c_begin()` | `Null` | Start the bus master |
| `arduino.i2c_end()` | `Null` | Release the peripheral |
| `arduino.i2c_set_clock(hz)` | `Null` | `0` keeps the 100 kHz default |
| `arduino.i2c_write(addr, value)` | `Int64` | `endTransmission` status |
| `arduino.i2c_read(addr)` | `Int64` | One byte, or `-1` |
| `arduino.i2c_write_reg(addr, reg, value)` | `Int64` | Register write |
| `arduino.i2c_read_reg(addr, reg)` | `Int64` | Repeated-start read, or `-1` |
| `arduino.i2c_available()` | `Int64` | Bytes in the receive buffer |

Addresses are 7-bit (0..127). The C layer casts to `uint8_t`; the typed
`vi2c` wrapper validates first and returns `-2` / `-3` for bad
arguments instead of truncating. `endTransmission` status values are
Arduino's: `0` ACK, `1` data too long, `2` NACK address, `3` NACK data,
`4` other.

```vyne
use native arduino;
use external "vi2c/vi2c.vy";

vi2c.begin();
id :: Int64 := vi2c.read_reg(0x68, 0x75);   # MPU-6050 WHO_AM_I
vi2c.end();
```

See `examples/arduino/i2c_imu.vy` and `vyne/modules/external/vi2c/README.md`.

### SPI — `mcu_spi.h`

SPI master bindings backed by the bundled Arduino `SPI` library. On the
Uno R3, MOSI is D11, MISO D12, SCK D13, and SS D10 (drive SS yourself).
The build integration compiles `vendor/arduino/libraries/SPI/src` and adds
it to the include path.

| Binding | Returns | Notes |
| :--- | :--- | :--- |
| `arduino.spi_begin()` | `Null` | Power the peripheral |
| `arduino.spi_end()` | `Null` | Release it |
| `arduino.spi_set(clock_hz, mode)` | `Null` | Begins a transaction; MSB first; `mode` 0..3 |
| `arduino.spi_end_transaction()` | `Null` | Ends the transaction |
| `arduino.spi_transfer(value)` | `Int64` | 8-bit full-duplex |
| `arduino.spi_transfer16(value)` | `Int64` | 16-bit full-duplex |

`arduino.spi_set` falls back to mode 0 for an out-of-range mode and to
4 MHz for a zero clock, so a bad constant cannot hang the bus. The typed
`vspi` wrapper validates clock, mode, and value widths and returns `-2`
/ `-3` instead.

```vyne
use native arduino;
use external "vspi/vspi.vy";

vspi.begin();
vspi.set(1000000, 0);
id :: Int64 := vspi.transfer(0x9F);   # JEDEC ID
vspi.end_transaction();
vspi.end();
```

See `examples/arduino/spi_loopback.vy` and `vyne/modules/external/vspi/README.md`.

### Interrupts — `mcu_interrupts.h`

The Uno R3 has two external-interrupt pins: D2 (IRQ 0) and D3 (IRQ 1).
`digitalPinToInterrupt()` returns `-1` for every other pin. The runtime
provides a **counter-based** interface, not a callback interface — see
[What is not bound and why](#what-is-not-bound-and-why) for the reasoning.

```vyne
arduino.attach_interrupt(pin :: Int64, mode :: Int64) -> Int64
```

Install an edge-counting trampoline on `pin`. `mode` selects the edge
type:

| `mode` | Trigger |
| :---: | :--- |
| `0` | LOW |
| `1` | CHANGE (both edges) |
| `2` | RISING |
| `3` | FALLING |

Returns the IRQ index (`0` for D2, `1` for D3) on success, `-1` if the
pin has no external interrupt. The counter is reset to zero on attach.

```vyne
arduino.detach_interrupt(pin :: Int64)
```

Remove the trampoline. Subsequent edges do nothing. Returns `null`.

```vyne
arduino.interrupt_count(pin :: Int64) -> Int64
```

Read the current edge count. Returns `0` for non-interrupt pins.
Interrupts are masked during the read so the 64-bit value is coherent.

```vyne
arduino.interrupt_reset(pin :: Int64)
```

Reset the counter to zero. Returns `null`.

```vyne
arduino.interrupt_fired(pin :: Int64) -> Bool
```

`true` if the counter is greater than zero. Equivalent to
`interrupt_count(pin) > 0` but marginally cheaper. Interrupts are
masked during the read.

The intended pattern:

```vyne
arduino.attach_interrupt(2, 2);   # D2, RISING

through tick :: 0..1000000-1 -> loop {
    if arduino.interrupt_fired(2) {
        arduino.interrupt_reset(2);
        arduino.digital_write(13, 1);
        arduino.delay(50);
        arduino.digital_write(13, 0);
    }
};
```

---

### Watchdog — `mcu_interrupts.h`

The AVR watchdog resets the board if it is not petted within the
configured interval. The standard tool for autonomous firmware that
must recover from a software lockup.

```vyne
arduino.watchdog_enable(timeout_ms :: Int64)
```

Enable the watchdog. The AVR hardware supports a fixed set of intervals;
`timeout_ms` is snapped up to the nearest supported value:

| Requested | Actual |
| :--- | :--- |
| ≤ 15 | 15 ms |
| ≤ 30 | 30 ms |
| ≤ 60 | 60 ms |
| ≤ 120 | 120 ms |
| ≤ 250 | 250 ms |
| ≤ 500 | 500 ms |
| ≤ 1000 | 1 s |
| ≤ 2000 | 2 s |
| ≤ 4000 | 4 s |
| ≤ 8000 | 8 s |

The watchdog is disabled by the Arduino core before `setup()` runs, so
a firmware must call `watchdog_enable` explicitly.

```vyne
arduino.watchdog_reset() -> null
```

Pet the watchdog. Every call resets the timer. The standard pattern is
to call this once per successful iteration of the main loop — if the
loop locks up, the watchdog fires and the board resets.

```vyne
arduino.watchdog_disable() -> null
```

Disable the watchdog. Useful for entering a critical section where you
explicitly do not want a reset, or for a firmware that has decided its
own recovery is better than a hard reset.

**Do not call `watchdog_disable` and forget to re-enable.** A watchdog
enabled at boot and then disabled without reason leaves the firmware
without the safety net it was designed to have.

---

### Typed arrays — `mcu_types.h`

Typed-array containers are the memory-efficient representation for
numerical arrays on AVR. See [Memory model](#memory-model) for the
layout. The emitter uses these directly from Vyne source via:

```vyne
region buffer {
    a :: Array<Float64> := [];
    a.push(1.0);
    a.push(2.0);
    # a.size() == 2
    # a[0] == 1.0
};
```

Operations supported from Vyne source:

| Operation | Emitter lowering | Notes |
| :--- | :--- | :--- |
| `Array<Float64>()` / `[]` | `vyne_array_f64_create(0)` | Empty container |
| `[1.0, 2.0, 3.0]` | `vyne_array_f64_create(3)` + direct writes | Fixed-size literal |
| `arr.push(x)` | `vyne_array_f64_push(&arr, x)` | Grows by 1.5× |
| `arr.size()` | `arr.size` (field read) | O(1) |
| `arr[i]` | `arr.data[i]` (direct load) | No bounds check |
| `arr[lo:hi]` | `vyne_array_f64_slice(&arr, lo, hi)` | `hi` inclusive |

Internal C functions defined in `mcu_types.h` for the emitter's use:

```c
VyneArray_f64 vyne_array_f64_create(int64_t n);
VyneArray_i64 vyne_array_i64_create(int64_t n);

void vyne_array_f64_push(VyneArray_f64* arr, double v);
void vyne_array_i64_push(VyneArray_i64* arr, int64_t v);

VyneValue vyne_array_f64_to_value(VyneArray_f64* arr);
VyneValue vyne_array_i64_to_value(VyneArray_i64* arr);
VyneArray_f64 vyne_value_to_array_f64(VyneValue v);
VyneArray_i64 vyne_value_to_array_i64(VyneValue v);

VyneArray_f64 vyne_array_f64_slice(VyneArray_f64* src, int64_t lo, int64_t hi);
VyneArray_i64 vyne_array_i64_slice(VyneArray_i64* src, int64_t lo, int64_t hi);
```

The untyped `VyneArray` fallback exists for `x := [];` (which infers no
element type) and for mixed-type literals. Its cost is 16 bytes per
element. **Annotate the binding with `Array<Float64>` or
`Array<Int64>` whenever the elements are all one type.**

---

### Arena — `mcu_arena.h`

The arena header is not part of the Vyne-facing API. It is documented
here for developers who are adding new bindings or debugging an
allocation failure.

```c
void*  arena_alloc(size_t size);
void*  arena_alloc_aligned(size_t size, size_t align);
void   arena_free_all(void);
size_t arena_used(void);

ArenaCheckpoint arena_checkpoint(void);
void            arena_rewind(ArenaCheckpoint cp);
```

`arena_alloc_aligned` currently ignores `align` and calls `arena_alloc`.
The default allocator already returns 8-byte-aligned pointers, so any
`align <= 8` is satisfied. A `align > 8` request would need a real
implementation if it is ever used.

`arena_free_all` resets `g_mcu_cur`, `g_mcu_peak`, and `g_mcu_cp_top`.
It is called once by `vyne_arduino.h` before `vyne_setup()` and can be
called from Vyne source via `vmem.reset()` if the binding exists on AVR
(it does not currently).

---

## Common patterns

### Bounding memory with a region

The single most important pattern on AVR. Any code that allocates must
be wrapped in a `region` block, or the arena will fill and the board
will halt.

```vyne
use native arduino;

through iteration :: 1..1000000 -> loop {
    region step {
        # Every allocation in here is reclaimed at the closing brace.
        buf :: Array<Float64> := [];
        through i :: 0..15 -> loop { buf.push(0.0); };
        # ... use buf ...
    };
    # buf is out of scope here; the 64 bytes it held are back in the arena.
};
```

If the region body allocates but the region never closes (e.g., inside
an infinite loop), the arena is not reclaimed. The pattern is
specifically per-iteration scope, not per-program scope.

### Verifying the rewind fired

```vyne
before :: Int64 := arduino.arena_used();

region probe {
    scratch buf :: Float64[16];
    through i :: 0..15 -> loop { buf[i] = arduino.sin(0.1 * i); };
};

after  :: Int64 := arduino.arena_used();
drift  :: Int64 := after - before;

arduino.serial_print("drift = ");
arduino.serial_print(drift);
arduino.serial_print(" B\n");
```

`drift` should be `0`. If it is not, the region's checkpoint and rewind
did not balance — a compiler bug, not a runtime bug. Report it with the
source that triggered it.

### Serial output

```vyne
arduino.serial_begin(115200);

arduino.serial_print("value = ");
arduino.serial_print(arduino.analog_read(0));
arduino.serial_print("\n");
```

Note: `out(...)` is not available on AVR — the boxed print engine that
backs it is not linked. Use `arduino.serial_print` and
`arduino.serial_println`.

The newline is part of the string, not part of `serial_print`. This
matches Arduino convention. If you want a trailing newline, call
`arduino.serial_println(v)`.

### Scratch vs typed array

**Use `scratch` when:**

- the buffer size is fixed at compile time,
- the buffer is small (under ~200 bytes),
- the buffer is used and discarded within one iteration,
- the buffer is a plain numerical array (`Float64[N]` or `Int64[N]`),
- the buffer is entirely local to a region body.

**Use `Array<Float64>` when:**

- the buffer needs to grow (dynamic `push`),
- the buffer outlives the enclosing block,
- the buffer needs to be passed to a function that takes `Array<Float64>`,
- the buffer size is only known at runtime.

The cost of `scratch` is stack. The cost of `Array<Float64>` is arena
plus one allocation per growth step. On AVR, prefer scratch.

### Boxed vs unboxed paths

The emitter's native fast paths fire when every operand of an expression
is provably typed. The classic case where boxing appears without
warning:

```vyne
x := arduino.analog_read(0);      # x is boxed (Int64 from a function call)
y :: Float64 := x * 0.1;          # boxing read, conversion, then store

# Better:
x :: Int64 := arduino.analog_read(0);    # native Int64
y :: Float64 := x * 0.1;                 # native arithmetic
```

The difference is a few dozen cycles per operation, but in a tight loop
that runs millions of times it matters. When a sketch is unexpectedly
slow, look for `:=` on a value produced by a function call and add a
`:: Type` annotation.

---

## Extending the runtime

Adding a new `arduino.*` binding requires four changes:

1. **Define the C function** in one of `detail/mcu_*.h`. Use the
   `arduino_<name>` naming convention. Every parameter and return value
   is `VyneValue` unless a `_f64` unboxed variant is also being added
   (in which case the unboxed form takes plain `double` and returns
   `double`). The function must be `static inline`.
2. **Register the mapping** in
   `compiler/codegen/native_maps.h`, in the `ARDUINO_MAP` table. The
   `vyneName` field is the source-level name (`"pin_mode"`), `cName` is
   the C symbol (`"arduino_pin_mode"`), `isProperty` is `true` for
   zero-argument getters, `usesArgv` is `true` for variadic functions,
   `nativeF64` is the optional unboxed variant name.
3. **Add the header** to `vyne_arduino.h`'s include list if the binding
   lives in a new header file. If it lives in an existing header, no
   change is needed here.
4. **Add a test** to `examples/arduino/` that exercises the new binding
   on real hardware. The test should verify the return value against a
   known-good reference; a compile-only test is not enough.

If the binding needs a bundled Arduino library (Wire, SPI, etc.), the
build integration already compiles and links it — see the next section
for the header guard and wrapper conventions.

### Adding a bundled-library binding

The AVR build integration now scans `vendor/arduino/libraries/*/src`
(cli/file_handler.cpp::buildArduino). For every library with a `src/`
directory it appends `-I<lib>/src` to the include path. Compilation is
demand-driven: a library is compiled — recursively, including nested
utility directories such as `Wire/src/utility/twi.c` — and linked only
when the generated translation unit defines its `VYNE_USE_<bus>` macro.
Unused libraries are never built, so their global constructors cannot
add flash to a sketch that does not use them.

So a new bundled-library binding no longer needs build-integration work.
The steps are:

1. **Define the C wrappers** in a new `detail/mcu_<bus>.h`. Include
   `mcu_types.h` first, then the bundled header (`<Wire.h>`, `<SPI.h>`,
   `<SoftwareSerial.h>`, ...). Every parameter and return value is
   `VyneValue` unless a `_f64` unboxed variant is also added.
2. **Register the mapping** in `ARDUINO_MAP` (`native_maps.h`).
3. **Guard the include** in `vyne_arduino.h` with a `VYNE_USE_<BUS>`
   macro, and set that macro from the emitter when the corresponding
   `arduino.<bus>_*` member is lowered (see `noteArduinoI2cUse` /
   `noteArduinoSpiUse` in `emitter.h` and the dispatcher in
   `method_calls.cpp`). This keeps unused peripherals out of the
   generated translation unit and avoids dragging in their global
   constructors.
4. **Add a typed wrapper** under `vyne/modules/external/v<bus>/` if the
   raw `VyneValue` API is too loose for users (address/range validation,
   named error codes).
5. **Add a hardware test** under `examples/arduino/`.

`mcu_i2c.h` and `mcu_spi.h` follow exactly this pattern. The alternative
still exists for libraries that are pure avr-libc wrappers — reimplement
directly (`mcu_eeprom.h` is the reference) and skip the build integration
and the macro guard entirely.

---

## What is not bound and why

The runtime is a strict subset. The missing bindings are missing for
specific reasons; documenting them here saves the next developer from
re-deriving the reasoning.

### `dot`, `norm`, `argmax`, `argmin`

Not in `ARDUINO_MAP`. Adding them requires:

- `VyneArray_f64` / `VyneArray_i64` containers in `mcu_types.h`
  (**done** — this is now available),
- the corresponding entries in `ARDUINO_MAP`,
- a `vlin`-side or `arduino`-side `.vy` wrapper for the user-facing
  signature.

The kernel side (`vlin.k_dot`, `vlin.k_sum`, etc.) is already
native-callable — the signature is `(Array<Float64>, Int64) -> Float64`
or `(Array<Float64>, Array<Float64>, Int64) -> Float64`, which is the
native shape. What is missing is the `arduino.*` entry point. A future
`arduino.dot(a, b, n)` binding would be a five-line addition to
`mcu_pins_ext.h` and a one-line addition to `ARDUINO_MAP`.

Meanwhile, the user-space `vlin.k_dot` **should** work on AVR if called
with typed array arguments. Not yet tested on hardware; the first
`k_dot` call from an MCU sketch is the next validation point.

### `attach_interrupt` callback semantics

The current binding installs a counter, not a callback. A Vyne `fn`
compiles to `VyneValue fn_X(int argc, VyneValue* argv)`; AVR ISRs are
`void(void)`. Installing a Vyne function as an ISR would require the
code generator to emit a per-function ISR trampoline — a shim like

```c
static void fn_X_isr(void) {
    fn_X(0, NULL);
}
ISR(INT0_vect) { fn_X_isr(); }
```

plus the pointer bookkeeping to bind the right trampoline to the right
ISR at runtime. That is a codegen feature, not a runtime feature, and
is out of scope for the current slice of work.

The counter-based interface is the correct tool for the common cases:
tachometer, flow meter, edge counter, wake-on-signal. It is not
sufficient for the rarer case of "run this Vyne function inside the
ISR", which nobody should be doing on an AVR anyway (ISRs should be
short, and Vyne functions are not guaranteed to be).

### SoftwareSerial

Not bound. `SoftwareSerial` ships a `.cpp` source and is now compiled by
the build integration, but no Vyne bindings are registered for it yet.
A future `mcu_softserial.h` would follow the same five-step pattern as
`mcu_i2c.h` / `mcu_spi.h` and add its own `VYNE_USE_SOFTSERIAL` guard.

I2C (`Wire`) and SPI are now bound — see the [I2C](#i2c--mcu_i2ch) and
[SPI](#spi--mcu_spih) API sections.

### `out()` (the boxed print engine)

Not linked on AVR. `out(x)` goes through a formatting engine that
handles `Int64`, `Float64`, `String`, `Bool`, `Null`, `Array`, `Map`,
and `Struct`. The runtime only needs the first five for serial output,
and `arduino.serial_print` provides them. Linking the full engine would
add about 1 KB of flash for a function that has no better syntax than
`arduino.serial_print`.

Use `arduino.serial_print` and `arduino.serial_println`.

### `free(x)`

A no-op on AVR. The arena model has no free; the region rewind is the
free. Calling `free(x)` from Vyne source compiles but does nothing.
This matches the desktop arena backend.

### Timer interrupts (compare-match ISR)

Not bound. Timer0 is reserved for `millis`/`micros` and `delay`.
Timer1 and Timer2 are available but configuring them requires
direct avr-libc register access, which is not exposed through Vyne.
A sketch that needs a timer interrupt has to write it in C++ and
include the resulting symbol manually — at which point it is no longer
a Vyne program in the pure sense.

The `arduino.timer_read()` binding exposes Timer1's raw counter for
sketches that already configure the timer via a co-compiled `.cpp` file.
That is a niche escape hatch, not a general feature.

---

## Testing

The runtime is testable on the desktop because every header compiles
standalone with `<Arduino.h>` replaced by a stub. The test harness
under `tests/arduino/` does exactly this: it includes each `mcu_*.h`
with a set of stubs for `Serial`, `digitalWrite`, etc., and asserts
the return values of the boxed forms against known-good inputs.

The parts that cannot be tested on the desktop:

- Anything that touches hardware registers (`analogRead`,
  `analogWrite`, timers).
- Anything whose behaviour depends on the AVR's 32-bit `double` (all
  arithmetic that would be 53-bit-precise on desktop).
- Anything that depends on the exact stack depth at interrupt entry.

For those, the verification is: **build the firmware, flash it, run
it, verify the output on the serial monitor.** The TinyML XOR example
is the reference for what a full firmware test looks like: it prints
a self-test of every math function, verifies the memory model with a
region probe, and then runs 30,000 SGD steps with the arena readout
printed at every reporting interval.

The next time you add a binding, add a line to the TinyML demo's
self-test section or write a small `examples/arduino/<feature>.vy`
that exercises it. Flash it. Watch the output. Do not trust a
compile-only test.

---

## Limitations

Documented constraints that are unlikely to change without a larger
milestone:

**AVR target is Arduino Uno R3 only.** `--arduino` accepts only `uno`.
Nano and Mega share the core runtime but their pin maps, memory sizes,
and interrupt counts differ. The `VYNE_MCU_ARENA_SIZE` macro is the
only tunable.

**Sixteen nested regions maximum.** `g_mcu_cp_stack[16]` is a fixed
array. A program with more than sixteen nested `region` blocks
overwrites the stack silently. The `RegionNode::compile` path does not
check this on AVR.

**No `VyneArray` bounds checking.** Element access via `arr[i]` lowers
to `arr.data[i]` with no runtime check, matching desktop behaviour.
Out-of-bounds reads return garbage; out-of-bounds writes corrupt
adjacent memory. `scratch` arrays have bounds checking; typed arrays
do not. The trade-off is deliberate — the check costs flash per read
and would be elided in release builds anyway.

**Array growth allocates in the arena; there is no free.** The
standard pattern is `region { ... }` around the array. Repeated push
without a region will exhaust the 1 KB arena and halt the board.

**`double` is 32-bit.** AVR's `double` type is a single-precision
float. Values that depend on 53-bit precision will produce different
results on the Uno than on the desktop. See
[Design constraints](#design-constraints).

**`int64_t` is emulated.** Every `int64_t` arithmetic operation
compiles to a 32-bit libgcc helper call. Prefer `Float64` for
numerical work where 24-bit precision suffices.

**No integer overflow checking.** Arithmetic wraps silently on both
desktop and AVR.

**`out()` is not available on AVR.** Use `arduino.serial_print` and
`arduino.serial_println`.

**String interning is bounded.** `vyne_string_static(s)` returns a
pointer into `.rodata` (no allocation). `vyne_string(s)` copies into
the arena (allocation). The two cannot be mixed freely: a static string
cannot be invalidated by a rewind; a dynamic one can.

**`attach_interrupt` is counter-based.** No callbacks. See
[What is not bound and why](#what-is-not-bound-and-why).

**I2C and SPI are bound; SoftwareSerial is not.** See
[What is not bound and why](#what-is-not-bound-and-why). Both peripheral
headers are opt-in and compile only when used.

**I2C/SPI block transfers are not bound yet.** Only single-byte and
16-bit word transfers are exposed. Multi-byte transfers need a buffer
ABI on the MCU runtime.

**`free_ram` and `stack_free` return the same value.** Without
stack-painting instrumentation there is no separate stack watermark.
Both expose the avr-libc `__brkval` / `__heap_start` computation.

---

## Contributing

Adding a binding, extending a header, or fixing a runtime bug follows
the same process as the rest of the Vyne repository. Before opening
a pull request:

1. Follow the naming conventions in
   [Naming conventions](#naming-conventions). Every new binding is a
   `static inline` function named `arduino_<name>`. If the
   argument/return type could be `Float64`, add a `_f64` unboxed
   variant alongside the boxed one.
2. Add the binding to `compiler/codegen/native_maps.h`'s `ARDUINO_MAP`
   table. The table is what the code generator consults; a binding
   that is not in the table is dead code.
3. Add the header to `vyne_arduino.h`'s include list if it is a new
   file. Headers under `detail/` are included in dependency order;
   preserve the ordering (arena, types, then everything else).
4. Add a test sketch under `examples/arduino/`. The test must run on
   real hardware. A compile-only test that verifies the firmware
   flashes is not sufficient — flash it, monitor the serial output,
   verify the value.
5. Update this README's [API reference](#api-reference) section with
   the new function's signature, parameters, return value, and any
   caveats.

For runtime bug fixes:

- The bug is in the Vyne runtime, not the Arduino core. Verify that
  the same behaviour does not occur in a pure-Arduino sketch before
  assuming it is a Vyne bug.
- A runtime bug means the emitted C is valid but produces the wrong
  result. A compiler bug means the emitted C is invalid or does not
  compile. These are different classes of problem; the fix belongs
  in different directories.
- Attach a minimal `.vy` file that reproduces the bug, and the
  serial output that a correct run should produce.

See the root `CONTRIBUTING.md` for the full guide.

---

## License

The AVR runtime is MIT-licensed, matching the desktop runtime. The
compiler that emits the C code is AGPL-3.0-only. See
[LICENSE-MIT](../../LICENSE-MIT) and [LICENSE](../../LICENSE) at the
repository root for the full text.

The runtime does not include any code from the bundled Arduino
libraries (`arduino/libraries/*`), the Arduino core
(`arduino/cores/arduino/*`), or avr-libc. It calls into those libraries
via the standard toolchain include path. Redistribution of a
Vyne-generated firmware is therefore subject to the same licensing
considerations as any Arduino sketch: the Arduino core is LGPL, the
AVR toolchain is GPL with a runtime exception, and Vyne's own runtime
is MIT.

---

## Acknowledgments

The runtime's design is informed by the constraints the Arduino core
places on any library that wants to remain small, and by the avr-libc
project's disciplined use of `static inline` and PROGMEM. The
`<avr/eeprom.h>` approach for EEPROM access was chosen specifically
because it lets the runtime compile without pulling in the bundled
Arduino EEPROM library — a pattern we recommend to anyone extending
this directory.

Special thanks to every developer who has filed an issue about a
runtime binding, especially the ones where the answer was "the AVR
core's `Print` class participates in unqualified lookup in a way that
makes `Serial.print(v)` ambiguous without `::Serial`". That is why
every print call in this directory uses the fully-qualified form.
