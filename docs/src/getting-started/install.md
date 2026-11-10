---
id: install
title: Installation
group: Getting Started
icon: fa-download
order: 2
description: Build vynec from source, verify the toolchain, and set up the optional Arduino and CUDA paths.
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
