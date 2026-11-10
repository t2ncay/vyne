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

#include "init.h"

#include "template_engine.h"
#include "tui.h"
#include "../vyne/utils/file_utils.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#    include <io.h>
#    define VYNE_ISATTY _isatty
#    define VYNE_FILENO _fileno
#else
#    include <unistd.h>
#    define VYNE_ISATTY isatty
#    define VYNE_FILENO fileno
#endif

namespace fs = std::filesystem;
using Vyne::Template::Context;
using Vyne::Template::Engine;
using Vyne::Template::trim;

#define INIT_RESET "\033[0m"
#define INIT_BOLD "\033[1m"
#define INIT_RED "\033[31m"
#define INIT_GREEN "\033[32m"
#define INIT_YELLOW "\033[33m"
#define INIT_CYAN "\033[36m"
#define INIT_MAGENTA "\033[35m"

namespace {

constexpr const char* VYNE_VERSION = "0.7.0";

#ifdef _WIN32
constexpr const char* NULL_SINK = " > NUL 2>&1";
#else
constexpr const char* NULL_SINK = " > /dev/null 2>&1";
#endif

struct Options {
    std::string path = ".";
    bool pathSet = false;

    std::string name;
    bool haveName = false;

    std::string kind;
    bool haveKind = false;

    std::string targets;
    bool haveTargets = false;

    std::string libraries;
    bool haveLibraries = false;

    std::string cuda;
    bool haveCuda = false;

    std::string license = "AGPL-3.0-only";
    bool haveLicense = false;

    bool yes = false;
    bool noTui = false;
    bool noGit = false;

    bool hooks = false;
    bool docker = false;
    bool paper = false;
    bool selfTest = false;

    std::string pretrained;
};

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    return s;
}

std::string quote(const std::string& s) {
    return "\"" + s + "\"";
}

int runCommand(const std::string& command) {
    return std::system(command.c_str());
}

std::string captureCommand(const std::string& command) {
#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (!pipe)
        return "";

    std::string out;
    char buf[4096];
    while (std::fgets(buf, sizeof(buf), pipe) != nullptr)
        out += buf;

#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return out;
}

bool stdoutIsTty() {
    return VYNE_ISATTY(VYNE_FILENO(stdout)) != 0;
}

bool validName(const std::string& name) {
    if (name.empty())
        return false;
    auto isStart = [](unsigned char c) {
        return std::isalpha(c) || c == '_';
    };
    auto isRest = [](unsigned char c) {
        return std::isalnum(c) || c == '_' || c == '-';
    };
    if (!isStart((unsigned char)name[0]))
        return false;
    for (char c : name) {
        if (!isRest((unsigned char)c))
            return false;
    }
    return true;
}

std::vector<std::string> splitCsv(const std::string& text) {
    std::vector<std::string> out;
    std::string current;

    auto flush = [&]() {
        std::string value = trim(current);
        current.clear();
        if (value.empty())
            return;
        if (std::find(out.begin(), out.end(), value) == out.end())
            out.push_back(value);
    };

    for (char c : text) {
        if (c == ',')
            flush();
        else
            current += c;
    }
    flush();
    return out;
}

std::string join(const std::vector<std::string>& values,
                 const std::string& sep) {
    std::string out;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i)
            out += sep;
        out += values[i];
    }
    return out;
}

bool isBinaryAsset(const fs::path& path) {
    static const char* extensions[] = {
        ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".tga",
        ".vwy", ".vwt", ".zip", ".gz", ".pdf",
        ".ttf", ".otf", ".ico", ".woff", ".woff2",
    };
    std::string ext = lower(path.extension().string());
    for (const char* candidate : extensions) {
        if (ext == candidate)
            return true;
    }
    return false;
}

bool knownTarget(const std::string& name) {
    return name == "desktop" || name == "arduino-uno" ||
           name == "coral" || name == "jetson";
}

bool knownLibrary(const std::string& name) {
    static const char* names[] = {
        "vlin", "vml", "vstat", "vtermplot", "vfft",
        "vcomplex", "vcsv", "vimage", "vconv",
    };
    for (const char* n : names) {
        if (name == n)
            return true;
    }
    return false;
}

