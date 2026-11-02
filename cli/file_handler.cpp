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

#include "file_handler.h"
#include "../vyne/utils/sha256.h"
#include <map>

// ---------------------------------------------------------------------------
// Integrity check (unchanged)
// ---------------------------------------------------------------------------
bool verifyIntegrity(const std::string& scriptPath) {
    namespace fs = std::filesystem;
    fs::path filePath(scriptPath);
    fs::path baseDir = filePath.parent_path();
    fs::path manifestPath =
        baseDir.empty() ? "checksums.dat" : baseDir / "checksums.dat";

    if (!fs::exists(manifestPath))
        return true;

    std::ifstream manifest(manifestPath);
    if (!manifest.is_open())
        return true;

    std::string relPath, expectedHash;
    std::map<std::string, std::string> expectedHashes;
    while (manifest >> relPath >> expectedHash) {
        expectedHashes[relPath] = expectedHash;
    }

    std::string fileKey = filePath.filename().string();
    auto it = expectedHashes.find(fileKey);
    if (it == expectedHashes.end())
        return true;

    std::string currentHash = SHA256::hashFile(scriptPath);
    if (currentHash != it->second) {
        std::cerr << RED << "[SECURITY ERROR] Integrity check failed for "
                  << scriptPath << "! File modified or corrupted." << RESET
                  << "\n";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------------------
namespace {

std::string outputBase(const std::string& filename, const BuildOptions& opts) {
    if (!opts.outputName.empty())
        return opts.outputName;
    size_t dotPos = filename.find_last_of(".");
    return (dotPos == std::string::npos) ? filename
                                         : filename.substr(0, dotPos);
}

std::string humanSize(uintmax_t bytes) {
    if (bytes < 1024)
        return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024)
        return std::to_string(bytes / 1024) + " KB";
    return std::to_string(bytes / (1024 * 1024)) + " MB";
}

void sectionOpen(const char* label) {
    std::cout << "\n";
    std::cout << CYAN << "  >> " << label << RESET << "\n";
    std::cout << CYAN << "  " << std::string(42, '-') << RESET << "\n\n";
}
void sectionClose() {
    std::cout << "\n";
    std::cout << CYAN << "  " << std::string(42, '-') << RESET << "\n\n";
}

} // namespace

// ---------------------------------------------------------------------------
// Arduino AVR Build Helper
// ---------------------------------------------------------------------------
static int buildArduino(
    const std::string& base,
    const std::string& ardCpp,
    const std::string& exeDir,
    const std::string& mcu,
    const std::string& variant,
    bool emitHex,
    const std::string& fqbn
) {
    std::string vyneInc = exeDir + "/vyne/runtime/arduino";
    std::string coreDir = exeDir + "/vendor/arduino/cores/arduino";
    std::string apiDir = exeDir + "/vendor/arduino/cores/api";
    std::string varDir = exeDir + "/vendor/arduino/variants/" + variant;

    std::string incFlags = " -I\"" + vyneInc +
                           "\""
                           " -I\"" +
                           coreDir +
                           "\""
                           " -I\"" +
                           apiDir +
                           "\""
                           " -I\"" +
                           varDir + "\"";

    std::string commonFlags = std::string(" -mmcu=") + mcu +
                              " -Os -ffunction-sections -fdata-sections -w"
                              " -DF_CPU=16000000UL -DARDUINO=10819";

    std::string objDir = base + "_obj";
    std::error_code ec;
    std::filesystem::create_directories(objDir, ec);

#ifdef _WIN32
    bool isWin = true;
    std::string scriptPath = base + "_build.bat";
#else
    bool isWin = false;
    std::string scriptPath = base + "_build.sh";
#endif

    std::ofstream script(scriptPath);
    if (!script.is_open()) {
        std::cerr << "\n"
                  << RED << "  error" << RESET
                  << "  cannot write build script\n\n";
        return 1;
    }

    // Initialize script
    if (isWin)
        script << "@echo off\n";
    else
        script << "set -e\n";

    std::string objects;
    int fileIdx = 0;

    auto addFile = [&](const std::filesystem::path& p,
                       const std::string& compiler,
                       const std::string& stdFlag,
                       const std::string& extraFlags) {
        std::string obj = objDir + "/" + std::to_string(fileIdx++) + "_" +
                          p.stem().string() + ".o";
        // Write the compile command to the script
        script << compiler << commonFlags << " " << stdFlag << " " << extraFlags
               << incFlags << " -c \"" << p.string() << "\" -o \"" << obj
               << "\"\n";

        if (isWin)
            script << "if errorlevel 1 exit /b 1\n";

        objects += "\"" + obj + "\" ";
    };

    // 1. Generated Vyne code (always C++)
    addFile(
        ardCpp,
        "avr-g++",
        "-std=gnu++11",
        "-fno-exceptions -fno-threadsafe-statics"
    );

    // 2. Core source files
    auto processDir = [&](const std::string& dir) {
        if (!std::filesystem::exists(dir))
            return;
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.path().extension() == ".cpp") {
                addFile(
                    entry.path(),
                    "avr-g++",
                    "-std=gnu++11",
                    "-fno-exceptions -fno-threadsafe-statics"
                );
            } else if (entry.path().extension() == ".c") {
                addFile(entry.path(), "avr-gcc", "-std=gnu11", "");
            }
        }
    };
    processDir(coreDir);
    processDir(apiDir);

    // 3. Link
    std::string outElf = base + ".elf";
    script << "avr-g++" << commonFlags << " -Wl,--gc-sections -o \"" << outElf
           << "\" " << objects << " -lm\n";
    if (isWin)
        script << "if errorlevel 1 exit /b 1\n";

    script.close();

    std::cout << GREEN << "  avr-build " << RESET
              << "  compiling core + sketch...\n";

    // Execute the batch/shell script
    std::string runCmd;
    if (isWin)
        runCmd = "call \"" + scriptPath + "\"";
    else
        runCmd = "sh \"" + scriptPath + "\"";

    int rc = system(runCmd.c_str());

    // Cleanup the temporary script
    std::filesystem::remove(scriptPath, ec);

    if (rc != 0) {
        std::cerr << "\n"
                  << RED << "  error" << RESET
                  << "  avr-gcc build failed — see above\n\n";
        return 1;
    }

    std::cout << GREEN << "  elf       " << RESET << "  " << outElf << "\n";

    if (emitHex) {
        std::string outHex = base + ".hex";
        std::string hexCmd = "avr-objcopy -O ihex -R .eeprom \"" + outElf +
                             "\" \"" + outHex + "\"";
        int hex_rc = system(hexCmd.c_str());
        if (hex_rc != 0) {
            std::cerr << "\n"
                      << RED << "  error" << RESET
                      << "  avr-objcopy failed\n\n";
            return 1;
        }
        std::cout << GREEN << "  hex       " << RESET << "  " << outHex << "\n";
        std::cout << "\n  " << CYAN
                  << "To flash: arduino-cli upload -p <port> --fqbn " << fqbn
                  << " ." << RESET << "\n";
    }

    return 0;
}
// ---------------------------------------------------------------------------
// Transpile + downstream phases
// ---------------------------------------------------------------------------
// Common path for --emit-c, --emit-asm, --compile, --run. The Vyne -> C step
// runs unconditionally; the mode decides where the driver stops.
// ---------------------------------------------------------------------------
static int runTranspile(
    const std::string& filename,
    const std::string& content,
    const BuildOptions& opts
) {
    auto tokens_for_count = tokenize(content);
    int tokenCount = (int)tokens_for_count.size();
    int lineCount = (int)std::count(content.begin(), content.end(), '\n') + 1;

    std::cout << "\n";
    std::cout << BOLD << "  Compiling " << RESET << filename << "\n\n";

    // ---------------- Phase 1: Vyne source -> C source ----------------
    auto start_transpile = std::chrono::high_resolution_clock::now();

    VyneLinker linker;
    auto units = linker.link(filename);

    C_Emitter emitter;
    emitter.reset();
    emitter.setScratchBoundsEnabled(opts.scratchBounds);
    emitter.setArduinoTarget(
        opts.mode == RunMode::ArduinoCompile || opts.mode == RunMode::ArduinoRun
    );

    if (opts.blasEnabled) {
        emitter.setBlasEnabled(true);
        emitter.addSystemInclude("cblas.h");
    }

    for (const auto& unit : units)
        emitter.markImported(unit.canonicalPath);
    for (auto& unit : units) {
        emitter.setSourceDir(
            std::filesystem::path(unit.canonicalPath).parent_path().string()
        );
        if (unit.alias.empty())
            unit.ast->compile(emitter);
        else
            unit.ast->compileAliased(emitter, unit.alias);
    }

    std::string exeDir = FileUtils::getExeDir();
    std::string runtime = exeDir + "/vyne/runtime/vyne_runtime.h";
    std::string cSource = emitter.finalize(runtime);

    std::string base = outputBase(filename, opts);
    std::string cFile = base + ".vy.c";
    std::string asmFile = base + ".vy.s";
    std::string exeName = base;
#ifdef _WIN32
    exeName += ".exe";
#endif

    {
        std::ofstream out(cFile);
        if (!out.is_open()) {
            std::cerr << RED << "  error" << RESET << "  could not write "
                      << cFile << "\n";
            return 1;
        }
        out << cSource;
    }

    auto end_transpile = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> transpile_ms =
        end_transpile - start_transpile;

    std::cout << GREEN << "  transpile" << RESET << "  " << lineCount
              << " lines  " << tokenCount << " tokens  " << std::fixed
              << std::setprecision(2) << transpile_ms.count() << "ms\n";

    // ---------------- Phase 2a: stop here for --emit-c ----------------
    if (opts.mode == RunMode::EmitC) {
        sectionOpen("emitted");
        std::cout << "    " << cFile << "  ("
                  << humanSize(std::filesystem::file_size(cFile)) << ")\n";
        sectionClose();

        vprintln("\n{}{}{}  >> summary {}", BOLD, YELLOW, "", RESET);
        vprintln(
            "{} {} source      {} ({}) {}",
            YELLOW,
            "  -",
            GREEN,
            cFile,
            RESET
        );
        vprintln("     {}lines       {}", GREEN, lineCount);
        vprintln("     {}tokens      {}", GREEN, tokenCount);
        vprintln("     {}transpile   {:.2f}ms", GREEN, transpile_ms.count());
        vprintln(
            "{} {} {}total       {:.2f}ms",
            YELLOW,
            "  -",
            BOLD,
            transpile_ms.count()
        );

        Vyne::DiagnosticEngine::printSummary();
        return 0;
    }

    // ---------------- Arduino path: avr-gcc, not gcc ----------------
    if (opts.mode == RunMode::ArduinoCompile ||
        opts.mode == RunMode::ArduinoRun) {
        if (opts.arduinoBoard.empty()) {
            std::cerr << RED << "  error" << RESET
                      << "  --arduino requires a board name "
                         "(--arduino uno, --arduino nano, ...)\n";
            return 1;
        }

        // Board -> MCU mapping. Keep this table in sync with what
        // arduino-cli would report for `board listall`. This is the
        // minimum needed to build anything today; a proper
        // board-package system can replace it later.
        struct Board {
            const char* name;
            const char* mcu;
            const char* fqbn;
        };
        static const Board boards[] = {
            {"uno", "atmega328p", "arduino:avr:uno"},
            {"nano", "atmega328p", "arduino:avr:nano"},
            {"mega2560", "atmega2560", "arduino:avr:mega"},
            {"leonardo", "atmega32u4", "arduino:avr:leonardo"},
            {"micro", "atmega32u4", "arduino:avr:micro"},
        };
        const Board* board = nullptr;
        for (const auto& b : boards)
            if (opts.arduinoBoard == b.name) {
                board = &b;
                break;
            }
        if (!board) {
            std::cerr << RED << "  error" << RESET
                      << "  unknown Arduino board '" << opts.arduinoBoard
                      << "'; supported: uno, nano, mega2560, leonardo, micro\n";
            return 1;
        }

        // Rename the emitted .vy.c to .cpp. The Arduino toolchain
        // (both the IDE and arduino-cli) only compiles .cpp / .ino /
        // .c files in the sketch directory; a .vy.c would be ignored.
        std::string ardCpp = base + ".ino.cpp";
        std::rename(cFile.c_str(), ardCpp.c_str());

        std::string variant =
            (std::string(board->name) == "mega2560") ? "mega" : "standard";
        bool emitHex = (opts.mode == RunMode::ArduinoRun);

        return buildArduino(
            base,
            ardCpp,
            exeDir,
            board->mcu,
            variant,
            emitHex,
            board->fqbn
        );
    }

    // Common gcc flags, extended per downstream mode.
    auto gccBase = [&]() {
        std::string cmd = "gcc \"" + cFile + "\"";
        cmd += " -I\"" + exeDir + "\" -O3 -w";
        if (opts.nativeIsa)
            cmd += " -march=native";
        if (opts.blasEnabled) {
            std::string obInclude = exeDir + "/vendor/openblas/include";
#ifdef _WIN32
            std::string obLib =
                exeDir + "/vendor/openblas/lib/libopenblas.dll.a";
#else
            std::string obLib = "-lopenblas";
#endif
            cmd += " -DVYNE_USE_OPENBLAS";
            cmd += " -I\"" + obInclude + "\"";
            cmd += " \"" + obLib + "\"";
        }
#ifndef _WIN32
        // libm is a separate shared object on glibc; the C runtime on
        // Windows folds its contents into msvcrt, so this link is only
        // needed on Linux/macOS. Placement matters: it must come after
        // the object files that reference exp/pow/fmod, which it does
        // here since gccBase is concatenated before -o and the output.
        cmd += " -lm";
#endif
        return cmd;
    };

    // ---------------- Phase 2b: assemble for --emit-asm ----------------
    if (opts.mode == RunMode::EmitAsm) {
        std::string cmd = gccBase() + " -S -o \"" + asmFile + "\"";

        auto start_asm = std::chrono::high_resolution_clock::now();
        int rc = system(cmd.c_str());
        auto end_asm = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> asm_ms = end_asm - start_asm;

        if (rc != 0) {
            std::cerr << "\n"
                      << RED << "  error" << RESET
                      << "  gcc -S failed — see above\n\n";
            return 1;
        }

        std::cout << GREEN << "  assemble " << RESET << "  gcc -S"
                  << (opts.nativeIsa ? " -march=native" : "") << "  "
                  << std::fixed << std::setprecision(2) << asm_ms.count()
                  << "ms\n";

        sectionOpen("emitted");
        std::cout << "    " << asmFile << "  ("
                  << humanSize(std::filesystem::file_size(asmFile)) << ")\n";
        std::cout << "    " << cFile << "  ("
                  << humanSize(std::filesystem::file_size(cFile))
                  << ", intermediate)\n";
        sectionClose();

        double total_ms = transpile_ms.count() + asm_ms.count();
        vprintln("\n{}{}{}  >> summary {}", BOLD, YELLOW, "", RESET);
        vprintln(
            "{} {} source      {} ({}) {}",
            YELLOW,
            "  -",
            GREEN,
            asmFile,
            RESET
        );
        vprintln("     {}transpile   {:.2f}ms", GREEN, transpile_ms.count());
        vprintln("     {}assemble    {:.2f}ms", GREEN, asm_ms.count());
        vprintln("{} {} {}total       {:.2f}ms", YELLOW, "  -", BOLD, total_ms);

        Vyne::DiagnosticEngine::printSummary();
        return 0;
    }

    // ---------------- Phase 2c: link for --compile / --run -------------
    std::string cmd = gccBase() + " -o \"" + exeName + "\"";
#ifdef _WIN32
    // 64 MB stack. MSVC and MinGW honour this; GNU ld on Linux does not
    // implement -z stacksize, so the flag is only emitted on Windows.
    cmd += " -Wl,--stack,67108864";
#endif

    auto start_compile = std::chrono::high_resolution_clock::now();
    int compile_result = system(cmd.c_str());
    auto end_compile = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> compile_ms =
        end_compile - start_compile;

    if (compile_result != 0) {
        std::cerr << "\n"
                  << RED << "  error" << RESET
                  << "  gcc failed — see above\n\n";
        return 1;
    }

#ifdef _WIN32
    if (opts.blasEnabled) {
        std::string obDll = exeDir + "/vendor/openblas/bin/libopenblas.dll";
        std::error_code ec;
        std::filesystem::copy_file(
            obDll,
            std::filesystem::path(exeName).parent_path() / "libopenblas.dll",
            std::filesystem::copy_options::overwrite_existing,
            ec
        );
        if (ec) {
            std::cerr << RED << "  warning  could not copy libopenblas.dll: "
                      << ec.message() << RESET << "\n";
        }
    }
#endif

    std::string sizeStr = std::filesystem::exists(exeName)
                              ? humanSize(std::filesystem::file_size(exeName))
                              : "?";

    std::cout << GREEN << "  compile  " << RESET << "  gcc -O3"
              << (opts.nativeIsa ? " -march=native" : "") << "  " << std::fixed
              << std::setprecision(2) << compile_ms.count() << "ms\n";

    // ---------------- Phase 3: run for --run (default) ----------------
    if (opts.mode == RunMode::CompileRun) {
        sectionOpen("output");

        // cmd.exe on Windows treats '/' as a switch prefix and does not
        // resolve a bare relative path against the CWD when the first
        // token isn't the exe name. Normalise to backslashes and force
        // a `.\` prefix for relative paths. POSIX shells are happy with
        // what we already have.
#ifdef _WIN32
        std::string runExe = exeName;
        std::replace(runExe.begin(), runExe.end(), '/', '\\');
        bool isAbsolute =
            (runExe.size() >= 2 && runExe[1] == ':') ||
            (runExe.size() >= 2 && runExe[0] == '\\' && runExe[1] == '\\') ||
            (!runExe.empty() && runExe[0] == '\\');
        if (!isAbsolute && runExe.rfind(".\\", 0) != 0) {
            runExe = ".\\" + runExe;
        }
        std::string runCmd = "\"" + runExe + "\"";
#else
        std::string runCmd = "\"" + exeName + "\"";
#endif

        auto start_exec = std::chrono::high_resolution_clock::now();
        int run_result = system(runCmd.c_str());
        auto end_exec = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> exec_ms =
            end_exec - start_exec;

        sectionClose();

        double total_ms =
            transpile_ms.count() + compile_ms.count() + exec_ms.count();

        vprintln("\n{}{}{}  >> summary {}", BOLD, YELLOW, "", RESET);
        vprintln(
            "{} {} binary     {} ({}) {}",
            YELLOW,
            "  -",
            GREEN,
            exeName,
            CYAN,
            sizeStr,
            RESET
        );
        vprintln("     {}transpile {:.2f}ms", GREEN, transpile_ms.count());
        vprintln("     {}compile   {:.2f}ms", GREEN, compile_ms.count());
        vprintln("     {}execution {:.2f}ms", GREEN, exec_ms.count());
        vprintln("{} {} {}total      {:.2f}ms", YELLOW, "  -", BOLD, total_ms);

        if (run_result != 0)
            std::cout << RED << "  >> exited with code " << run_result << RESET
                      << "\n\n";

        Vyne::DiagnosticEngine::printSummary();
        return 0;
    }

    // ---------------- --compile: stop after the .exe is built ----------------
    double total_ms = transpile_ms.count() + compile_ms.count();
    vprintln("\n{}{}{}  >> summary {}", BOLD, YELLOW, "", RESET);
    vprintln(
        "{} {} binary     {} ({}) {}",
        YELLOW,
        "  -",
        GREEN,
        exeName,
        CYAN,
        sizeStr,
        RESET
    );
    vprintln("     {}transpile {:.2f}ms", GREEN, transpile_ms.count());
    vprintln("     {}compile   {:.2f}ms", GREEN, compile_ms.count());
    vprintln("{} {} {}total      {:.2f}ms", YELLOW, "  -", BOLD, total_ms);

    Vyne::DiagnosticEngine::printSummary();
    return 0;
}

