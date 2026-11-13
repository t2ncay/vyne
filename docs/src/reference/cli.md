---
id: cli
title: CLI Reference
group: Reference
icon: fa-terminal
order: 1
description: Every vynec command and flag, project discovery, Arduino upload, and benchmark reports.
---

# CLI Reference

```text
vynec <script.vy> [flags]          compile and run
vynec <mode> <script.vy> [flags]   explicit mode
vynec build [path]                 build the current project
vynec run [path]                   build and run the project
vynec bench [file]                 benchmark project targets
vynec init [path]                  scaffold a new project
vynec hardware list               list bundled board/preset labels
vynec hardware show               print resolved hardware as TOML
vynec hardware setup              install the selected core/libraries
```

## Execution modes

| Flag | Effect |
| :--- | :--- |
| *(none)* | Compile and run |
| `--run` | Compile and run |
| `--ast` | Interpret directly; no C emitted |
| `--verify` | Hash-check source, then interpret |
| `--compile` | Emit C and compile a binary; stop |
| `--emit-c` | Write `.vy.c`; do not invoke the host compiler |
| `--emit-asm` | Write `.vy.s` via the host compiler |

```shell
vynec --ast script.vy
vynec --compile script.vy
vynec --emit-c script.vy
vynec --emit-asm --native script.vy
```

## Output and codegen

| Flag | Effect |
| :--- | :--- |
| `-o <name>` / `--out <name>` | Set the output binary name |
| `--native` | Add `-march=native` to the host compile |
| `--blas` | Lower matched matrix calls to BLAS (desktop only) |
| `--no-scratch-bounds` | Elide generated scratch bounds checks |
| `--quiet` | Suppress notes and warnings |
| `--strict` | Enable strict type checking |
| `-h`, `--help` | Show help |
| `-V`, `--version` | Show version |

## Arduino

| Flag | Effect |
| :--- | :--- |
| `--arduino <board>` | Target Arduino; currently `uno` |
| `--arduino-flash` / `--flash` | Upload the built `.hex` with `arduino-cli` |
| `--monitor` | Open `arduino-cli monitor` after flashing |
| `--port <device>` / `--device-port <device>` | Serial port, for example `COM3` |
| `--baud <rate>` | Monitor baud rate; default `115200` |
| `--fqbn <string>` | Override the board identifier |

```shell
vynec --arduino uno sketch.vy
vynec --arduino uno --flash --monitor --port COM3 sketch.vy
```

`--arduino uno` alone produces `.elf` and `.hex`. `--flash` and `--monitor` are post-build actions.

## Projects

Embedded projects load `hardware.toml` automatically. `vynec build` compiles
without uploading; `vynec run` compiles, uploads, resets and starts the board.
Monitoring is opt-in with `--monitor`. Generated firmware and sketches live
under `build/firmware/`.

| Hardware option | Effect |
| :--- | :--- |
| `--preset <name>` | Override the bundled preset (`hw950`, `esp32`, `esp32s2`, `esp32s3`, `esp32c3`, `esp32c6`) |
| `--hardware-set section.key=value` | Repeatable typed TOML override, e.g. `display.rotation=1` |
| `--port <device>` | Override configured upload/monitor port for either board family |
| `--baud <rate>` | Override configured serial baud |

`hardware show` and `hardware setup` also accept `--preset` and
`--hardware-set`. Configuration precedence is CLI → TOML → preset defaults.
`vynec init --kind embedded --preset hw950 --port COM5 --name demo` creates a
project ready for the short workflow after `vynec hardware setup`.

```shell
vynec build
vynec run
vynec build examples/projects/srcnn
vynec run examples/projects/srcnn
```

Without a path, the driver walks up from the current directory until it finds `project.toml`, then uses `src/main.vy`, then `main.vy`. Project configuration is applied before explicit CLI flags, so flags still win.

See [Projects & Tooling](page:projects) for the manifest schema and templates.

## Benchmark

```shell
vynec bench
vynec bench src/main.vy --warmup 5 --iterations 200
vynec bench --report build/bench.json
```

| Flag | Effect |
| :--- | :--- |
| `--warmup <n>` | Warm-up runs before measuring |
| `--iterations <n>` | Measured runs |
| `--report <path>` | Override the JSON report path |

Project benchmarks use `[bench]` from `project.toml`. Board targets are skipped. Single-file mode writes `<stem>_bench.json` next to the source unless `--report` is given.

## Init

```shell
vynec init
vynec init --name demo --kind ml --no-git
vynec init --help
```

| Flag | Effect |
| :--- | :--- |
| `--name <name>` | Project name; suppresses the TUI |
| `--kind <kind>` | `blank`, `ml`, `embedded`, `scientific`, `sr`, `mixed` |
| `--targets <list>` | `desktop`, `arduino-uno`, `coral`, `jetson` |
| `--libraries <list>` | Comma-separated library names |
| `--cuda yes\|no` | Write `[device.cuda]` |
| `--license <id>` | `AGPL-3.0-only`, `MIT`, `proprietary` |
| `--yes` | Accept defaults |
| `--no-tui` | Force non-interactive |
| `--no-git` | Skip `git init` and the initial commit |

Exit codes: `0` success, `1` cancel, `2` non-interactive without a name or a terminal smaller than 80×24, `3` `project.toml` already exists, `4` generation or configuration error.

## Exit codes

| Code | Meaning |
| :--- | :--- |
| `0` | Success |
| `1` | Compilation or runtime error |
| `2` | CLI misuse |

Diagnostics go to stderr. Color is enabled only when stdout is a terminal and `NO_COLOR` is unset.

## Examples

```shell
vynec examples/hello.vy
vynec --compile -o ml examples/training/ml_seq.vy
vynec --blas --native -o matmul_blas examples/benchmark/matmul_1024_vlin.vy
vynec --emit-asm --native examples/benchmark/matmul_1024.vy
vynec --arduino uno --flash --port COM3 examples/arduino/blink.vy
```

## See also

- [Installation](page:install)
- [Projects & Tooling](page:projects)
- [Error Codes](page:errcodes)
