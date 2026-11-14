---
id: projects
title: Projects & Tooling
group: Getting Started
icon: fa-folder-tree
order: 5
description: Scaffold with vynec init, declare targets in project.toml, then build, run, and benchmark.
---

# Projects & Tooling

A Vyne project is a directory with a `project.toml` and an entry point. `vynec build` and `vynec run` discover that file, apply its target and device configuration, and then use the normal compile pipeline.

## Scaffold a project

```shell
vynec init
```

On a terminal this opens an FTXUI questionnaire. In scripts and CI it is fully non-interactive:

```shell
vynec init --name demo --kind ml --no-git
cd demo
vynec run src/main.vy
```

The TUI asks up to five questions:

1. **Project kind** — ML training, Embedded, Scientific, Image / Video, or Mixed.
2. **Targets** — Desktop, Arduino Uno R3, generic ESP32 / S2 / S3 / C3 / C6 boards, the measured HW-950 display/touch preset, Coral Edge TPU, Jetson Nano.
3. **Libraries** — `vlin`, `vml`, `vstat`, `vtermplot`, `vfft`, `vcomplex`, `vcsv`, `vimage`, `vconv`, `vplot`, `vraylib`.
4. **CUDA** — only when Desktop is selected and the project is not Embedded. It detects `nvcc` and warns without blocking.
5. **License** — AGPL-3.0-only, MIT, or Proprietary.

| Kind | What the template demonstrates |
| :--- | :--- |
| `blank` | `@bounded` and `@deterministic` regions |
| `ml` | Iris MLP, manual backprop, Adam, `vtermplot` loss curve |
| `embedded` | Uno blinky, generic ESP32 serial greetings, or HW-950 display/touch over `@bounded` |
| `scientific` | Lorenz RK4 integration, Lyapunov estimate, phase portrait |
| `sr` | ESPCN-style 2x upscaling with `vimage` + `vconv` |

`mixed` uses the ML template with every library selected.

### `vynec init` flags

| Flag | Effect |
| :--- | :--- |
| `--name <name>` | project name; also suppresses the TUI |
| `--kind <kind>` | `blank`, `ml`, `embedded`, `scientific`, `sr`, `mixed` |
| `--targets <list>` | `desktop`, `arduino-uno`, `esp32`, `esp32s2`, `esp32s3`, `esp32c3`, `esp32c6`, `hw950`, `coral`, `jetson` |
| `--board <name>` | select an embedded board instead of `--targets` |
| `--preset <name>` | select compiler-managed hardware defaults, e.g. `hw950` |
| `--port <device>` | save the upload/monitor port in `hardware.toml` |
| `--baud <rate>` | save serial baud (default `115200`) |
| `--libraries <list>` | comma-separated library names |
| `--cuda yes\|no` | add `[device.cuda]` |
| `--license <id>` | `AGPL-3.0-only`, `MIT`, `proprietary` |
| `--yes` | accept defaults without prompting |
| `--no-tui` | force non-interactive |
| `--no-git` | skip `git init` and the initial commit |

Exit codes: `0` success, `1` cancel, `2` non-interactive without a name or a terminal smaller than 80x24, `3` `project.toml` already exists, `4` generation or configuration error.

## project.toml

The scaffolder writes a complete manifest. The driver reads the subset shown here; unknown sections are ignored for forward compatibility.

```toml title="project.toml"
[project]
name = "srcnn"
version = "0.1.0"
authors = ["Tuncay Gafarli"]
license = "AGPL-3.0-only"
description = "A comprehensive SRCNN trainer & inferrer"

[toolchain]
vyne = "0.7.0"

[dependencies]
vlin = "0.4"
vml = "0.4"
vstat = "0.2"
voptim = "0.4"
vimage = "0.4"
vconv = "0.4"

[targets]
default = "desktop"

[targets.desktop]
backend = "c"

[targets.uno]
backend = "c"
board = "arduino:avr:uno"

[build]
deterministic = true

[bench]
warmup = 10
iterations = 100
report = "build/bench.json"

[device.cuda]
sm_version = "86"
```

