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

#include "bench.h"

#include "file_handler.h"
#include "project.h"

#include "../vyne/compiler/codegen/linker.h"
#include "../vyne/runtime/diagnostics.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

struct BenchStats {
    double mean = 0.0;
    double median = 0.0;
    double min = 0.0;
    double max = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    double stddev = 0.0;
};

struct BenchResult {
    std::string key;
    BenchStats stats;
};

struct BenchTarget {
    fs::path path;
    std::string key;
};

std::string fixed2(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << value;
    return out.str();
}

std::string jsonEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (unsigned char c : value) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (c < 0x20) {
                char buffer[7];
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                out += buffer;
            } else {
                out += static_cast<char>(c);
            }
            break;
        }
    }
    return out;
}

std::string iso8601UtcNow() {
    std::time_t now =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    std::ostringstream out;
    out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

std::string shellQuote(const std::string& value) {
#ifdef _WIN32
    return "\"" + value + "\"";
#else
    std::string out = "'";
    for (char c : value) {
        if (c == '\'')
            out += "'\\''";
        else
            out += c;
    }
    out += "'";
    return out;
#endif
}

std::string silentRunCommand(const fs::path& exe) {
    std::error_code ec;
    fs::path absolute = fs::absolute(exe, ec);
    std::string path = (ec ? exe : absolute).string();
#ifdef _WIN32
    // Redirect stdin too: a benchmark target that accidentally reads from
    // the console must not hang the measurement loop.
    return shellQuote(path) + " < NUL > NUL 2>&1";
#else
    return shellQuote(path) + " < /dev/null > /dev/null 2>&1";
#endif
}

std::string sanitizeName(std::string value) {
    for (char& c : value) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' &&
            c != '-')
            c = '_';
    }
    return value;
}

// Path relative to `base`, using forward slashes for stable JSON keys and
// cross-platform terminal output.
std::string relativeDisplay(const fs::path& path, const fs::path& base) {
    std::error_code ec;
    fs::path absPath = fs::absolute(path, ec);
    fs::path absBase = fs::absolute(base, ec);
    if (ec)
        return path.generic_string();

    fs::path rel = fs::relative(absPath, absBase, ec);
    if (ec)
        return path.generic_string();

    std::string text = rel.generic_string();
    return text.empty() ? path.filename().string() : text;
}

// True when `path` is a descendant of `root`. `fs::relative` is used
// instead of a string prefix check so `C:\project` never matches
// `C:\project-old`, and different Windows drives fail cleanly.
bool isInsideDirectory(const fs::path& path, const fs::path& root) {
    std::error_code ec;
    fs::path relative = fs::relative(path, root, ec);
    if (ec || relative.empty())
        return false;

    auto first = relative.begin();
    return first == relative.end() || *first != "..";
}

// Standard-library imports live under the compiler installation's
// modules/external or modules/common trees. Those files are read-only
// dependencies and must never be benchmark targets.
bool isStandardLibraryPath(const fs::path& path) {
    std::string normalized = path.lexically_normal().generic_string();
    return normalized.find("/modules/external/") != std::string::npos ||
           normalized.find("/modules/common/") != std::string::npos;
}

BenchStats computeStats(std::vector<double> times) {
    BenchStats stats;
    if (times.empty())
        return stats;

    std::sort(times.begin(), times.end());
    const size_t count = times.size();

    stats.min = times.front();
    stats.max = times.back();

    double sum = std::accumulate(times.begin(), times.end(), 0.0);
    stats.mean = sum / static_cast<double>(count);

    if (count % 2 == 1) {
        stats.median = times[count / 2];
    } else {
        stats.median = (times[count / 2 - 1] + times[count / 2]) / 2.0;
    }

    auto percentile = [&](double p) {
        // Spec: index = (p / 100) * N, clamped to the last sample.
        size_t index = static_cast<size_t>((p / 100.0) * count);
        if (index >= count)
            index = count - 1;
        return times[index];
    };
    stats.p95 = percentile(95.0);
    stats.p99 = percentile(99.0);

    double variance = 0.0;
    for (double time : times) {
        double delta = time - stats.mean;
        variance += delta * delta;
    }
    stats.stddev = std::sqrt(variance / static_cast<double>(count));
    return stats;
}