std::vector<std::string> defaultTargets(const std::string& kind) {
    if (kind == "ml")
        return {"desktop", "coral"};
    if (kind == "embedded")
        return {"arduino-uno"};
    if (kind == "scientific")
        return {"desktop"};
    if (kind == "sr")
        return {"desktop", "jetson"};
    if (kind == "mixed")
        return {"desktop", "arduino-uno", "coral", "jetson"};
    return {"desktop"};
}

std::vector<std::string> defaultLibraries(const std::string& kind) {
    if (kind == "ml")
        return {"vlin", "vml", "vstat", "vtermplot"};
    if (kind == "embedded")
        return {};
    if (kind == "scientific")
        return {"vlin", "vfft", "vstat", "vcomplex", "vtermplot"};
    if (kind == "sr")
        return {"vimage", "vconv", "vfft"};
    if (kind == "mixed")
        return {"vlin", "vml", "vstat", "vtermplot", "vfft",
                "vcomplex", "vcsv", "vimage", "vconv"};
    return {};
}

std::string templateKindFor(const std::string& kind) {
    return kind == "mixed" ? "ml" : kind;
}

bool isKnownKind(const std::string& kind) {
    return kind == "blank" || kind == "ml" || kind == "embedded" ||
           kind == "scientific" || kind == "sr" || kind == "mixed";
}

std::string dependencyVersion(const std::string& name) {
    if (name == "vstat")
        return "0.2";
    return "0.4";
}

std::string tomlEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\\')
            out += "\\\\";
        else if (c == '"')
            out += "\\\"";
        else
            out += c;
    }
    return out;
}

int currentYear() {
    std::time_t now = std::time(nullptr);
    std::tm* local = std::localtime(&now);
    if (!local)
        return 2026;
    return local->tm_year + 1900;
}

std::string gitConfigValue(const std::string& key) {
    std::string out =
        captureCommand("git config --get " + key + NULL_SINK);
    return trim(out);
}

std::string defaultAuthor() {
    std::string name = gitConfigValue("user.name");
    std::string email = gitConfigValue("user.email");
    if (name.empty())
        name = "Your Name";
    if (email.empty())
        email = "you@example.com";
    return name + " <" + email + ">";
}

std::string sanitizeProjectName(std::string raw) {
    std::string out;
    for (char c : raw) {
        if (std::isalnum((unsigned char)c) || c == '_' || c == '-')
            out += c;
        else
            out += '_';
    }
    if (out.empty())
        out = "vyne_project";
    if (std::isdigit((unsigned char)out[0]) || out[0] == '-')
        out = "_" + out;
    return out;
}

std::string projectNameFromPath(const fs::path& root) {
    fs::path abs = fs::absolute(root);
    std::string name = abs.filename().string();
    if (name.empty() || name == ".")
        name = abs.parent_path().filename().string();
    if (name.empty() || name == ".")
        name = "vyne_project";
    return sanitizeProjectName(name);
}

bool readTextFile(const fs::path& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open())
        return false;
    std::stringstream buffer;
    buffer << in.rdbuf();
    out = buffer.str();
    return true;
}

bool writeTextFile(const fs::path& path, const std::string& content) {
    std::error_code ec;
    if (!path.parent_path().empty()) {
        fs::create_directories(path.parent_path(), ec);
        if (ec)
            return false;
    }

    std::ofstream out(path, std::ios::binary);
    if (!out.is_open())
        return false;
    out.write(content.data(), (std::streamsize)content.size());
    return out.good();
}

bool writeIfAbsent(const fs::path& path,
                   const std::string& content,
                   bool& skipped,
                   std::string& error) {
    std::error_code ec;
    if (fs::exists(path, ec)) {
        skipped = true;
        return true;
    }
    skipped = false;
    if (!writeTextFile(path, content)) {
        error = "could not write " + path.string();
        return false;
    }
    return true;
}

bool gitAvailable() {
    return runCommand("git --version" + std::string(NULL_SINK)) == 0;
}

bool insideRepo(const fs::path& root) {
    return runCommand(
               "git -C " + quote(root.string()) +
               " rev-parse --is-inside-work-tree" + NULL_SINK
           ) == 0;
}

bool repoClean(const fs::path& root) {
    std::string out = captureCommand(
        "git -C " + quote(root.string()) + " status --porcelain" + NULL_SINK
    );
    return trim(out).empty();
}