Target keys are intentionally small: `backend`, `device`, and `board`. A `board` value routes the build through the Arduino pipeline; `device = "cuda"` or a `[device.cuda]` section implies host-native code generation.

## Build and run

```shell
vynec build              # discover project.toml, build the default target
vynec run                # build, then run the project entry point
vynec build examples/projects/srcnn
vynec run examples/projects/srcnn
```

Without an argument the driver walks up from the current directory until it finds `project.toml`. The entry point is `src/main.vy`, then `main.vy`. Explicit CLI flags are applied after the project configuration, so `--native`, `--blas`, and `--arduino` still win.

## Embedded hardware: select once, build and run

```shell
vynec init screen_demo --name screen_demo --kind embedded --preset hw950 --port COM5 --no-git
cd screen_demo
vynec hardware setup   # one-time core/library install; requires arduino-cli
vynec hardware show    # inspect all resolved board/display/touch settings
vynec build            # compile firmware only
vynec run              # compile, upload, reset and start
vynec run --monitor    # explicitly select serial monitoring
```

For a generic board, replace `--preset hw950` with `--board esp32s3` (or
`esp32`, `esp32s2`, `esp32c3`, `esp32c6`). Uno uses `--board arduino-uno`.
The interactive target menu offers the same boards and preset labels.
`--board`/`--preset` and `--targets` are alternative ways to select targets;
an explicit board must match the selected preset.

The generated user-facing configuration is TOML:

```toml title="hardware.toml"
[board]
preset = "hw950"

[connection]
port = "COM5"
baud = 115200
```

Presets are bundled under `share/vyne/hardware/*.json` and loaded internally.
Normal applications need no JSON loading, Python launcher or test fixture.
The measured HW-950 preset uses ST7796-compatible initialization, native
240×320 RGB dimensions, vertical mirror correction, GPIO21 backlight,
GPIO27 enable and shared-HSPI XPT2046 touch. Generic boards have no display.
The HW-950 starter uses landscape rotation `1`, shows labeled corners and RGB
bars, and continuously draws touch-contact dots. Its serial diagnostics report
geometry, configured/calibrated touch status, contacts and memory heartbeats.
Send **C** through the serial monitor to explicitly calibrate the four corners;
existing NVS calibration is preserved unless calibration is requested.

### Overrides and validation

Precedence is **CLI overrides → hardware.toml overrides → preset defaults**.
Use native portrait dimensions and rotation `0..3` for orientation:

```toml
[display]
rotation = 1
spi_frequency = 16000000
mirror_y = true
```

```shell
vynec hardware list
vynec hardware show --hardware-set display.rotation=2
vynec build --hardware-set display.rotation=2
vynec run --port COM5 --baud 115200
```

Strings in `--hardware-set` require TOML quotes (for example,
`'display.driver="st7796"'` in PowerShell or a POSIX shell). The resolved
report lists supported display/touch pin fields. Invalid types, duplicate
keys, unknown settings, drivers, dimensions, GPIO ranges/input-only or
flash-reserved pins, shared chip-selects and incompatible board families
produce actionable errors before compilation.

Firmware, generated sketches and compiler intermediates live in
`build/firmware/`. Hardware defines use the ESP32 core's `build_opt.h` hook,
preserving platform USB/board flags. Existing low-level ESP32 build-property
options remain available; `build.extra_flags` additions are staged through
that response file instead of replacing the platform defaults.

`esp32.display_init()` uses the configured default rotation, while
`esp32.display_init(0..3)` honors the explicit source argument. This lets the
same radar/touch examples work inside a project without silently forcing a
different orientation. When response-file hardware options change, Vyne
requests a clean Arduino build so cached objects cannot retain old settings.

The HW-950 Y-mirror flag is the correction from the known-good hardware test
profile; it is not a generic ESP32 default. A local TFT_eSPI `User_Setup.h` may
describe a different configuration. Use `--preset hw950` for these standalone
examples to reproduce the verified board setup without manual property strings:

