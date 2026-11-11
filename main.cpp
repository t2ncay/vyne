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

#include "cli/bench.h"
#include "cli/file_handler.h"
#include "cli/init.h"
#include "cli/packager.h"
#include "cli/project.h"
#include "cli/repl.h"
#include "editors/vscode/lsp/backend/src/lsp_server.h"
#include "vyne/utils/file_utils.h"
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define VYNE_ISATTY _isatty
#define VYNE_FILENO _fileno
#else
#include <unistd.h>
#define VYNE_ISATTY isatty
#define VYNE_FILENO fileno
#endif

static constexpr const char* VYNE_VERSION = "0.7.0";

// ---------------------------------------------------------------------------
// Terminal capability detection
// ---------------------------------------------------------------------------
// Colors are emitted only when stdout is a terminal and NO_COLOR is unset.
// On Windows, VT processing is enabled explicitly so the ANSI sequences are
// interpreted even on Conhost or non-Win10 hosts. On POSIX the sequences are
// always understood; the only reason to suppress is the TTY check above.
//
// The rest of the codebase (file_handler.cpp, repl.cpp, packager.cpp) still
// emits raw ANSI unconditionally. Consolidating that is a follow-up; it does
// not affect correctness, only the piped-output case.
// ---------------------------------------------------------------------------
namespace {

bool g_colorEnabled = true;

void enableVtProcessing() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE)
        return;
    DWORD mode = 0;
    if (!GetConsoleMode(hOut, &mode))
        return;
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, mode);
#endif
}

void configureColor() {
    if (std::getenv("NO_COLOR") != nullptr) {
        g_colorEnabled = false;
        return;
    }
    if (!VYNE_ISATTY(VYNE_FILENO(stdout))) {
        g_colorEnabled = false;
        return;
    }
    enableVtProcessing();
}

// Wrap `s` in `color` and reset. Returns `s` unchanged when colors are off,
// so callers can concatenate without branching on the setting.
std::string C(const char* color, const std::string& s) {
    if (!g_colorEnabled)
        return s;
    return std::string(color) + s + RESET;
}

// Two stacked attributes, e.g. BOLD + YELLOW for section headers.
std::string C2(const char* c1, const char* c2, const std::string& s) {
    if (!g_colorEnabled)
        return s;
    return std::string(c1) + c2 + s + RESET;
}

// One aligned row: colored token in column 1, description in column 2.
// Padding is computed from `left.size()`, which is the visible length —
// escape sequences are added around it, not counted as width.
void row(
    const std::string& left,
    const char* leftColor,
    const std::string& desc,
    int leftWidth
) {
    std::cout << "    " << C(leftColor, left);
    int pad = leftWidth - (int)left.size();
    if (pad < 0)
        pad = 0;
    std::cout << std::string(pad, ' ') << desc << "\n";
}

void header(const std::string& text) {
    std::cout << "\n  " << C2(BOLD, YELLOW, text) << "\n";
}

void divider() {
    std::cout << "  " << C(CYAN, std::string(60, '-')) << "\n";
}

} // namespace

