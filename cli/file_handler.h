// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Tuncay Gafarli
//
// This file is part of the Vyne compiler.
//
// Vyne is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License as published by the
// Free Software Foundation, version 3.
//
// Vyne is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public
// License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with Vyne. If not, see <https://www.gnu.org/licenses/>.

#pragma once
#include <chrono>
#include <cstdio>
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../vyne/compiler/ast/ast.h"
#include "../vyne/compiler/ast/value.h"
#include "../vyne/compiler/codegen/codegen.h"
#include "../vyne/compiler/codegen/linker.h"
#include "../vyne/compiler/lexer/lexer.h"
#include "../vyne/compiler/parser/parser.h"

#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define CYAN "\033[36m"
#define BOLD "\033[1m"
#define MAGENTA "\033[35m"

// ---------------------------------------------------------------------------
// Run modes
// ---------------------------------------------------------------------------
// Each mode names the LAST phase the driver runs. Every phase before it runs
// unconditionally, so `--emit-asm` implies `--emit-c` and `--run` implies
// `--compile`. This mirrors `rustc --emit=…` and `cargo build` / `cargo run`.
//
//   Interpret    parse + evaluate (interpreter, no C is emitted)
//   EmitC        parse + emit .vy.c
//   EmitAsm      parse + emit .vy.c + gcc -S -> .vy.s
//   CompileOnly  parse + emit .vy.c + gcc    -> .exe
//   CompileRun   parse + emit .vy.c + gcc    -> .exe, then execute it
//
// CompileRun is the default when a filename is present and no mode flag is
// given, matching `go run` / `cargo run` ergonomics.
// ---------------------------------------------------------------------------
enum class RunMode {
    Interpret,
    EmitC,
    EmitAsm,
    CompileOnly,
    CompileRun,
    ArduinoCompile, // emit .ino-ready .cpp + avr-gcc -c, no link
    ArduinoRun,     // + avr-objcopy -> .hex, ready for avrdude
    ESP32Compile,   // emit .ino, arduino-cli compile, produce .bin
    ESP32Run        // + arduino-cli upload, optionally monitor
};

// ---------------------------------------------------------------------------
// Project configuration (project.toml)
// ---------------------------------------------------------------------------
// Parsed by cli/project.cpp. Only the schema the build driver needs is
// represented here; unknown sections and keys are ignored by the parser so
// project.toml can grow without breaking older compilers.
//
// Field names mirror the TOML keys:
//
//   [project]          name, version
//   [dependencies]     vlin = "0.4", vml = "0.4", ...
//   [targets]          default = "desktop"
//   [targets.desktop]  backend = "c", device = "cuda", board = "..."
//   [build]            deterministic = true
//   [device.cuda]      sm_version = "86"
// ---------------------------------------------------------------------------
struct ProjectConfig {
    std::string name;
    std::string version;
    std::string default_target;
    std::string backend;
    std::string device; // "cuda", "coral", ...
    std::string board;  // "arduino:avr:uno"
    std::vector<std::string> dependencies;
    bool deterministic = false;
    int cuda_sm = 0;

    // [bench] section. Defaults match the scaffold written by `vynec init`.
    int bench_warmup = 10;
    int bench_iterations = 100;
    std::string bench_report = "build/bench.json";

    // Discovery metadata. `root_dir` is the directory containing
    // project.toml; `toml_path` is the file itself. `dependency_paths` is
    // parallel to `dependencies` — an empty entry means the dependency was
    // not found in any known module root.
    std::string root_dir;
    std::string toml_path;
    std::vector<std::string> dependency_paths;
};

struct BuildOptions {
    RunMode mode = RunMode::Interpret;
    bool nativeIsa = false;
    bool enforceIntegrity = false;
    bool scratchBounds = true;
    bool blasEnabled = false;
    std::string outputName;
    std::string arduinoBoard; // "uno", "nano", "mega2560", "leonardo" — empty =
                              // not arduino

    // Arduino post-build actions. All three are opt-in; the two booleans
    // only take effect under ArduinoRun (which --flash / --monitor set).
    // They run arduino-cli AFTER avr-gcc produces the .hex — they do not
    // change what gets built, only what happens next.
    std::string arduinoPort;     // --port / --device-port, e.g. "COM3"
    bool arduinoUpload = false;  // --flash: arduino-cli upload <hex>
    bool arduinoMonitor = false; // --monitor: arduino-cli monitor
    int baudRate = 115200;       // --baud: must match vyne_arduino.h

    // ESP32 target. This pipeline is independent of the AVR one: it
    // emits a .ino, then delegates to `arduino-cli compile/upload/monitor`
    // so the Espressif core supplies its own toolchain.
    std::string esp32Board;       // "esp32", "esp32s3", ...
    std::string esp32Port;        // --esp32-port or --port
    bool esp32Upload = false;     // --esp32-flash
    bool esp32Monitor = false;    // --esp32-monitor
    int esp32Baud = 115200;       // --esp32-baud
    std::vector<std::string> esp32BuildProperties; // repeatable arduino-cli overrides
    std::string hardwareFlags; // generated build_opt.h response-file options
    std::string boardFqbn;     // complete project/hardware FQBN, including board options

    // Present when the driver was launched through `vynec build` / `vynec
    // run` and a project.toml was discovered. The flag-based CLI leaves
    // this clear, so the project box is never printed for plain
    // `vynec --run foo.vy`.
    ProjectConfig project;
    bool hasProject = false;

    // Internal compile-only mode used by the benchmark driver. It keeps
    // diagnostics and errors but suppresses the normal build/run UI.
    bool quiet = false;
};

// ---------------------------------------------------------------------------
// Benchmark options
// ---------------------------------------------------------------------------
struct BenchOptions {
    int warmup = 10;
    int iterations = 100;
    std::string reportPath; // resolved output path
};

int runFile(
    const std::string& filename,
    SymbolContainer& env,
    const BuildOptions& opts
);

// Shared section UI, exposed so `vynec bench` can render the same
// `>> project` and sectioned boxes as `vynec build` / `vynec run`.
void sectionOpen(const char* label);
void sectionClose();
void printProjectBox(const ProjectConfig& cfg);

template <typename... Args>
static inline void vprint(std::string_view fmt, Args&&... args) {
    std::string s = std::vformat(fmt, std::make_format_args(args...));
    std::printf("%s", s.c_str());
}

template <typename... Args>
static inline void vprintln(std::string_view fmt, Args&&... args) {
    std::string s = std::vformat(fmt, std::make_format_args(args...));
    std::printf("%s\n", s.c_str());
}
