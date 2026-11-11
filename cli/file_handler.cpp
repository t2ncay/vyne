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
#include "../vyne/compiler/contract_config.h"
#include <functional>
#include <map>
#include <vector>

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

} // namespace

void sectionOpen(const char* label) {
    std::cout << "\n";
    std::cout << CYAN << "  >> " << label << RESET << "\n";
    std::cout << CYAN << "  " << std::string(42, '-') << RESET << "\n\n";
    // Flush before a child process writes directly to the same console.
    // Without this, a piped/captured run can interleave the child's output
    // ahead of the section header.
    std::cout.flush();
}
void sectionClose() {
    std::cout << "\n";
    std::cout << CYAN << "  " << std::string(42, '-') << RESET << "\n\n";
    std::cout.flush();
}

// One row inside the `>> project` box: colored label, aligned value.
// Padding is computed from `label.size()` (visible length), so ANSI escapes
// do not disturb the column.
static void projectRow(
    const std::string& label,
    const std::string& value,
    const char* valueColor
) {
    constexpr int LABEL_WIDTH = 13;
    std::cout << "    " << CYAN << label << RESET;
    int pad = LABEL_WIDTH - static_cast<int>(label.size());
    if (pad < 0)
        pad = 0;
    std::cout << std::string(pad, ' ') << valueColor << value << RESET << "\n";
}