// ---------------------------------------------------------------------------
// Interpret path
// ---------------------------------------------------------------------------
static int runInterpret(
    const std::string& filename,
    SymbolContainer& env,
    const std::shared_ptr<ASTNode>& rootShared,
    Parser& parser
) {
    auto start = std::chrono::high_resolution_clock::now();

    env.setSourceDir(filename);
    uint32_t globalId = StringPool::instance().intern("global");
    rootShared->evaluate(env, globalId);

    auto end = std::chrono::high_resolution_clock::now();
    parser.checkUnusedVariables(env);
    std::chrono::duration<double, std::milli> ms = end - start;

    std::cout << GREEN << "\nExecution finished in: " << ms.count() << "ms"
              << RESET;

    bool hasErrors = false;
    for (const auto& d : Vyne::DiagnosticEngine::getDiagnostics()) {
        if (d.severity == Vyne::Severity::Error ||
            d.severity == Vyne::Severity::Critical)
            hasErrors = true;
    }
    Vyne::DiagnosticEngine::printSummary();
    return hasErrors ? 1 : 0;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int runFile(
    const std::string& filename,
    SymbolContainer& env,
    const BuildOptions& opts
) {
    if (opts.enforceIntegrity && !verifyIntegrity(filename))
        return 1;

    size_t dotPos = filename.find_last_of(".");
    if (dotPos == std::string::npos) {
        std::cerr << RED << "Error: File must end in .vy ( .vyne )" << RESET
                  << "\n";
        return 1;
    }
    std::string ext = filename.substr(dotPos + 1);
    if (ext != "vy" && ext != "vyne") {
        std::cerr << RED << "Error: File must end in .vy ( .vyne )" << RESET
                  << "\n";
        return 1;
    }

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << RED << "Could not open file: " << filename << RESET
                  << "\n";
        return 1;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string content = buffer.str();

    Vyne::DiagnosticEngine::setSourceText(content);
    Vyne::DiagnosticEngine::setCurrentFile(
        std::filesystem::path(filename).filename().string()
    );

    try {
        auto tokens = tokenize(content);
        Parser parser(std::move(tokens));
        parser.setSourceDir(
            std::filesystem::weakly_canonical(
                std::filesystem::absolute(filename)
            )
                .parent_path()
                .string()
        );
        auto programRoot = parser.parseProgram(env);
        std::shared_ptr<ASTNode> rootShared = std::move(programRoot);

        bool hasErrors = false;
        for (const auto& d : Vyne::DiagnosticEngine::getDiagnostics()) {
            if (d.severity == Vyne::Severity::Error ||
                d.severity == Vyne::Severity::Critical)
                hasErrors = true;
        }
        if (hasErrors) {
            std::cerr << RED << "Compilation failed due to errors" << RESET
                      << "\n";
            return 1;
        }

        if (opts.mode == RunMode::Interpret)
            return runInterpret(filename, env, rootShared, parser);

        return runTranspile(filename, content, opts);

    } catch (const std::exception& e) {
        if (Vyne::DiagnosticEngine::getDiagnostics().empty()) {
            const std::string& f = Vyne::DiagnosticEngine::getCurrentFile();
            std::cerr << RED << "Error";
            if (!f.empty())
                std::cerr << " in " << f;
            std::cerr << ": " << e.what() << RESET << "\n";
        }
        Vyne::DiagnosticEngine::printSummary();
        return 1;
    }
}
