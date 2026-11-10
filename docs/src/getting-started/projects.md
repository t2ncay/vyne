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
2. **Targets** — Desktop, Arduino Uno R3, Coral Edge TPU, Jetson Nano.
3. **Libraries** — `vlin`, `vml`, `vstat`, `vtermplot`, `vfft`, `vcomplex`, `vcsv`, `vimage`, `vconv`.
4. **CUDA** — only when Desktop is selected and the project is not Embedded. It detects `nvcc` and warns without blocking.
5. **License** — AGPL-3.0-only, MIT, or Proprietary.

| Kind | What the template demonstrates |
| :--- | :--- |
| `blank` | `@bounded` and `@deterministic` regions |
| `ml` | Iris MLP, manual backprop, Adam, `vtermplot` loss curve |
| `embedded` | Arduino Uno blinky and serial over `@bounded` |
| `scientific` | Lorenz RK4 integration, Lyapunov estimate, phase portrait |
| `sr` | ESPCN-style 2x upscaling with `vimage` + `vconv` |

`mixed` uses the ML template with every library selected.

### `vynec init` flags

| Flag | Effect |
| :--- | :--- |
| `--name <name>` | project name; also suppresses the TUI |
| `--kind <kind>` | `blank`, `ml`, `embedded`, `scientific`, `sr`, `mixed` |
| `--targets <list>` | `desktop`, `arduino-uno`, `coral`, `jetson` |
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
