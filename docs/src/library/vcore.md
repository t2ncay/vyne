---
id: vcore
title: VCore
group: Standard Library
icon: fa-microchip
order: 2
description: Time, sleep, platform, input, string building, process information, and realtime telemetry.
---

# VCore

`vcore` is the environment and utility module. Import it with `use native vcore;`.

## Time and sleep

| Function | Result | Notes |
| :--- | :--- | :--- |
| `vcore.now()` | `String` | Local time formatted `YYYY-MM-DD HH:MM:SS`. |
| `vcore.now_ns()` | `Int64` | Monotonic nanoseconds. Origin is arbitrary; use differences for durations. |
| `vcore.sleep(ms)` | `Int64` | Blocks for `ms` milliseconds; returns `1`. |

`now_ns` is the timing substrate for `vynec bench` and the library benchmarks.

## Environment

| Function | Result | Notes |
| :--- | :--- | :--- |
| `vcore.platform()` | `String` | `"Windows x64"`, `"Linux"`, `"Mac OSX"`, ... |
| `vcore.input(prompt?)` | `String` / `null` | Reads one line from stdin, optionally printing a prompt. |
| `vcore.version` | `String` | Compiler/runtime version. |
| `vcore.engine` | `String` | Engine identifier. |
| `vcore.build` | `String` | Build descriptor. |
| `vcore.processor_count` | `Int64` | Logical processor count. |
| `vcore.pid` | `Int64` | Process id. |
| `vcore.memory_usage` | `Int64` | Process memory usage in bytes (platform-dependent). |

```vyne
use native vcore;

out("platform: " + vcore.platform());
out("now: " + vcore.now());

t0 :: Int64 := vcore.now_ns();
heavy_work();
out("ms: " + string(float64(vcore.now_ns() - t0) / 1_000_000.0));
```

## Parsing helpers

| Function | Result | Notes |
| :--- | :--- | :--- |
| `vcore.parse_array(text)` | `Array` | Parses a nested bracket/number representation such as `"[1, 2, [3, 4]]"`. |
| `vcore.hex_to_int64(text)` | `Int64` | Parses a hexadecimal string, with or without the `0x` prefix. |
| `vcore.chr(code)` | `String` | One-byte string from a code point masked to `0..255`. |

```vyne
arr := vcore.parse_array("[1, 2, [3, 4]]");
mask := vcore.hex_to_int64("0xFF2D809A");
letter := vcore.chr(65);        # "A"
```

`chr` is intended for assembling multi-byte UTF-8 sequences from computed bytes.

## String builder

The string builder avoids repeated reallocation on append-heavy output.

| Function | Result | Notes |
| :--- | :--- | :--- |
| `vcore.sb_create()` | `Int64` | Creates a builder and returns its handle. |
| `vcore.sb_append(handle, value)` | `null` | Appends any value, stringified. |
| `vcore.sb_append_char(handle, code)` | `null` | Appends one byte without allocating a one-character string. |
| `vcore.sb_build(handle)` | `String` | Returns an independent copy of the contents. |
| `vcore.sb_reset(handle)` | `null` | Rewinds size to 0, keeps capacity. |

```vyne
sb :: Int64 := vcore.sb_create();
through i :: 0..9 -> loop {
    vcore.sb_append(sb, i);
    vcore.sb_append_char(sb, 44);    # ','
};
out(vcore.sb_build(sb));
```

`sb_append_char` is the substrate for byte-oriented parsers such as `vcsv`.

## Realtime telemetry

`@realtime<ms>` regions maintain a small stats table. Read it with:

| Function | Result |
| :--- | :--- |
| `vcore.realtime_worst_ms()` | Worst observed elapsed milliseconds across all realtime regions. |
| `vcore.realtime_overruns()` | Total overrun count. |
| `vcore.realtime_reset()` | Clear the aggregate counters. |
| `vcore.realtime_worst_ms_named(name)` | Worst observed time for one named region. |
| `vcore.realtime_overruns_named(name)` | Overrun count for one named region. |

```vyne
@realtime<5> region control_tick {
    controller.step(reading);
};
out("worst: " + string(vcore.realtime_worst_ms()));
out("overruns: " + string(vcore.realtime_overruns()));
```

The AVR runtime exposes the same five calls through `arduino.realtime_*`.

## AVR note

On the Arduino target only time and sleep are meaningful. `now`, `now_ns`, and `sleep` are registered; filesystem, process, and input APIs are desktop-only.

## See also

- [VMem](page:vmem)
- [Memory & Regions](page:regions)
- [Built-ins](page:builtins)