std::string buildProjectToml(
    const std::string& name,
    const std::string& version,
    const std::string& author,
    const std::string& license,
    const std::string& description,
    const std::string& kind,
    const std::vector<std::string>& targets,
    const std::vector<std::string>& libraries,
    bool cuda,
    const std::string& defaultTarget
) {
    std::ostringstream out;

    out << "[project]\n";
    out << "name = \"" << tomlEscape(name) << "\"\n";
    out << "version = \"" << tomlEscape(version) << "\"\n";
    out << "authors = [\"" << tomlEscape(author) << "\"]\n";
    out << "license = \"" << tomlEscape(license) << "\"\n";
    out << "description = \"" << tomlEscape(description) << "\"\n";
    out << "repository = \"\"\n";
    out << "keywords = []\n";

    bool anyDevice = cuda;
    bool cudaSection = cuda;
    for (const auto& target : targets) {
        if (target == "coral" || target == "jetson")
            anyDevice = true;
        if (target == "jetson")
            cudaSection = true;
    }

    out << "\n[toolchain]\n";
    out << "vyne = \"" << VYNE_VERSION << "\"\n";
    if (anyDevice)
        out << "ggml = \"0.9.4\"\n";

    out << "\n[dependencies]\n";
    for (const auto& lib : libraries) {
        out << lib << " = \"" << dependencyVersion(lib) << "\"\n";
    }

    out << "\n[targets]\n";
    out << "default = \"" << tomlEscape(defaultTarget) << "\"\n";

    for (const auto& target : targets) {
        out << "\n[targets." << target << "]\n";
        if (target == "desktop") {
            out << "backend = \"c\"\n";
        } else if (target == "arduino-uno") {
            out << "backend = \"avr\"\n";
            out << "board = \"arduino:avr:uno\"\n";
        } else if (target == "coral") {
            out << "backend = \"c\"\n";
            out << "device = \"coral\"\n";
            out << "quantize = \"int8\"\n";
        } else if (target == "jetson") {
            out << "backend = \"c\"\n";
            out << "device = \"cuda\"\n";
        }
    }

    out << "\n[build]\n";
    out << "default_targets = [";
    for (size_t i = 0; i < targets.size(); ++i) {
        if (i)
            out << ", ";
        out << "\"" << targets[i] << "\"";
    }
    out << "]\n";
    out << "deterministic = true\n";

    out << "\n[test]\n";
    out << "discovery = \"tests/*.vy\"\n";
    out << "snapshot_format = \"json\"\n";

    out << "\n[bench]\n";
    out << "warmup = 10\n";
    out << "iterations = 100\n";
    out << "report = \"build/bench.json\"\n";

    if (templateKindFor(kind) == "ml") {
        out << "\n[data]\n";
        out << "iris = { url = "
               "\"https://archive.ics.uci.edu/ml/machine-learning-databases/"
               "iris/iris.data\", "
               "sha256 = "
               "\"6f608b71a7317216319b4d27b4d9bc84e6abd734eda7872b71a458569e2656c0\" "
               "}\n";
    }

    if (cudaSection) {
        out << "\n[device.cuda]\n";
        out << "sm_version = \"86\"\n";
    }

    return out.str();
}