void benchRow(
    int indent,
    int labelWidth,
    const std::string& label,
    const std::string& value,
    const char* valueColor = GREEN
) {
    std::cout << std::string(static_cast<size_t>(indent), ' ') << CYAN << label
              << RESET;

    int padding = labelWidth - static_cast<int>(label.size());
    if (padding < 0)
        padding = 0;

    std::cout << std::string(static_cast<size_t>(padding), ' ') << valueColor
              << value << RESET << "\n";
}

bool compileTarget(
    const fs::path& source,
    BuildOptions options,
    const fs::path& artifactBase,
    fs::path& exeOut,
    std::string& error
) {
    SymbolContainer env;
    uint32_t globalId = StringPool::instance().intern("global");
    env[globalId] = {};

    Vyne::DiagnosticEngine::clearDiagnostics();

    options.mode = RunMode::CompileOnly;
    options.quiet = true;
    options.outputName = artifactBase.string();
    options.arduinoUpload = false;
    options.arduinoMonitor = false;

    int rc = runFile(source.string(), env, options);
    if (rc != 0) {
        error = "failed to compile `" + source.string() + "`";
        return false;
    }

    exeOut = artifactBase;
#ifdef _WIN32
    exeOut += ".exe";
#endif

    std::error_code ec;
    if (!fs::is_regular_file(exeOut, ec)) {
        error = "compiler did not produce `" + exeOut.string() + "`";
        return false;
    }
    return true;
}

bool measureTarget(
    const fs::path& exe,
    int warmup,
    int iterations,
    std::vector<double>& times,
    std::string& error
) {
    std::string command = silentRunCommand(exe);

    for (int i = 0; i < warmup; ++i) {
        if (std::system(command.c_str()) == -1) {
            error = "could not launch `" + exe.string() + "`";
            return false;
        }
    }

    times.clear();
    times.reserve(static_cast<size_t>(iterations));

    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        int rc = std::system(command.c_str());
        auto end = std::chrono::high_resolution_clock::now();

        if (rc == -1) {
            error = "could not launch `" + exe.string() + "`";
            return false;
        }

        std::chrono::duration<double, std::milli> elapsed = end - start;
        times.push_back(elapsed.count());
    }

    return true;
}

bool writeReport(
    const fs::path& reportPath,
    const std::string& project,
    const std::string& target,
    int warmup,
    int iterations,
    const std::vector<BenchResult>& results,
    std::string& error
) {
    std::error_code ec;
    if (reportPath.has_parent_path())
        fs::create_directories(reportPath.parent_path(), ec);

    std::ofstream out(reportPath);
    if (!out.is_open()) {
        error = "could not write `" + reportPath.string() + "`";
        return false;
    }

    out << std::fixed << std::setprecision(2);
    out << "{\n";
    out << "  \"project\": \"" << jsonEscape(project) << "\",\n";
    out << "  \"target\": \"" << jsonEscape(target) << "\",\n";
    out << "  \"timestamp\": \"" << iso8601UtcNow() << "\",\n";
    out << "  \"config\": {\n";
    out << "    \"warmup\": " << warmup << ",\n";
    out << "    \"iterations\": " << iterations << "\n";
    out << "  },\n";
    out << "  \"results\": {\n";

    for (size_t i = 0; i < results.size(); ++i) {
        const BenchResult& result = results[i];
        out << "    \"" << jsonEscape(result.key) << "\": {\n";
        out << "      \"mean_ms\": " << result.stats.mean << ",\n";
        out << "      \"median_ms\": " << result.stats.median << ",\n";
        out << "      \"min_ms\": " << result.stats.min << ",\n";
        out << "      \"max_ms\": " << result.stats.max << ",\n";
        out << "      \"p95_ms\": " << result.stats.p95 << ",\n";
        out << "      \"p99_ms\": " << result.stats.p99 << ",\n";
        out << "      \"stddev_ms\": " << result.stats.stddev << "\n";
        out << "    }" << (i + 1 < results.size() ? "," : "") << "\n";
    }

    out << "  }\n";
    out << "}\n";

    if (!out.good()) {
        error = "failed while writing `" + reportPath.string() + "`";
        return false;
    }
    return true;
}