```shell
vynec --preset hw950 --esp32-flash --esp32-port COM5 examples/test/esp32_wifi_radar.vy
vynec --preset hw950 --esp32-flash --esp32-port COM5 examples/test/esp32_touch.vy
```

Legacy Uno `[board]`, `[pinout]` and `[budget]` metadata remains accepted;
flash/SRAM budget entries are metadata, not resource enforcement. Projects
without `hardware.toml` continue to use their `project.toml` board target.

Run acceptance examples under `examples/templates/` with
`python scripts/test_hardware.py --compile`; add `--port COM5` to upload the
fresh HW-950 project and verify its serial startup. `xmake test-hardware`
runs configuration/initialization checks without requiring a board.

## Plotting and graphics starters

Select `vplot` for an HW-950 project to generate the friendly live-chart starter:

```shell
vynec init dashboard --name dashboard --kind embedded --preset hw950 --libraries vplot --port COM5
```

Then run `vynec hardware setup` once and `vynec run` from the project directory.
See [vplot](page:vplot) and [ESP32 & Hardware](page:esp32).

For desktop graphics, initialize a project with the library dependency and use
the [managed application example](page:vraylib-managed) as `src/main.vy`:

```shell
vynec init graphics_demo --name graphics_demo --kind blank --targets desktop --libraries vraylib
```

Or run the ready-made `examples/projects/raylib-showcase` project. The first
compiled `vraylib` import builds vendored native archives automatically; subsequent
builds reuse the user cache. See [Graphics Showcase](page:graphics) for commands
and [Installation](page:install) for platform prerequisites.

## Benchmark

```shell
vynec bench
vynec bench src/main.vy --warmup 5 --iterations 200
vynec bench --report build/bench.json
```

`vynec bench` compiles every native `.vy` target in the project, runs the configured warm-up count, measures the requested iterations, and writes a JSON report. Board targets are skipped. A single-file invocation writes `<stem>_bench.json` next to the source unless `--report` is given.

```json title="build/bench.json"
{
  "project": "srcnn",
  "target": "desktop",
  "timestamp": "2026-11-10T13:40:28Z",
  "config": { "warmup": 10, "iterations": 100 },
  "results": {
    "src/main.vy": {
      "mean_ms": 378.31,
      "median_ms": 378.25,
      "min_ms": 372.03,
      "max_ms": 386.10,
      "p95_ms": 384.06,
      "p99_ms": 386.10,
      "stddev_ms": 3.08
    }
  }
}
```

## Compiler flags

| Flag | Effect |
| :--- | :--- |
| *(none)* | Compile and run |
| `--ast` | Interpret directly; no C emitted |
| `--verify` | Hash-check source, then interpret |
| `--compile` | Emit C and compile a binary; stop |
| `--run` | Compile and run |
| `--emit-c` | Write `.vy.c`; do not invoke the host compiler |
| `--emit-asm` | Write `.vy.s` via the host compiler |
| `--out <name>` / `-o <name>` | Set the output binary name |
| `--native` | Add `-march=native` to the host compile |
| `--blas` | Lower matched matrix calls to BLAS (desktop only) |
| `--no-scratch-bounds` | Elide generated scratch bounds checks |
| `--quiet` | Suppress notes and warnings |
| `--strict` | Enable strict type checking |
| `--arduino <board>` | Target Arduino; currently `uno` |
| `--arduino-flash` / `--flash` | Upload the built `.hex` with `arduino-cli` |
| `--monitor` | Open `arduino-cli monitor` after flashing |
| `--port <device>` | Serial port, for example `COM3` |
| `--baud <rate>` | Monitor baud rate; default `115200` |
| `--fqbn <string>` | Override the board identifier |

Diagnostics go to stderr. Exit codes are `0` success, `1` compilation or runtime error, `2` CLI misuse.

## Where next

- [CLI Reference](page:cli) for the complete flag list.
- [SRCNN Showcase](page:srcnn) for a project that uses the full stack.
- [Library Overview](page:libs) before adding dependencies.
