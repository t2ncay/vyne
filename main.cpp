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

#include "cli/file_handler.h"
#include "cli/packager.h"
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

static constexpr const char* VYNE_VERSION = "0.4.0";

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

    header("TOOLS");
    row("--build-game <script.vy>",
        MAGENTA,
        "Bundle a self-contained release directory",
        W);
    row("--lsp", MAGENTA, "Start the language server", W);

    divider();
    std::cout << "\n  " << C(CYAN, "Run ") << C(GREEN, "vynec")
              << C(CYAN, " with no arguments to start the REPL.") << "\n";

    header("EXAMPLES");
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

    if (argc == 3 && strcmp(argv[1], "--build-game") == 0) {
        VynePackager packager(argv[2]);
        packager.build();
        return 0;
    }

    // --- Flag scan ---------------------------------------------------
    // Modes are mutually exclusive; the last one wins. `-o name` overrides
    // the derived output base. Unknown flags are ignored so external build
    // tools can pass their own through unharmed.
    BuildOptions opts;
    bool haveMode = false;
    std::string filename;

    for (int i = 1; i < argc; ++i) {
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
        std::string input;
        init_REPL(input, env);
        return 0;
    }

    if (!haveMode)
        opts.mode = RunMode::CompileRun;

    return runFile(filename, env, opts);
}