void printInitHelp() {
    std::cout << "\n  " << INIT_BOLD << INIT_CYAN << "vynec init"
              << INIT_RESET << "  scaffold a new Vyne project\n\n";

    std::cout << "  " << INIT_CYAN << "USAGE" << INIT_RESET << "\n";
    std::cout << "    vynec init [PATH] [options]\n\n";

    std::cout << "  " << INIT_CYAN << "OPTIONS" << INIT_RESET << "\n";
    std::cout << "    --name <name>        project name (default: basename of PATH)\n";
    std::cout << "    --kind <kind>        blank | ml | embedded | scientific | sr | mixed\n";
    std::cout << "    --targets <list>     desktop, arduino-uno, coral, jetson\n";
    std::cout << "    --libraries <list>   comma-separated library names\n";
    std::cout << "    --cuda <yes|no>      add [device.cuda] when yes\n";
    std::cout << "    --license <id>       AGPL-3.0-only | MIT | proprietary\n";
    std::cout << "    --yes                accept defaults without prompting\n";
    std::cout << "    --no-tui             force non-interactive mode\n";
    std::cout << "    --no-git             skip git init and the initial commit\n";
    std::cout << "    -h, --help           show this help\n\n";

    std::cout << "  " << INIT_CYAN << "TEMPLATES" << INIT_RESET << "\n";
    std::cout << "    blank        @bounded + @deterministic starter\n";
    std::cout << "    ml           Iris MLP, Adam, vtermplot loss curve\n";
    std::cout << "    embedded     Arduino Uno blinky with @bounded\n";
    std::cout << "    scientific   Lorenz RK4 + Lyapunov estimate\n";
    std::cout << "    sr           ESPCN 2x upscaling with vimage + vconv\n";
    std::cout << "    mixed        ML template with every library\n\n";

    std::cout << "  " << INIT_CYAN << "DEVELOPER" << INIT_RESET << "\n";
    std::cout << "    --self-test          run the template-engine checks and exit\n\n";

    std::cout << "  Run without --name on a terminal to open the interactive\n";
    std::cout << "  questionnaire (kind, targets, libraries, CUDA, license).\n";
    std::cout << "  --name, --yes, or --no-tui make it non-interactive. A\n";
    std::cout << "  non-TTY shell or a terminal smaller than 80x24 exits with 2.\n\n";
}

int matchValue(int& i,
               int argc,
               char** argv,
               const std::string& arg,
               const std::string& flag,
               std::string& out) {
    if (arg == flag) {
        if (i + 1 >= argc)
            return -1;
        out = argv[++i];
        return 1;
    }
    if (arg.rfind(flag + "=", 0) == 0) {
        out = arg.substr(flag.size() + 1);
        return 1;
    }
    return 0;
}

} // namespace