// Project metadata banner shown before the compilation phase for
// `vynec build` / `vynec run`. Mirrors the visual language of `>> summary`
// and `>> output` via sectionOpen()/sectionClose().
void printProjectBox(const ProjectConfig& cfg) {
    sectionOpen("project");

    projectRow("name", cfg.name.empty() ? "-" : cfg.name, BOLD GREEN);
    projectRow("version", cfg.version.empty() ? "-" : cfg.version, GREEN);

    std::string target =
        cfg.default_target.empty() ? "-" : cfg.default_target;
    if (!cfg.backend.empty())
        target += " (" + cfg.backend + ")";
    projectRow("target", target, YELLOW);

    std::string deps;
    for (size_t i = 0; i < cfg.dependencies.size(); ++i) {
        if (i)
            deps += ", ";
        deps += cfg.dependencies[i];
    }
    if (deps.empty())
        deps = "(none)";
    projectRow("deps", deps, GREEN);

    sectionClose();
}

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
    const std::string& fqbn,
    const std::string& boardName
) {
    std::string vyneInc = exeDir + "/vyne/runtime/arduino";
    std::string coreDir = exeDir + "/vendor/arduino/cores/arduino";
    std::string apiDir = exeDir + "/vendor/arduino/cores/api";
    std::string varDir = exeDir + "/vendor/arduino/variants/" + variant;
    std::string libRoot = exeDir + "/vendor/arduino/libraries";

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

    // Discover bundled Arduino libraries (Wire, SPI, SoftwareSerial, ...).
    // Every library's `src/` directory joins the include path so any
    // header resolves. Compilation is separate and demand-driven (step 3
    // below): only libraries whose VYNE_USE_<bus> macro appears in the
    // generated translation unit are built, because their global
    // constructors survive --gc-sections. Recursion picks up nested
    // utility directories such as Wire's `src/utility/twi.c`.
    std::vector<std::filesystem::path> librarySrcDirs;
    {
        std::error_code lec;
        if (std::filesystem::exists(libRoot, lec)) {
            for (const auto& libEntry :
                 std::filesystem::directory_iterator(libRoot, lec)) {
                if (!libEntry.is_directory())
                    continue;
                std::filesystem::path src = libEntry.path() / "src";
                if (!std::filesystem::exists(src, lec) ||
                    !std::filesystem::is_directory(src, lec))
                    continue;
                librarySrcDirs.push_back(src);
                incFlags += " -I\"" + src.string() + "\"";
            }
        }
    }

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
    std::function<void(const std::string&)> processDir =
        [&](const std::string& dir) {
            if (!std::filesystem::exists(dir))
                return;
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (entry.is_directory()) {
                    processDir(entry.path().string());
                    continue;
                }
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

    // 3. Bundled Arduino libraries. The include paths above are always
    //    added so any header resolves. Compilation is demand-driven: a
    //    library is compiled only when the generated translation unit
    //    defines its VYNE_USE_<bus> macro. This matters because these
    //    objects carry global constructors (TwoWire) that --gc-sections
    //    cannot drop, so compiling Wire into every blink sketch would
    //    cost ~1.4 KB of flash for nothing.
    std::string generatedSource;
    {
        std::ifstream gen(ardCpp);
        std::stringstream ss;
        ss << gen.rdbuf();
        generatedSource = ss.str();
    }
    auto usesMacro = [&](const char* macro) {
        return generatedSource.find(std::string("#define ") + macro + " 1") !=
               std::string::npos;
    };
    struct BundledLib {
        const char* macro;
        const char* name;
    };
    static const BundledLib bundledLibs[] = {
        {"VYNE_USE_I2C", "Wire"},
        {"VYNE_USE_SPI", "SPI"},
    };
    for (const auto& src : librarySrcDirs) {
        std::string libName = src.parent_path().filename().string();
        bool needed = false;
        for (const auto& lib : bundledLibs) {
            if (libName == lib.name && usesMacro(lib.macro)) {
                needed = true;
                break;
            }
        }
        if (needed)
            processDir(src.string());
    }

    // 4. Link
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
                  << " -i \"" << outHex << "\"" << RESET << "\n";
        std::cout << "  " << CYAN << "Or:       vynec --arduino " << boardName
                  << " --flash --port <port> <script.vy>" << RESET << "\n";
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Arduino post-build actions: upload and monitor
// ---------------------------------------------------------------------------
// Both are opt-in (--flash / --monitor) and both need a serial port. If
// --port was not passed, we prompt on stdin. If stdin is closed or piped
// from something that doesn't send a port, promptPort() fails fast with a
// clear error rather than hanging on an invisible prompt.
// ---------------------------------------------------------------------------
static std::string promptPort() {
    std::cout << "\n  " << YELLOW << "Enter Arduino port (e.g. COM3): " << RESET
              << std::flush;
    std::string port;
    if (!std::getline(std::cin, port)) {
        std::cerr << "\n  " << RED << "error" << RESET
                  << "  no port given (stdin closed); pass --port\n\n";
        return "";
    }
    size_t start = port.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return "";
    size_t end = port.find_last_not_of(" \t\r\n");
    return port.substr(start, end - start + 1);
}

// Upload the emitted .hex to the board. The command is exactly what
// arduino-cli expects; the hex path is quoted so paths with spaces still
// resolve. Fails fast on a nonzero arduino-cli exit so the caller doesn't
// try to open a monitor against a board that never got the new firmware.
static int doArduinoUpload(
    const std::string& base,
    const std::string& fqbn,
    std::string& port
) {
    std::string hexPath = base + ".hex";

    sectionOpen("flash");

    if (port.empty()) {
        port = promptPort();
        if (port.empty()) {
            sectionClose();
            return 1;
        }
    }

    std::cout << "    " << CYAN << "port" << RESET << "        " << port
              << "\n";
    std::cout << "    " << CYAN << "fqbn" << RESET << "        " << fqbn
              << "\n";
    std::cout << "    " << CYAN << "hex" << RESET << "         " << hexPath
              << "\n\n";

    std::string cmd = "arduino-cli upload -p " + port + " --fqbn " + fqbn +
                      " -i \"" + hexPath + "\"";
    std::cout << "    " << CYAN << "running" << RESET << "     " << cmd
              << "\n\n";

    int rc = system(cmd.c_str());
    if (rc != 0) {
        sectionClose();
        std::cerr << RED << "  error" << RESET
                  << "  arduino-cli upload failed (exit " << rc << ")\n\n";
        return 1;
    }

    sectionClose();
    return 0;
}

// Open the serial monitor on the same port. Blocking call: the user exits
// with Ctrl+C. arduino-cli handles the SIGINT and returns cleanly; the
// parent's system() returns and we print the section close. On terminals
// where Ctrl+C kills the whole process group, the prompt still comes back
// to the shell — either behaviour is acceptable, since the user asked for
// a blocking monitor.
static int doArduinoMonitor(const std::string& port, int baud) {
    if (port.empty())
        return 0;

    sectionOpen("monitor");

    std::cout << "    " << CYAN << "port" << RESET << "        " << port
              << "\n";
    std::cout << "    " << CYAN << "baud" << RESET << "        " << baud
              << "\n\n";

    std::string cmd = "arduino-cli monitor -p " + port +
                      " -c baudrate=" + std::to_string(baud);
    std::cout << "    " << CYAN << "running" << RESET << "     " << cmd << "\n";
    std::cout << "    " << YELLOW << "(press Ctrl+C to exit)" << RESET
              << "\n\n";

    int rc = system(cmd.c_str());
    sectionClose();
    return rc;
}

// ---------------------------------------------------------------------------
// ESP32 Build Helper
// ---------------------------------------------------------------------------
// The ESP32 core is too large to reproduce the xtensa toolchain invocation
// by hand (unlike the AVR path above), so this pipeline delegates the
// compile/upload/monitor phases to `arduino-cli`.
//
// The generated `.vy.c` is moved into a throwaway sketch directory as
// `<sketch>/<sketch>.ino`. The runtime headers are copied alongside it
// rather than passed through `compiler.cpp.extra_flags`: Arduino sketch
// directories are implicitly on the include path, which sidesteps the
// cmd.exe quoting problems caused by spaces in the Vyne install path.
static int buildESP32(
    const std::string& base,
    const std::string& cFile,
    const std::string& exeDir,
    const std::string& fqbn
) {
    namespace fs = std::filesystem;
    std::error_code ec;

    std::string sketchDir = base + "_sketch";
    std::string buildDir = base + "_build";

    fs::remove_all(sketchDir, ec);
    fs::create_directories(sketchDir, ec);
    if (ec) {
        std::cerr << RED << "  error" << RESET
                  << "  cannot create " << sketchDir << ": " << ec.message()
                  << "\n";
        return 1;
    }

    // Arduino requires the primary .ino to share the sketch folder name.
    std::string sketchName = fs::path(sketchDir).filename().string();
    std::string inoPath = sketchDir + "/" + sketchName + ".ino";

    fs::remove(inoPath, ec);
    fs::rename(cFile, inoPath, ec);
    if (ec) {
        // Cross-volume or locked file: fall back to copy + delete.
        ec.clear();
        fs::copy_file(cFile, inoPath, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            std::cerr << RED << "  error" << RESET
                      << "  cannot move " << cFile << " to " << inoPath
                      << ": " << ec.message() << "\n";
            return 1;
        }
        fs::remove(cFile, ec);
    }

    // Vendor the runtime headers into the sketch so `#include
    // "vyne_esp32.h"` resolves without any -I flags.
    std::string runtimeDir = exeDir + "/vyne/runtime/esp32";
    fs::copy_file(
        runtimeDir + "/vyne_esp32.h",
        sketchDir + "/vyne_esp32.h",
        fs::copy_options::overwrite_existing,
        ec
    );
    if (!ec) {
        fs::copy(
            runtimeDir + "/detail",
            sketchDir + "/detail",
            fs::copy_options::recursive | fs::copy_options::overwrite_existing,
            ec
        );
    }
    if (ec) {
        std::cerr << RED << "  error" << RESET
                  << "  cannot stage ESP32 runtime headers: " << ec.message()
                  << "\n";
        return 1;
    }

    sectionOpen("esp32-build");
    std::cout << "    " << CYAN << "fqbn" << RESET << "        " << fqbn
              << "\n";
    std::cout << "    " << CYAN << "sketch" << RESET << "      " << inoPath
              << "\n\n";

    std::string cmd = "arduino-cli compile --fqbn " + fqbn + " --output-dir \"" +
                      buildDir + "\" \"" + sketchDir + "\"";
    std::cout << "    " << CYAN << "running" << RESET << "     " << cmd
              << "\n\n";

    int rc = system(cmd.c_str());
    if (rc != 0) {
        sectionClose();
        std::cerr << RED << "  error" << RESET
                  << "  arduino-cli compile failed (exit " << rc << ")\n\n";
        return 1;
    }

    std::cout << "    " << GREEN << "bin" << RESET << "         " << buildDir
              << "\n";
    sectionClose();
    return 0;
}

static int runESP32Compile(
    const std::string& base,
    const std::string& cFile,
    const BuildOptions& opts
) {
    if (opts.esp32Board.empty()) {
        std::cerr << RED << "  error" << RESET
                  << "  --esp32 requires a board name "
                     "(--esp32 esp32, --esp32 esp32s3, ...)\n";
        return 1;
    }

    struct ESP32Board {
        const char* name;
        const char* fqbn;
    };
    static const ESP32Board boards[] = {
        {"esp32", "esp32:esp32:esp32"},
        {"esp32s2", "esp32:esp32:esp32s2"},
        {"esp32s3", "esp32:esp32:esp32s3"},
        {"esp32c3", "esp32:esp32:esp32c3"},
        {"esp32c6", "esp32:esp32:esp32c6"},
    };
    const ESP32Board* board = nullptr;
    for (const auto& b : boards)
        if (opts.esp32Board == b.name) {
            board = &b;
            break;
        }
    if (!board) {
        std::cerr << RED << "  error" << RESET << "  unknown ESP32 board '"
                  << opts.esp32Board
                  << "'; supported: esp32, esp32s2, esp32s3, esp32c3, esp32c6\n";
        return 1;
    }

    std::string exeDir = FileUtils::getExeDir();
    int rc = buildESP32(base, cFile, exeDir, board->fqbn);
    if (rc != 0)
        return rc;

    std::string resolvedPort =
        opts.esp32Port.empty() ? opts.arduinoPort : opts.esp32Port;

    if (opts.esp32Upload) {
        sectionOpen("flash");
        if (resolvedPort.empty()) {
            resolvedPort = promptPort();
            if (resolvedPort.empty()) {
                sectionClose();
                return 1;
            }
        }
        std::string cmd = "arduino-cli upload -p " + resolvedPort + " --fqbn " +
                          board->fqbn + " --input-dir \"" + base + "_build\"";
        std::cout << "    " << CYAN << "running" << RESET << "     " << cmd
                  << "\n\n";
        rc = system(cmd.c_str());
        sectionClose();
        if (rc != 0) {
            std::cerr << RED << "  error" << RESET
                      << "  arduino-cli upload failed (exit " << rc << ")\n\n";
            return 1;
        }
    }

    if (opts.esp32Monitor) {
        int baud = opts.esp32Baud > 0 ? opts.esp32Baud : opts.baudRate;
        rc = doArduinoMonitor(resolvedPort, baud);
        if (rc != 0)
            return rc;
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

    if (!opts.quiet) {
        std::cout << "\n";
        std::cout << BOLD << "  Compiling " << RESET << filename << "\n\n";
    }

    // ---------------- Phase 1: Vyne source -> C source ----------------
    auto start_transpile = std::chrono::high_resolution_clock::now();

    VyneLinker linker;
    auto units = linker.link(filename);

    C_Emitter emitter;
    emitter.reset();
    emitter.setScratchBoundsEnabled(opts.scratchBounds);
    emitter.setTarget(
        opts.mode == RunMode::ArduinoCompile || opts.mode == RunMode::ArduinoRun
            ? Target::AVR
        : opts.mode == RunMode::ESP32Compile || opts.mode == RunMode::ESP32Run
            ? Target::ESP32
            : Target::Desktop
    );

    // Project-level build policy. These become defines in the emitted
    // translation unit; they do not change AST lowering, only the metadata
    // available to the runtime header.
    if (opts.hasProject) {
        emitter.setDeterministicBuild(opts.project.deterministic);
        emitter.setCudaSmVersion(opts.project.cuda_sm);
    }

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

    if (!opts.quiet) {
        std::cout << GREEN << "  transpile" << RESET << "  " << lineCount
                  << " lines  " << tokenCount << " tokens  " << std::fixed
                  << std::setprecision(2) << transpile_ms.count() << "ms\n";
    }

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

    // ---------------- ESP32 path: arduino-cli, not avr-gcc --------------
    if (opts.mode == RunMode::ESP32Compile || opts.mode == RunMode::ESP32Run) {
        return runESP32Compile(base, cFile, opts);
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
        // std::rename fails on Windows when the destination exists, so
        // remove the previous .ino.cpp first; otherwise avr-gcc silently
        // compiles a stale translation unit.
        std::string ardCpp = base + ".ino.cpp";
        {
            std::error_code rec;
            std::filesystem::remove(ardCpp, rec);
            std::filesystem::rename(cFile, ardCpp, rec);
            if (rec) {
                std::cerr << "\n"
                          << RED << "  error" << RESET
                          << "  cannot move " << cFile << " to " << ardCpp
                          << ": " << rec.message() << "\n\n";
                return 1;
            }
        }

        std::string variant =
            (std::string(board->name) == "mega2560") ? "mega" : "standard";
        bool emitHex = (opts.mode == RunMode::ArduinoRun);

        int rc = buildArduino(
            base,
            ardCpp,
            exeDir,
            board->mcu,
            variant,
            emitHex,
            board->fqbn,
            board->name
        );
        if (rc != 0)
            return rc;

        // --flash / --monitor are post-build steps. They only make sense
        // when a .hex was actually emitted; --flash / --monitor both set
        // RunMode::ArduinoRun, so emitHex is true by construction.
        std::string resolvedPort = opts.arduinoPort;

        if (opts.arduinoUpload) {
            rc = doArduinoUpload(base, board->fqbn, resolvedPort);
            if (rc != 0)
                return rc;
        }

        if (opts.arduinoMonitor) {
            rc = doArduinoMonitor(resolvedPort, opts.baudRate);
            if (rc != 0)
                return rc;
        }

        return 0;
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

    if (!opts.quiet) {
        std::cout << GREEN << "  compile  " << RESET << "  gcc -O3"
                  << (opts.nativeIsa ? " -march=native" : "") << "  "
                  << std::fixed << std::setprecision(2) << compile_ms.count()
                  << "ms\n";
    }

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
    if (!opts.quiet) {
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
        vprintln(
            "{} {} {}total      {:.2f}ms",
            YELLOW,
            "  -",
            BOLD,
            total_ms
        );

        Vyne::DiagnosticEngine::printSummary();
    }
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

    // Project banner is part of the compilation phase, before any source
    // is read or parsed. It is only present for `vynec build` / `vynec run`
    // with a discovered project.toml. The benchmark driver prints its own
    // banner and compiles quietly.
    if (opts.hasProject && !opts.quiet)
        printProjectBox(opts.project);

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
        resetContractMode();
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
