---
id: arduino
title: Arduino & AVR
group: Language Reference
icon: fa-microchip
order: 15
description: The Uno R3 target, the 1 KB arena, scratch on the AVR stack, and the complete arduino.* binding surface.
---

# Arduino & AVR

The Arduino Uno R3 (ATmega328P) is Vyne's first non-desktop target. The same `.vy` source that runs under the interpreter or compiles to a desktop binary can be lowered to firmware:

```shell
vynec --arduino uno sketch.vy
vynec --arduino uno --flash --monitor --port COM3 sketch.vy
```

The target has a **1 KB static arena**, a **16-deep checkpoint stack**, and a hard halt on arena exhaustion. Region semantics do not change: the arena is a bump allocator with checkpoint / rewind, exactly like the desktop runtime, only smaller.

## A first sketch

For a new project, select the board once and use the project workflow:

```shell
vynec init blink --name blink --kind embedded --board arduino-uno --port COM5
cd blink
vynec hardware setup
vynec build
vynec run
```

`hardware.toml` stores the board and port. Existing Uno pinout and budget
fields remain accepted. `vynec run --monitor` explicitly enables monitoring.
ESP32 and HW-950 initialization use the same short commands; see
[Projects & Tooling](page:projects) for presets, overrides and setup.

```vyne title="blink.vy"
use native arduino;

arduino.serial_begin(115200);

through i :: 0..9 -> loop {
    arduino.digital_write(13, 1);
    arduino.delay(100);
    arduino.digital_write(13, 0);
    arduino.delay(100);
};

@bounded<512>
region probe {
    scratch buf :: Float64[16];
    through i :: 0..15 -> loop {
        buf[i] = arduino.sin(0.1 * i);
    };
    arduino.serial_println(buf.mean());
};
```

`scratch` lowers to a raw array on the AVR stack, not the arena. On AVR, `Float64` is a 32-bit float and `Int64` is emulated over 32-bit hardware; prefer `Float64` for numerical work.

## Quick reference

| Header | Functions | Surface |
| :--- | :---: | :--- |
| `mcu_hardware.h` | 14 | Pin mode, digital/analog I/O, delays, base trig, `arena_used` |
| `mcu_serial.h` | 7 | Available, read, read_line, parse, println, flush |
| `mcu_timing.h` | 2 | Pulse width, Timer1 read |
| `mcu_math.h` | 34 | Transcendentals (boxed + unboxed `_f64`), constrain, map, lerp, smoothstep, random |
| `mcu_pins_ext.h` | 9 | ADC voltage/reference/resolution, oversampling, toggle, pulse, shift, count |
| `mcu_memory.h` | 4 | Arena peak/free, free RAM, stack free |
| `mcu_bits.h` | 9 | Bit set/clear/toggle/read, byte split/merge, popcount, CRC-8 |
| `mcu_eeprom.h` | 4 | Read/write `Int64` and `Float64` across resets |
| `mcu_i2c.h` | 8 | I2C master via `Wire`: byte/register read-write, clock |
| `mcu_spi.h` | 6 | SPI master via `SPI`: settings, byte/word transfer |
| `mcu_realtime.h` | 5 | `@realtime` watchdog and telemetry (opt-in) |
| `mcu_interrupts.h` | 8 | Counter-based external interrupts (D2/D3) + watchdog |

Total exposed to Vyne source: **110 functions** (91 core + 14 peripheral + 5 realtime telemetry).

## The memory model on AVR

- **Arena.** A `static uint8_t g_mcu_arena[VYNE_MCU_ARENA_SIZE]` in `.bss`, 1024 bytes by default. `arena_alloc` is a pointer bump plus a bounds check. Exhaustion halts with `while (1) ;`.
- **Checkpoints.** Sixteen fixed slots. Nested regions deeper than 16 are not supported.
- **Scratch.** C stack storage. Roughly 1.5 KB of stack headroom remains after frame pointers and ISR return addresses. Use scratch for small fixed buffers.
- **Typed arrays.** `Array<Float64>` costs 4 bytes per element plus a 12-byte header; an untyped `Array` costs 16 bytes per element plus the same header. Use the annotated form whenever possible.
- **Push grows, rewind reclaims.** `push` allocates a fresh buffer (1.5×, minimum 8 elements) and copies. Repeating push without a region exhausts the arena.
- **Strings.** String literals do not allocate; `vyne_string` copies into the arena and can trigger the halt path if full.
- **No `out()`.** The boxed print engine is not linked. Use `arduino.serial_print` / `serial_println`.

## `@bounded` on AVR

The MCU arena maintains the same live-allocation counter as the desktop arena. `@bounded<bytes>` checks it at region exit, and `vyne_panic_region_bound` halts the sketch on a violation. The embedded project template exercises this path directly.

```vyne
@bounded<512>
region sensor_frame {
    scratch samples :: Float64[32];
    through i :: 0..31 -> loop {
        samples[i] = arduino.analog_read(0);
    };
    arduino.serial_println(samples.mean());
};
```

## What is not bound

- `dot`, `norm`, `argmax`, `argmin` are absent from the `ARDUINO_MAP` table. They would require typed-array containers in `mcu_types.h` plus `boxAny` lowering for MCU targets. Calling them from a sketch fails at compile time with "not defined".
- `attach_interrupt` is **counter-based**, not callback-based. A Vyne function compiles to the boxed signature, which cannot be installed as an AVR `void(void)` ISR without compiler-side wrapper emission. Poll `interrupt_count`, `interrupt_fired`, and `interrupt_reset`.
- Timer compare-match ISRs are not exposed.
- The MCU typed-array method surface is partial: `contains`, `pop`, `reverse`, `sort`, `place_all`, `delete_at`, `back`, and `pop_front` are not bound on AVR.
- `vlin.multiply` through the `Ops.vy` wrapper is still boxed on MCU even though the kernel is unboxed; direct `vlin.k_*` calls from a sketch have not been hardware-tested.
- No filesystem (`vfs` is desktop-only), no `@device`, no `@persistent`, no `@streamed`.

## Complete API reference

The section below is generated from the AVR runtime manual so the binding surface stays aligned with the headers. Every function is reachable through `use native arduino;`. C symbols are shown for reference only; call the Vyne names.

<!-- ARDUINO_API -->

## See also

- [Memory & Regions](page:regions)
- [vi2c](page:vi2c) and [vspi](page:vspi) for typed bus wrappers
- [Projects & Tooling](page:projects) for the `embedded` template