int runInit(int argc, char** argv) {
    Options opts;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printInitHelp();
            return 0;
        }
        if (arg == "--self-test") {
            opts.selfTest = true;
            continue;
        }
        if (arg == "--yes") {
            opts.yes = true;
            continue;
        }
        if (arg == "--no-tui") {
            opts.noTui = true;
            continue;
        }
        if (arg == "--no-git") {
            opts.noGit = true;
            continue;
        }
        if (arg == "--hooks") {
            opts.hooks = true;
            continue;
        }
        if (arg == "--docker") {
            opts.docker = true;
            continue;
        }
        if (arg == "--paper") {
            opts.paper = true;
            continue;
        }

        int matched = 0;
        if ((matched = matchValue(i, argc, argv, arg, "--name", opts.name))) {
            if (matched < 0) {
                std::cerr << INIT_RED << "  error" << INIT_RESET
                          << "  --name needs a value\n";
                return 4;
            }
            opts.haveName = true;
            continue;
        }
        if ((matched = matchValue(i, argc, argv, arg, "--kind", opts.kind))) {
            if (matched < 0) {
                std::cerr << INIT_RED << "  error" << INIT_RESET
                          << "  --kind needs a value\n";
                return 4;
            }
            opts.haveKind = true;
            continue;
        }
        if ((matched = matchValue(
                 i, argc, argv, arg, "--targets", opts.targets
             ))) {
            if (matched < 0) {
                std::cerr << INIT_RED << "  error" << INIT_RESET
                          << "  --targets needs a value\n";
                return 4;
            }
            opts.haveTargets = true;
            continue;
        }
        if ((matched = matchValue(
                 i, argc, argv, arg, "--libraries", opts.libraries
             ))) {
            if (matched < 0) {
                std::cerr << INIT_RED << "  error" << INIT_RESET
                          << "  --libraries needs a value\n";
                return 4;
            }
            opts.haveLibraries = true;
            continue;
        }
        if ((matched = matchValue(i, argc, argv, arg, "--cuda", opts.cuda))) {
            if (matched < 0) {
                std::cerr << INIT_RED << "  error" << INIT_RESET
                          << "  --cuda needs a value\n";
                return 4;
            }
            opts.haveCuda = true;
            continue;
        }
        if ((matched = matchValue(
                 i, argc, argv, arg, "--license", opts.license
             ))) {
            if (matched < 0) {
                std::cerr << INIT_RED << "  error" << INIT_RESET
                          << "  --license needs a value\n";
                return 4;
            }
            opts.haveLicense = true;
            continue;
        }
        if ((matched = matchValue(
                 i, argc, argv, arg, "--pretrained", opts.pretrained
             ))) {
            if (matched < 0) {
                std::cerr << INIT_RED << "  error" << INIT_RESET
                          << "  --pretrained needs a value\n";
                return 4;
            }
            continue;
        }

        if (!arg.empty() && arg[0] == '-') {
            std::cerr << INIT_RED << "  error" << INIT_RESET
                      << "  unknown option '" << arg << "'\n";
            return 4;
        }

        if (opts.pathSet) {
            std::cerr << INIT_RED << "  error" << INIT_RESET
                      << "  unexpected argument '" << arg << "'\n";
            return 4;
        }
        opts.path = arg;
        opts.pathSet = true;
    }

    if (opts.selfTest) {
        std::string error;
        if (Engine::selfTest(error)) {
            std::cout << INIT_GREEN << "✓ template engine self-test passed"
                      << INIT_RESET << "\n";
            return 0;
        }
        std::cerr << INIT_RED << "  template engine self-test failed: "
                  << error << INIT_RESET << "\n";
        return 1;
    }

    if (opts.hooks || opts.docker || opts.paper || !opts.pretrained.empty()) {
        std::cerr << INIT_RED << "  error" << INIT_RESET
                  << "  --hooks, --docker, --paper, and --pretrained land in "
                     "Milestone 7\n";
        return 4;
    }

    if (!opts.haveKind)
        opts.kind = "blank";

    if (!isKnownKind(opts.kind)) {
        std::cerr << INIT_RED << "  error" << INIT_RESET
                  << "  unknown kind '" << opts.kind
                  << "'; use blank, ml, embedded, scientific, sr, or mixed\n";
        return 4;
    }

    std::string license = lower(opts.license);
    if (license == "agpl" || license == "agpl-3.0" ||
        license == "agpl-3.0-only") {
        opts.license = "AGPL-3.0-only";
    } else if (license == "mit") {
        opts.license = "MIT";
    } else if (license == "proprietary") {
        opts.license = "proprietary";
    } else {
        std::cerr << INIT_RED << "  error" << INIT_RESET
                  << "  unknown license '" << opts.license
                  << "'; use AGPL-3.0-only, MIT, or proprietary\n";
        return 4;
    }

    bool cuda = false;
    if (opts.haveCuda) {
        std::string value = lower(opts.cuda);
        if (value == "yes" || value == "true" || value == "1") {
            cuda = true;
        } else if (value == "no" || value == "false" || value == "0") {
            cuda = false;
        } else {
            std::cerr << INIT_RED << "  error" << INIT_RESET
                      << "  --cuda expects yes or no\n";
            return 4;
        }
    }

    if (opts.kind == "embedded" && cuda) {
        std::cerr << INIT_RED << "  error" << INIT_RESET
                  << "  embedded projects do not use CUDA targets\n";
        return 4;
    }

    bool tuiWanted = !opts.haveName && !opts.yes && !opts.noTui &&
                     stdoutIsTty() &&
                     !(opts.haveKind && opts.kind == "blank");

    bool tuiRan = false;
    if (tuiWanted) {
        std::string derived = projectNameFromPath(opts.path);
        if (!validName(derived)) {
            std::cerr << INIT_RED << "  error" << INIT_RESET
                      << "  could not derive a valid project name from '"
                      << opts.path << "'; pass --name\n";
            return 4;
        }

        fs::path share =
            fs::path(FileUtils::getExeDir()) / "share" / "vyne";

        InitChoices choices;
        choices.kind = opts.haveKind ? opts.kind : "ml";
        choices.targets = opts.haveTargets
                              ? splitCsv(opts.targets)
                              : std::vector<std::string>{};
        choices.libraries = opts.haveLibraries
                                ? splitCsv(opts.libraries)
                                : std::vector<std::string>{};
        choices.cuda = cuda;
        choices.license = opts.license;

        std::set<std::string> locked;
        if (opts.haveKind)
            locked.insert("kind");
        if (opts.haveTargets)
            locked.insert("targets");
        if (opts.haveLibraries)
            locked.insert("libraries");
        if (opts.haveCuda)
            locked.insert("cuda");
        if (opts.haveLicense)
            locked.insert("license");

        std::string tuiError;
        bool cancelled = false;
        bool confirmed = runQuestionnaire(
            choices,
            share,
            derived,
            locked,
            tuiError,
            cancelled
        );

        if (cancelled) {
            std::cout << "\n  " << INIT_YELLOW << "cancelled"
                      << INIT_RESET << "\n\n";
            return 1;
        }
        if (!confirmed) {
            std::cerr << INIT_RED << "  error" << INIT_RESET << "  "
                      << tuiError << "\n";
            return 2;
        }

        opts.kind = choices.kind;
        opts.haveKind = true;
        opts.targets = join(choices.targets, ",");
        opts.haveTargets = true;
        opts.libraries = join(choices.libraries, ",");
        opts.haveLibraries = true;
        opts.cuda = choices.cuda ? "yes" : "no";
        opts.haveCuda = true;
        opts.license = choices.license;
        opts.haveLicense = true;
        cuda = choices.cuda;
        tuiRan = true;
    }

    bool implicitNonInteractive = opts.haveKind && opts.kind == "blank";

    std::string name = opts.name;
    if (!opts.haveName) {
        if (!tuiRan && !opts.yes && !implicitNonInteractive) {
            if (!stdoutIsTty()) {
                std::cerr << INIT_RED << "  error" << INIT_RESET
                          << "  non-interactive environment detected; use "
                             "--name (and optionally --kind)\n";
            } else {
                std::cerr << INIT_RED << "  error" << INIT_RESET
                          << "  --no-tui requires --name\n";
            }
            return 2;
        }
        name = projectNameFromPath(opts.path);
        if (!validName(name)) {
            std::cerr << INIT_RED << "  error" << INIT_RESET
                      << "  could not derive a valid project name from '"
                      << opts.path << "'; pass --name\n";
            return 4;
        }
    }

    if (!validName(name)) {
        std::cerr << INIT_RED << "  error" << INIT_RESET
                  << "  invalid project name '" << name
                  << "'; use letters, digits, '_' and '-' (must not start "
                     "with a digit)\n";
        return 4;
    }

    std::vector<std::string> targets =
        opts.haveTargets ? splitCsv(opts.targets) : defaultTargets(opts.kind);
    if (targets.empty()) {
        std::cerr << INIT_RED << "  error" << INIT_RESET
                  << "  at least one target is required\n";
        return 4;
    }
    for (const auto& target : targets) {
        if (!knownTarget(target)) {
            std::cerr << INIT_RED << "  error" << INIT_RESET
                      << "  unknown target '" << target << "'\n";
            return 4;
        }
    }

    std::vector<std::string> libraries = opts.haveLibraries
                                             ? splitCsv(opts.libraries)
                                             : defaultLibraries(opts.kind);
    for (const auto& lib : libraries) {
        if (!knownLibrary(lib)) {
            std::cerr << INIT_RED << "  error" << INIT_RESET
                      << "  unknown library '" << lib << "'\n";
            return 4;
        }
    }

    std::string defaultTarget = "desktop";
    if (std::find(targets.begin(), targets.end(), "desktop") !=
        targets.end()) {
        defaultTarget = "desktop";
    } else {
        defaultTarget = targets.front();
    }

    fs::path root(opts.path);
    std::error_code ec;
    fs::create_directories(root, ec);
    if (ec && !fs::exists(root)) {
        std::cerr << INIT_RED << "  error" << INIT_RESET
                  << "  could not create directory '" << root.string()
                  << "': " << ec.message() << "\n";
        return 4;
    }
    if (fs::is_regular_file(root, ec)) {
        std::cerr << INIT_RED << "  error" << INIT_RESET
                  << "  '" << root.string() << "' is a file, not a directory\n";
        return 4;
    }

    if (fs::exists(root / "project.toml", ec)) {
        std::cerr << INIT_RED << "  error" << INIT_RESET
                  << "  project.toml already exists in " << root.string()
                  << "; refusing to overwrite\n";
        return 3;
    }

    fs::path share = fs::path(FileUtils::getExeDir()) / "share" / "vyne";
    fs::path templateDir = share / "templates" / templateKindFor(opts.kind);
    if (!fs::is_directory(templateDir, ec)) {
        std::cerr << INIT_RED << "  error" << INIT_RESET
                  << "  template directory not found: " << templateDir.string()
                  << " (installation incomplete)\n";
        return 4;
    }

    bool gitUsable = !opts.noGit && gitAvailable();
    bool wasRepo = gitUsable && insideRepo(root);
    bool wasClean = wasRepo && repoClean(root);

    std::string author = defaultAuthor();
    std::string version = "0.1.0";
    std::string description = "A Vyne project";
    std::string licenseForTemplate =
        opts.license == "proprietary" ? "" : opts.license;

    Context ctx;
    ctx.vars["name"] = name;
    ctx.vars["kind"] = opts.kind;
    ctx.vars["version"] = version;
    ctx.vars["vyne_version"] = VYNE_VERSION;
    ctx.vars["author"] = author;
    ctx.vars["year"] = std::to_string(currentYear());
    ctx.vars["license"] = opts.license;
    ctx.vars["license_spdx"] = licenseForTemplate;
    ctx.vars["target"] = defaultTarget;
    ctx.vars["targets_list"] = join(targets, ", ");
    ctx.vars["library_list"] = join(libraries, ", ");
    ctx.vars["description"] = description;
    ctx.vars["cuda"] = cuda ? "1" : "0";
    ctx.vars["cpu"] = cuda ? "0" : "1";
    ctx.vars["device_attr"] = cuda ? "cuda" : "cpu";
    ctx.vars["proprietary"] =
        opts.license == "proprietary" ? "1" : "0";
    ctx.lists["targets"] = targets;
    ctx.lists["libraries"] = libraries;

    std::cout << "\n  " << INIT_BOLD << INIT_CYAN << "vynec init"
              << INIT_RESET << "\n\n";

    size_t created = 0;
    size_t skippedCount = 0;

    std::string toml = buildProjectToml(
        name,
        version,
        author,
        opts.license,
        description,
        opts.kind,
        targets,
        libraries,
        cuda,
        defaultTarget
    );

    {
        bool skipped = false;
        std::string error;
        if (!writeIfAbsent(root / "project.toml", toml, skipped, error)) {
            std::cerr << INIT_RED << "  error" << INIT_RESET << "  " << error
                      << "\n";
            return 4;
        }
        std::cout << "  " << INIT_GREEN << "created" << INIT_RESET
                  << "  project.toml\n";
        ++created;
    }

    if (opts.license == "proprietary") {
        std::string content =
            "Copyright (c) {{year}} {{author}}\n"
            "All rights reserved.\n\n"
            "Replace this file with your proprietary license terms.\n";
        std::string rendered;
        std::string error;
        if (!Engine::render(content, ctx, rendered, error)) {
            std::cerr << INIT_RED << "  error" << INIT_RESET
                      << "  license header: " << error << "\n";
            return 4;
        }
        bool skipped = false;
        if (!writeIfAbsent(
                root / "LICENSE_HEADER.txt", rendered, skipped, error
            )) {
            std::cerr << INIT_RED << "  error" << INIT_RESET << "  " << error
                      << "\n";
            return 4;
        }
        std::cout << "  " << (skipped ? INIT_YELLOW : INIT_GREEN)
                  << (skipped ? "skipped" : "created") << INIT_RESET << "  "
                  << "LICENSE_HEADER.txt"
                  << (skipped ? " (exists)" : "") << "\n";
        if (skipped)
            ++skippedCount;
        else
            ++created;
    } else {
        fs::path licensePath =
            share / "licenses" / (opts.license + ".license");
        std::string licenseText;
        if (!readTextFile(licensePath, licenseText)) {
            std::cerr << INIT_RED << "  error" << INIT_RESET
                      << "  license text not found: " << licensePath.string()
                      << " (installation incomplete)\n";
            return 4;
        }

        std::string rendered;
        std::string error;
        if (!Engine::render(licenseText, ctx, rendered, error)) {
            std::cerr << INIT_RED << "  error" << INIT_RESET
                      << "  license: " << error << "\n";
            return 4;
        }
        bool skipped = false;
        if (!writeIfAbsent(root / "LICENSE", rendered, skipped, error)) {
            std::cerr << INIT_RED << "  error" << INIT_RESET << "  " << error
                      << "\n";
            return 4;
        }
        std::cout << "  " << (skipped ? INIT_YELLOW : INIT_GREEN)
                  << (skipped ? "skipped" : "created") << INIT_RESET << "  "
                  << "LICENSE" << (skipped ? " (exists)" : "") << "\n";
        if (skipped)
            ++skippedCount;
        else
            ++created;
    }

    std::vector<fs::path> templateFiles;
    for (const auto& entry :
         fs::recursive_directory_iterator(templateDir, ec)) {
        if (ec)
            break;
        if (!entry.is_regular_file())
            continue;
        templateFiles.push_back(entry.path());
    }
    std::sort(templateFiles.begin(), templateFiles.end());

    if (ec) {
        std::cerr << INIT_RED << "  error" << INIT_RESET
                  << "  could not scan " << templateDir.string() << ": "
                  << ec.message() << "\n";
        return 4;
    }

    for (const auto& source : templateFiles) {
        fs::path relative = fs::relative(source, templateDir);
        fs::path destination = root / relative;

        std::string sourceText;
        if (!readTextFile(source, sourceText)) {
            std::cerr << INIT_RED << "  error" << INIT_RESET
                      << "  could not read template " << source.string() << "\n";
            return 4;
        }

        std::string rendered;
        std::string error;
        if (isBinaryAsset(relative)) {
            rendered = std::move(sourceText);
        } else if (!Engine::render(sourceText, ctx, rendered, error)) {
            std::cerr << INIT_RED << "  error" << INIT_RESET << "  "
                      << relative.generic_string() << ": " << error << "\n";
            return 4;
        }

        bool skipped = false;
        if (!writeIfAbsent(destination, rendered, skipped, error)) {
            std::cerr << INIT_RED << "  error" << INIT_RESET << "  " << error
                      << "\n";
            return 4;
        }

        if (skipped) {
            ++skippedCount;
            std::cout << "  " << INIT_YELLOW << "skipped" << INIT_RESET
                      << "  " << relative.generic_string() << " (exists)\n";
        } else {
            ++created;
            std::cout << "  " << INIT_GREEN << "created" << INIT_RESET
                      << "  " << relative.generic_string() << "\n";
        }
    }

    if (gitUsable) {
        if (wasRepo && !wasClean) {
            std::cout << "\n  " << INIT_YELLOW << "git" << INIT_RESET
                      << "  repository has uncommitted changes; skipping "
                         "initial commit\n";
        } else {
            bool ready = wasRepo;
            if (!wasRepo) {
                int rc = runCommand(
                    "git -C " + quote(root.string()) + " init"
                );
                ready = (rc == 0);
                if (!ready) {
                    std::cout << "\n  " << INIT_YELLOW << "git" << INIT_RESET
                              << "  git init failed; files were still written\n";
                }
            }
            if (ready) {
                runCommand(
                    "git -C " + quote(root.string()) + " add -A -- ."
                );
                int rc = runCommand(
                    "git -C " + quote(root.string()) +
                    " commit -m \"Initial commit from vynec init\""
                );
                if (rc != 0) {
                    std::cout << "  " << INIT_YELLOW << "git" << INIT_RESET
                              << "  commit skipped (configure user.name and "
                                 "user.email, or commit manually)\n";
                }
            }
        }
    } else if (!opts.noGit) {
        std::cout << "\n  " << INIT_YELLOW << "git" << INIT_RESET
                  << "  git not found; skipping repository setup\n";
    }

    std::string display = opts.path;
    if (display.empty())
        display = ".";

    std::cout << "\n  " << INIT_GREEN << "✓ Created " << display
              << INIT_RESET << "  (" << created << " file"
              << (created == 1 ? "" : "s") << " created";
    if (skippedCount)
        std::cout << ", " << skippedCount << " skipped";
    std::cout << ")\n\n";

    std::cout << "  Next steps:\n";
    if (display != ".")
        std::cout << "    cd " << quote(display) << "\n";
    if (opts.kind == "embedded") {
        std::cout << "    vynec --arduino uno src/main.vy\n";
        std::cout << "    # add --flash --port COM3 to upload\n\n";
    } else {
        std::cout << "    vynec run src/main.vy\n\n";
    }

    return 0;
}
