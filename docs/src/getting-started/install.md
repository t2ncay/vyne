---
id: install
title: Installation
group: Getting Started
icon: fa-download
order: 2
description: Build vynec, verify the toolchain, and set up Arduino/ESP32, CUDA, and desktop raylib graphics.
---

# Installation

Vyne is a source-built toolchain. The compiler and desktop runtime are C++26; generated programs are C11; the Arduino target additionally needs `avr-gcc`, `avr-libc`, and `arduino-cli`.

## Prerequisites

| Component | Needed for | Notes |
| :--- | :--- | :--- |
| C++26 compiler | building `vynec` | GCC 14+, Clang 18+, or MSVC 19.40+ |
| [xmake](https://xmake.io/) | build system | `scoop install xmake`, `brew install xmake`, or the official install script |
| C11 compiler | generated programs | GCC, Clang, or MSVC; defaults to the host compiler |
| `avr-gcc` + `avr-libc` | Arduino firmware | the AVR toolchain from Arduino or your package manager |
| `arduino-cli` + `arduino:avr` | flashing and serial monitor | `arduino-cli core install arduino:avr` |
| OpenBLAS | `--blas` | optional; vendored binaries live under `vendor/openblas/` |
| `arduino-cli` + `esp32:esp32` | ESP32 firmware | `vynec hardware setup` installs the selected core/libraries |
| GCC + CMake + make | `vraylib` compiled desktop programs | vendored raylib builds automatically on first import |

The compiler vendors its other dependencies: `stb_image` / `stb_image_write` for `vimage`, and the FTXUI amalgamated header used by `vynec init`.

## Build

```shell title="Build with xmake"
# macOS / Linux / Windows (PowerShell)
xmake

# Windows convenience wrapper
build.bat
```

The build produces `vynec` in the repository root. To check it:

```shell title="Verify the compiler"
./vynec --version
./vynec --help
```

## First program

```shell title="hello.vy"
out("hello, vyne");
```

```shell title="Run it"
./vynec hello.vy          # compile and run
./vynec --ast hello.vy    # interpret, no C emitted
./vynec --compile hello.vy
./hello
```

A bare `vynec hello.vy` compiles and runs the program. Add `--ast` when you want the interpreter semantics with no C emission, or `--emit-c` when you want to inspect the generated translation unit.

## REPL

```shell
./vynec
```

The REPL evaluates expressions and declarations against the live symbol table. Use it for quick numeric checks and API discovery; use `--ast` on real files when you need reproducible interpreter runs.

## Arduino toolchain

Verify the AVR tools before the first firmware build:

```shell
avr-gcc --version          # expect "avr-gcc (GCC) ..."
avr-objcopy --version
arduino-cli core list      # expect "arduino:avr ... installed"
```

Then build, flash, and monitor:

```shell
./vynec --arduino uno examples/arduino/blink.vy
./vynec --arduino uno --flash --monitor --port COM3 examples/arduino/tinyml_xor.vy
```

`--arduino uno` alone produces `.elf` and `.hex` files. `--flash` and `--monitor` invoke `arduino-cli` after the build. `--fqbn` overrides the default board identifier (`arduino:avr:uno`).

## ESP32 setup

Install `arduino-cli`, select a board/preset, and let the hardware workflow install
the core and libraries:

```shell
vynec init screen --name screen --kind embedded --preset hw950 --port COM5
cd screen
vynec hardware setup
vynec build
vynec run --monitor
```

Generic ESP32 boards use `--board esp32` (or S2/S3/C3/C6 aliases) instead of the
HW-950 preset. See [ESP32 & Hardware](page:esp32) for memory, display, and touch.

## Desktop graphics and audio

`vraylib` supports compiled Windows (MinGW), Linux, and macOS desktop programs.
It requires GCC, CMake, and make; rendering needs an OpenGL 3.3 context.
Windows additionally needs `mingw32-make` on PATH. On Ubuntu install
`libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev`;
`xvfb` supplies a virtual display for rendering tests. macOS needs command-line
developer tools. The compiler's own C++ toolchain requirement still applies.

```shell
vynec run examples/projects/raylib-showcase
```

The first build compiles vendored raylib into `%LOCALAPPDATA%/vyne/raylib/` on
Windows or the XDG/`~/.cache/vyne/raylib/` directory on Unix. `VYNE_RAYLIB_CACHE`
overrides that location. Archives are statically linked: no system raylib package
or copied DLL is needed. `--emit-c` / `--emit-asm` do not build/link archives.
`--ast`, AVR, and ESP32 are not execution targets for `vraylib`.

See [Managed Graphics & Audio](page:vraylib-managed),
[Native Reference](page:vraylib), and [Graphics Showcase](page:graphics).

## CUDA path

The `@device<cuda>` backend uses the CUDA **driver API** at runtime. You do **not** need CUDA headers or link flags for generated programs. CI and ordinary builds do not need `nvcc`.

If you change the device kernels and need to regenerate the embedded PTX:

```powershell
pwsh -File scripts/build_cuda_ptx.ps1
```

The script uses `nvcc -ptx -arch=compute_80` and must run inside a Visual Studio developer environment. The generated header is committed, so this is a maintainer step, not an install requirement.

## Editors

- **VS Code** — extension and language server under `editors/vscode/lsp/`, with prebuilt VSIX packages.
- **Neovim** — plugin under `editors/nvim/` with file-type detection, syntax, and an LSP client.
- **Doxygen** — `doxygen Doxyfile` for the compiler and runtime internals.

## Verify the install

```shell
xmake test
xmake test-init
```

`xmake test` runs the compiler, runtime, numerical library, region-safety, and (when the AVR toolchain is present) host-side MCU translation-unit tests. `xmake test-init` exercises every `vynec init` template.