// ---------------------------------------------------------------------------
// Help
// ---------------------------------------------------------------------------
static void printHelp() {
    constexpr int W = 34; // width of the left column, in visible characters

    std::cout << "\n";
    std::cout << "  " << C2(BOLD, CYAN, "Vyne") << " " << C(CYAN, VYNE_VERSION)
              << "  " << C(CYAN, "compiler and runtime for the Vyne language")
              << "\n";
    divider();

    header("USAGE");
    row("vynec <script.vy> [flags]", GREEN, C(CYAN, "compile and run"), W);
    row("vynec <mode> <script.vy> [flags]", GREEN, C(CYAN, "explicit mode"), W);
    row("vynec build [path]", GREEN, C(CYAN, "build the current project"), W);
    row("vynec run [path]", GREEN, C(CYAN, "build and run the project"), W);
    row("vynec bench [file]", GREEN, C(CYAN, "benchmark project targets"), W);
    row("vynec init [path]", GREEN, C(CYAN, "scaffold a new project"), W);

    header("MODES");
    row("--run",
        GREEN,
        "Compile to .exe, then execute  " + C(CYAN, "(default)"),
        W);
    row("--compile", GREEN, "Compile to .exe, stop", W);
    row("--emit-c", GREEN, "Emit .vy.c, do not invoke gcc", W);
    row("--emit-asm", GREEN, "Emit .vy.s via gcc -S", W);
    row("--ast", GREEN, "Interpret directly, no C emitted", W);
    row("--verify", GREEN, "Hash-check source, then interpret", W);

    header("FLAGS");
    row("--native", YELLOW, "Compile with " + C(CYAN, "-march=native"), W);
    row("--no-scratch-bounds",
        YELLOW,
        "Disable VNE-072 bounds checks in emitted C",
        W);
    row("--blas",
        YELLOW,
        "Route vlin.multiply through " + C(CYAN, "cblas_dgemm"),
        W);
    row("-o <name>",
        YELLOW,
        "Override output base name " + C(CYAN, "(default: source stem)"),
        W);
    row("-h, --help", YELLOW, "Show this message", W);
    row("-V, --version", YELLOW, "Show version and exit", W);

    header("ARDUINO");
    row("--arduino <board>",
        YELLOW,
        "Target board  " + C(CYAN, "(uno, nano, mega2560, leonardo)"),
        W);
    row("--arduino-flash",
        YELLOW,
        "Emit " + C(CYAN, ".hex") + " after avr-gcc, no upload",
        W);
    row("--flash",
        GREEN,
        "Build then " + C(CYAN, "arduino-cli upload") + " the .hex",
        W);
    row("--monitor",
        GREEN,
        "Build then " + C(CYAN, "arduino-cli monitor") + " the port",
        W);
    row("--port <port>",
        YELLOW,
        "Serial port  " + C(CYAN, "(COM3, /dev/ttyUSB0)"),
        W);
    row("--device-port <port>", YELLOW, C(CYAN, "alias for --port"), W);
    row("--baud <rate>",
        YELLOW,
        "Monitor baud rate  " + C(CYAN, "(default: 115200)"),
        W);

    header("ESP32");
    row("--esp32 <board>",
        YELLOW,
        "Target board  " +
            C(CYAN, "(esp32, esp32s2, esp32s3, esp32c3, esp32c6)"),
        W);
    row("--esp32-flash",
        GREEN,
        "Build then " + C(CYAN, "arduino-cli upload") + " the .bin",
        W);
    row("--esp32-monitor",
        GREEN,
        "Build then " + C(CYAN, "arduino-cli monitor") + " the port",
        W);
    row("--esp32-port <port>",
        YELLOW,
        "Serial port  " + C(CYAN, "(COM3, /dev/ttyUSB0)"),
        W);
    row("--esp32-baud <rate>",
        YELLOW,
        "Monitor baud rate  " + C(CYAN, "(default: 115200)"),
        W);

    header("PROJECTS");
    row("build [path]",
        MAGENTA,
        "Compile the project.toml target",
        W);
    row("run [path]",
        MAGENTA,
        "Compile and run the project.toml target",
        W);
    row("bench [file]",
        MAGENTA,
        "Benchmark targets and write a JSON report",
        W);
    row("init [path]", MAGENTA, "Scaffold with the interactive questionnaire", W);
    std::cout << "      " << C(CYAN, "templates: ")
              << C(GREEN, "blank, ml, embedded, scientific, sr")
              << C(CYAN, "  (run ") << C(GREEN, "vynec init --help")
              << C(CYAN, " for flags)") << "\n";

    header("TOOLS");
    row("--build-game <script.vy>",
        MAGENTA,
        "Bundle a self-contained release directory",
        W);

    divider();
    std::cout << "\n  " << C(CYAN, "Run ") << C(GREEN, "vynec")
              << C(CYAN, " with no arguments to start the REPL.") << "\n";

    header("EXAMPLES");
    std::cout << "    " << C(GREEN, "vynec init") << "  "
              << C(CYAN, "# interactive questionnaire") << "\n";
    std::cout << "    "
              << C(GREEN, "vynec init --name demo --kind ml --no-git")
              << "  " << C(CYAN, "# non-interactive") << "\n";
    std::cout << "    " << C(GREEN, "vynec examples/training/ml_seq.vy") << "  "
              << C(CYAN, "# compile + run") << "\n";
    std::cout << "    "
              << C(GREEN, "vynec --compile -o ml examples/training/ml_seq.vy")
              << "  " << C(CYAN, "# write ./ml.exe") << "\n";
    std::cout
        << "    "
        << C(GREEN,
             "vynec --emit-asm --native examples/benchmark/matmul_1024.vy")
        << "\n";
    std::cout << "    "
              << C(GREEN,
                   "vynec --blas --native -o matmul_blas "
                   "examples/benchmark/matmul_1024_blas.vy")
              << "\n";
    std::cout << "    "
              << C(GREEN, "vynec --verify examples/training/ml_seq.vy") << "  "
              << C(CYAN, "# integrity-gated run") << "\n";
    std::cout << "    "
              << C(GREEN,
                   "vynec --arduino uno --flash --port COM3 "
                   "examples/arduino/reactive.vy")
              << "\n";
    std::cout << "    "
              << C(GREEN,
                   "vynec --arduino uno --flash --monitor --port COM3 "
                   "examples/arduino/reactive.vy")
              << "\n";

    std::cout << "\n";
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    FileUtils::setExeDir(argv[0]);
    configureColor();

    SymbolContainer env;
    uint32_t globalId = StringPool::instance().intern("global");
    env[globalId] = {};

    // Project scaffolder. Kept ahead of the flag scan so `init` can own
    // its own option namespace without colliding with compile flags.
    if (argc >= 2 && strcmp(argv[1], "init") == 0)
        return runInit(argc, argv);

    // Benchmark driver owns its own option namespace (warmup / iterations /
    // report), so it is intercepted before the generic flag scan too.
    if (argc >= 2 && strcmp(argv[1], "bench") == 0)
        return runBench(argc, argv);

    if (argc == 3 && strcmp(argv[1], "--build-game") == 0) {
        VynePackager packager(argv[2]);
        packager.build();
        return 0;
    }

    // --- Project subcommands -----------------------------------------
    // `vynec build` / `vynec run` are cargo-style entry points. They are
    // intercepted before the generic flag scan so the bare word `run` is
    // never mistaken for a source path, and so a discovered project.toml
    // can seed the options before explicit flags override it.
    const bool projectCommand =
        argc >= 2 &&
        (strcmp(argv[1], "build") == 0 || strcmp(argv[1], "run") == 0);

    BuildOptions opts;
    bool haveMode = false;
    std::string filename;
    int firstArg = 1;

    if (projectCommand) {
        const bool isRun = strcmp(argv[1], "run") == 0;
        opts.mode = isRun ? RunMode::CompileRun : RunMode::CompileOnly;
        haveMode = true;
        firstArg = 2;

        // project.toml discovery walks up from the CWD. When a path is
        // supplied explicitly below, a missing project.toml is not fatal;
        // it only becomes an error if no entry point can be derived.
        ProjectLoadResult loaded = loadProject(".");
        if (loaded.found) {
            if (!loaded.ok) {
                std::cerr << RED << "  error" << RESET << "  " << loaded.error
                          << "\n";
                return 1;
            }

            for (size_t i = 0; i < loaded.config.dependencies.size(); ++i) {
                if (!loaded.config.dependency_paths[i].empty())
                    continue;
                std::cerr
                    << YELLOW << "  warning" << RESET << "  dependency `"
                    << loaded.config.dependencies[i]
                    << "` not found under share/vyne/modules/external\n";
            }

            applyProjectConfig(loaded.config, opts);
            opts.project = loaded.config;
            opts.hasProject = true;
        }
    }

    // --- Flag scan ---------------------------------------------------
    // Modes are mutually exclusive; the last one wins. `-o name` overrides
    // the derived output base. Unknown flags are ignored so external build
    // tools can pass their own through unharmed.
    for (int i = firstArg; i < argc; ++i) {
        std::string arg = argv[i];
        auto setMode = [&](RunMode m) {
            opts.mode = m;
            haveMode = true;
        };

        if (arg == "-h" || arg == "--help") {
            printHelp();
            return 0;
        }
        if (arg == "-V" || arg == "--version") {
            std::cout << "vynec " << VYNE_VERSION << "\n";
            return 0;
        }

        if (arg == "--native")
            opts.nativeIsa = true;
        else if (arg == "--no-scratch-bounds")
            opts.scratchBounds = false;
        else if (arg == "--blas")
            opts.blasEnabled = true;
        else if (arg == "--verify") {
            opts.enforceIntegrity = true;
            setMode(RunMode::Interpret);
        } else if (arg == "--ast") {
            setMode(RunMode::Interpret);
        } else if (arg == "--emit-c") {
            setMode(RunMode::EmitC);
        } else if (arg == "--emit-asm") {
            setMode(RunMode::EmitAsm);
        } else if (arg == "--compile") {
            setMode(RunMode::CompileOnly);
        } else if (arg == "--run") {
            setMode(RunMode::CompileRun);
        } else if (arg == "--arduino" && i + 1 < argc) {
            opts.arduinoBoard = argv[++i];
            setMode(RunMode::ArduinoCompile);
        } else if (arg.rfind("--arduino=", 0) == 0) {
            opts.arduinoBoard = arg.substr(std::strlen("--arduino="));
            setMode(RunMode::ArduinoCompile);
        } else if (arg == "--arduino-flash") {
            setMode(RunMode::ArduinoRun);
        } else if (arg == "--flash") {
            // Upload the emitted .hex via arduino-cli. Implies ArduinoRun
            // (no hex without a build); the upload itself is a post-build
            // step in file_handler.cpp.
            opts.arduinoUpload = true;
            setMode(RunMode::ArduinoRun);
        } else if (arg == "--monitor") {
            // Open arduino-cli monitor on the same port. Can be combined
            // with --flash (upload, then watch) or used alone (attach to
            // a board that's already running the last upload).
            opts.arduinoMonitor = true;
            setMode(RunMode::ArduinoRun);
        } else if (
            (arg == "--port" || arg == "--device-port") && i + 1 < argc
        ) {
            opts.arduinoPort = argv[++i];
        } else if (arg.rfind("--port=", 0) == 0) {
            opts.arduinoPort = arg.substr(std::strlen("--port="));
        } else if (arg.rfind("--device-port=", 0) == 0) {
            opts.arduinoPort = arg.substr(std::strlen("--device-port="));
        } else if (arg == "--baud" && i + 1 < argc) {
            try {
                opts.baudRate = std::stoi(argv[++i]);
            } catch (...) {
                std::cerr << RED << "  error" << RESET
                          << "  --baud requires an integer\n";
                return 1;
            }
        } else if (arg == "--esp32" && i + 1 < argc) {
            opts.esp32Board = argv[++i];
            setMode(RunMode::ESP32Compile);
        } else if (arg.rfind("--esp32=", 0) == 0) {
            opts.esp32Board = arg.substr(std::strlen("--esp32="));
            setMode(RunMode::ESP32Compile);
        } else if (arg == "--esp32-flash") {
            opts.esp32Upload = true;
            setMode(RunMode::ESP32Run);
        } else if (arg == "--esp32-monitor") {
            opts.esp32Monitor = true;
            setMode(RunMode::ESP32Run);
        } else if (arg == "--esp32-port" && i + 1 < argc) {
            opts.esp32Port = argv[++i];
        } else if (arg.rfind("--esp32-port=", 0) == 0) {
            opts.esp32Port = arg.substr(std::strlen("--esp32-port="));
        } else if (arg == "--esp32-baud" && i + 1 < argc) {
            try {
                opts.esp32Baud = std::stoi(argv[++i]);
            } catch (...) {
                std::cerr << RED << "  error" << RESET
                          << "  --esp32-baud requires an integer\n";
                return 1;
            }
        } else if (arg == "-o" && i + 1 < argc)
            opts.outputName = argv[++i];
        else if (arg.size() > 2 && arg[0] == '-' && arg[1] == 'o')
            opts.outputName = arg.substr(2);
        else if (!arg.empty() && arg[0] != '-') {
            if (filename.empty())
                filename = arg;
        }
    }

    if (filename.empty()) {
        if (projectCommand) {
            if (!opts.hasProject) {
                std::cerr << RED << "  error" << RESET
                          << "  could not find `project.toml`\n";
                return 1;
            }

            std::string entry;
            if (!findProjectEntry(opts.project.root_dir, entry)) {
                std::cerr << RED << "  error" << RESET
                          << "  could not find `src/main.vy` or `main.vy`\n";
                return 1;
            }
            filename = entry;
        } else {
            std::string input;
            init_REPL(input, env);
            return 0;
        }
    }

    if (!haveMode)
        opts.mode = RunMode::CompileRun;

    return runFile(filename, env, opts);
}