void printBenchHelp() {
    std::cout << "\n  " << BOLD << "vynec bench" << RESET
              << "  benchmark Vyne targets\n\n";
    std::cout << "  " << CYAN << "USAGE" << RESET << "\n";
    std::cout << "    vynec bench                 benchmark the project entry "
                 "point + imports\n";
    std::cout << "    vynec bench <file.vy>       benchmark one file\n\n";
    std::cout << "  " << CYAN << "OPTIONS" << RESET << "\n";
    std::cout << "    --warmup <n>       warm-up runs before measuring\n";
    std::cout << "    --iterations <n>   measured runs\n";
    std::cout << "    --report <path>    override the JSON report path\n";
    std::cout << "    -h, --help         show this help\n\n";
}

bool parseInteger(const std::string& value, int& out) {
    try {
        out = std::stoi(value);
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<BenchTarget> collectProjectTargets(
    const fs::path& entry,
    const ProjectConfig& config,
    std::string& error
) {
    std::vector<BenchTarget> targets;
    std::error_code ec;

    fs::path entryCanonical = fs::weakly_canonical(entry, ec);
    if (ec)
        entryCanonical = fs::absolute(entry, ec);

    targets.push_back(
        {entryCanonical, relativeDisplay(entryCanonical, config.root_dir)}
    );

    VyneLinker linker;
    std::vector<VyneLinker::CompileUnit> units;
    try {
        Vyne::DiagnosticEngine::clearDiagnostics();
        units = linker.link(entry.string());
    } catch (const std::exception& ex) {
        error = std::string("could not resolve imports: ") + ex.what();
        return {};
    }

    std::vector<BenchTarget> projectFiles;
    std::set<std::string> seen;

    seen.insert(entryCanonical.string());

    for (const auto& unit : units) {
        // `unit.isExtern` is the parser's own classification; the path
        // checks below additionally catch relative imports that happen to
        // point into a standard-library tree.
        if (unit.isExtern)
            continue;

        fs::path path = fs::weakly_canonical(fs::path(unit.canonicalPath), ec);
        if (ec)
            path = fs::path(unit.canonicalPath);

        if (!isInsideDirectory(path, config.root_dir))
            continue;
        if (isStandardLibraryPath(path))
            continue;

        std::string identity = path.string();
        if (!seen.insert(identity).second)
            continue;

        std::string extension = path.extension().string();
        if (extension != ".vy" && extension != ".vyne")
            continue;

        projectFiles.push_back({path, relativeDisplay(path, config.root_dir)});
    }

    auto byKey = [](const BenchTarget& a, const BenchTarget& b) {
        return a.key < b.key;
    };
    std::sort(projectFiles.begin(), projectFiles.end(), byKey);

    targets.insert(targets.end(), projectFiles.begin(), projectFiles.end());
    return targets;
}

} // namespace

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int runBench(int argc, char** argv) {
    std::string targetArg;
    std::string reportOverride;
    int warmupOverride = -1;
    int iterationsOverride = -1;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printBenchHelp();
            return 0;
        }

        if (arg == "--warmup" && i + 1 < argc) {
            if (!parseInteger(argv[++i], warmupOverride)) {
                std::cerr << RED << "  error" << RESET
                          << "  --warmup requires an integer\n";
                return 1;
            }
        } else if (arg == "--iterations" && i + 1 < argc) {
            if (!parseInteger(argv[++i], iterationsOverride)) {
                std::cerr << RED << "  error" << RESET
                          << "  --iterations requires an integer\n";
                return 1;
            }
        } else if (arg == "--report" && i + 1 < argc) {
            reportOverride = argv[++i];
        } else if (!arg.empty() && arg[0] != '-') {
            if (!targetArg.empty()) {
                std::cerr << RED << "  error" << RESET
                          << "  bench accepts a single file path\n";
                return 1;
            }
            targetArg = arg;
        }
    }

    const bool explicitTarget = !targetArg.empty();
    fs::path entry;

    if (explicitTarget) {
        std::error_code ec;
        entry = fs::absolute(fs::path(targetArg), ec);
        if (ec)
            entry = fs::path(targetArg);

        std::string extension = entry.extension().string();
        if (extension != ".vy" && extension != ".vyne") {
            std::cerr << RED << "  error" << RESET
                      << "  file must end in .vy or .vyne\n";
            return 1;
        }
        if (!fs::is_regular_file(entry, ec)) {
            std::cerr << RED << "  error" << RESET << "  could not open `"
                      << targetArg << "`\n";
            return 1;
        }
    }

    ProjectLoadResult loaded = loadProject(".");
    if (!loaded.found && explicitTarget)
        loaded = loadProject(entry.parent_path().string());

    if (loaded.found && !loaded.ok) {
        std::cerr << RED << "  error" << RESET << "  " << loaded.error << "\n";
        return 1;
    }

    const bool hasProject = loaded.found && loaded.ok;
    ProjectConfig config = loaded.config;

    if (!explicitTarget) {
        if (!hasProject) {
            std::cerr << RED << "  error" << RESET
                      << "  could not find `project.toml`\n";
            return 1;
        }

        std::string entryPath;
        if (!findProjectEntry(config.root_dir, entryPath)) {
            std::cerr << RED << "  error" << RESET
                      << "  could not find `src/main.vy` or `main.vy`\n";
            return 1;
        }
        entry = entryPath;
    }

    int warmup = hasProject ? config.bench_warmup : 10;
    int iterations = hasProject ? config.bench_iterations : 100;
    if (warmupOverride >= 0)
        warmup = warmupOverride;
    if (iterationsOverride >= 0)
        iterations = iterationsOverride;

    if (warmup < 0) {
        std::cerr << RED << "  error" << RESET
                  << "  warmup count cannot be negative\n";
        return 1;
    }
    if (iterations <= 0) {
        std::cerr << RED << "  error" << RESET
                  << "  iteration count must be greater than zero\n";
        return 1;
    }

    // Report path. Project mode honours [bench].report; single-file mode
    // always writes `<stem>_bench.json` next to the source unless the user
    // explicitly overrides it.
    fs::path reportPath;
    std::string reportDisplay;
    if (explicitTarget) {
        reportPath =
            entry.parent_path() / (entry.stem().string() + "_bench.json");
        if (!reportOverride.empty())
            reportPath = fs::absolute(fs::path(reportOverride));
        reportDisplay = relativeDisplay(reportPath, fs::current_path());
    } else {
        std::string configured = config.bench_report.empty()
                                     ? "build/bench.json"
                                     : config.bench_report;
        reportPath = fs::path(configured);
        if (reportPath.is_relative())
            reportPath = fs::path(config.root_dir) / reportPath;
        if (!reportOverride.empty()) {
            reportPath = fs::absolute(fs::path(reportOverride));
            reportDisplay = relativeDisplay(reportPath, fs::current_path());
        } else {
            reportDisplay = configured;
        }
    }

    BenchOptions benchOptions;
    benchOptions.warmup = warmup;
    benchOptions.iterations = iterations;
    benchOptions.reportPath = reportPath.string();

    BuildOptions build;
    build.hasProject = hasProject;
    if (hasProject) {
        build.project = config;
        applyProjectConfig(config, build);

        if (!config.board.empty()) {
            std::cerr << RED << "  error" << RESET
                      << "  bench requires a native target; board targets "
                         "produce firmware\n";
            return 1;
        }
    }

    std::vector<BenchTarget> targets;
    if (explicitTarget) {
        std::error_code ec;
        fs::path canonical = fs::weakly_canonical(entry, ec);
        if (ec)
            canonical = entry;
        fs::path base =
            hasProject ? fs::path(config.root_dir) : fs::current_path();
        targets.push_back({canonical, relativeDisplay(canonical, base)});
    } else {
        std::string error;
        targets = collectProjectTargets(entry, config, error);
        if (!error.empty()) {
            std::cerr << RED << "  error" << RESET << "  " << error << "\n";
            return 1;
        }
    }

    if (targets.empty()) {
        std::cerr << RED << "  error" << RESET << "  no benchmark targets\n";
        return 1;
    }

    fs::path artifactRoot = hasProject ? fs::path(config.root_dir)
                                       : (explicitTarget ? entry.parent_path()
                                                         : fs::current_path());
    fs::path artifactDir = artifactRoot / "build" / "bench";
    std::error_code ec;
    fs::create_directories(artifactDir, ec);
    if (ec) {
        std::cerr << RED << "  error" << RESET
                  << "  could not create benchmark directory `"
                  << artifactDir.string() << "`: " << ec.message() << "\n";
        return 1;
    }

    if (hasProject)
        printProjectBox(config);

    sectionOpen("bench");
    benchRow(4, 13, "warmup", std::to_string(benchOptions.warmup));
    benchRow(4, 13, "iterations", std::to_string(benchOptions.iterations));
    benchRow(4, 13, "report", reportDisplay, YELLOW);

    std::vector<BenchResult> results;
    bool hadError = false;

    for (size_t i = 0; i < targets.size(); ++i) {
        const BenchTarget& target = targets[i];

        std::cout << "\n";
        benchRow(4, 13, "running", target.key);
        std::cout.flush();

        std::string artifactName = "bench_" + std::to_string(i) + "_" +
                                   sanitizeName(target.path.stem().string());
        fs::path artifactBase = artifactDir / artifactName;
        fs::path exe;

        std::string error;
        if (!compileTarget(target.path, build, artifactBase, exe, error)) {
            std::cerr << RED << "  error" << RESET << "  " << error << "\n";
            hadError = true;
            continue;
        }

        std::vector<double> times;
        if (!measureTarget(
                exe,
                benchOptions.warmup,
                benchOptions.iterations,
                times,
                error
            )) {
            std::cerr << RED << "  error" << RESET << "  " << error << "\n";
            hadError = true;
            continue;
        }

        BenchStats stats = computeStats(times);
        results.push_back({target.key, stats});

        benchRow(6, 11, "mean", fixed2(stats.mean) + " ms");
        benchRow(6, 11, "median", fixed2(stats.median) + " ms");
        benchRow(6, 11, "p95", fixed2(stats.p95) + " ms");
        benchRow(
            6,
            11,
            "min/max",
            fixed2(stats.min) + " / " + fixed2(stats.max) + " ms"
        );
        std::cout.flush();
    }

    sectionClose();

    std::string projectName =
        config.name.empty() ? entry.stem().string() : config.name;
    std::string targetName =
        config.default_target.empty() ? "desktop" : config.default_target;

    std::string reportError;
    if (!writeReport(
            reportPath,
            projectName,
            targetName,
            benchOptions.warmup,
            benchOptions.iterations,
            results,
            reportError
        )) {
        std::cerr << RED << "  error" << RESET << "  " << reportError << "\n";
        return 1;
    }

    if (results.empty()) {
        std::cerr << RED << "  error" << RESET
                  << "  no benchmarks completed successfully\n";
        return 1;
    }

    return hadError ? 1 : 0;
}
