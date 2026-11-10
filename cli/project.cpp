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

#include "project.h"

#include "../vyne/utils/file_utils.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;

std::string_view trimView(std::string_view s) {
    size_t begin = 0;
    while (begin < s.size() &&
           std::isspace(static_cast<unsigned char>(s[begin])))
        ++begin;
    size_t end = s.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1])))
        --end;
    return s.substr(begin, end - begin);
}

std::string trim(std::string_view s) {
    return std::string(trimView(s));
}

// Remove a trailing `#` comment while respecting quoted strings. TOML only
// allows comments outside strings, but a `#` inside a dependency URL or a
// project description must not truncate the line.
std::string stripComment(const std::string& line) {
    char quote = 0;
    bool escaped = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (quote != 0) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\' && quote == '"') {
                escaped = true;
            } else if (c == quote) {
                quote = 0;
            }
            continue;
        }
        if (c == '"' || c == '\'') {
            quote = c;
        } else if (c == '#') {
            return line.substr(0, i);
        }
    }
    return line;
}

// Find `wanted` outside of a quoted string. Used to split `key = value`
// without being fooled by an `=` inside a key or string.
size_t findUnquotedChar(const std::string& text, char wanted) {
    char quote = 0;
    bool escaped = false;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (quote != 0) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\' && quote == '"') {
                escaped = true;
            } else if (c == quote) {
                quote = 0;
            }
            continue;
        }
        if (c == '"' || c == '\'') {
            quote = c;
        } else if (c == wanted) {
            return i;
        }
    }
    return std::string::npos;
}

std::string unescapeToml(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '\\' || i + 1 >= value.size()) {
            out.push_back(value[i]);
            continue;
        }
        char next = value[++i];
        switch (next) {
        case 'n':
            out.push_back('\n');
            break;
        case 't':
            out.push_back('\t');
            break;
        case 'r':
            out.push_back('\r');
            break;
        case '"':
            out.push_back('"');
            break;
        case '\\':
            out.push_back('\\');
            break;
        default:
            // Unknown escapes are preserved verbatim. This keeps the
            // parser forgiving for values we do not interpret.
            out.push_back(next);
            break;
        }
    }
    return out;
}

// Accept both `key = "value"` and `key = value` (bare strings, booleans and
// integers all pass through; callers convert when they need a specific type).
std::string parseScalar(const std::string& raw) {
    std::string value = trim(raw);
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        return unescapeToml(value.substr(1, value.size() - 2));
    if (value.size() >= 2 && value.front() == '\'' && value.back() == '\'')
        return value.substr(1, value.size() - 2);
    return value;
}

std::string parseKeyName(const std::string& raw) {
    return parseScalar(raw);
}

bool parseBool(const std::string& raw) {
    return trim(raw) == "true";
}

std::string boardAlias(const std::string& board) {
    // project.toml stores the fully-qualified board name
    // ("arduino:avr:uno"), while the Arduino pipeline keys off the short
    // name. The mega FQBN is "arduino:avr:mega" but the internal board
    // table calls it "mega2560".
    size_t colon = board.find_last_of(':');
    std::string name =
        (colon == std::string::npos) ? board : board.substr(colon + 1);
    if (name == "mega")
        return "mega2560";
    return name;
}

std::string
resolveDependency(const std::string& name, const std::string& projectDir) {
    if (name.empty())
        return {};

    std::vector<fs::path> roots;
    const fs::path exeDir(FileUtils::getExeDir());

    // Installed layout: <prefix>/share/vyne/modules/external/<name>
    roots.push_back(exeDir / "share" / "vyne" / "modules" / "external");
    // Source-tree layout: <repo>/vyne/modules/external/<name>
    roots.push_back(exeDir / "vyne" / "modules" / "external");
    // Binary in <prefix>/bin, data in <prefix>/share.
    roots.push_back(
        exeDir.parent_path() / "share" / "vyne" / "modules" / "external"
    );
    // Project-local overrides/vendoring.
    if (!projectDir.empty()) {
        roots.push_back(fs::path(projectDir) / "modules" / "external");
        roots.push_back(fs::path(projectDir) / "vyne" / "modules" / "external");
    }

    std::error_code ec;
    for (const auto& root : roots) {
        if (root.empty())
            continue;

        fs::path direct = root / name;
        if (fs::exists(direct, ec)) {
            if (fs::is_directory(direct, ec)) {
                // `<name>/<name>.vy` is the conventional facade.
                fs::path leaf = direct / (direct.filename().string() + ".vy");
                if (fs::is_regular_file(leaf, ec))
                    return leaf.string();
                return direct.string();
            }
            return direct.string();
        }

        // Allow a dependency to refer to a bare leaf file.
        fs::path file = root / (name + ".vy");
        if (fs::is_regular_file(file, ec))
            return file.string();
    }

    return {};
}

} // namespace

// ---------------------------------------------------------------------------
// Discovery
// ---------------------------------------------------------------------------
bool findProjectToml(const std::string& startDir, std::string& outPath) {
    std::error_code ec;
    fs::path dir = fs::absolute(fs::path(startDir), ec);
    if (ec)
        dir = fs::path(startDir);

    if (!fs::is_directory(dir, ec)) {
        dir = dir.parent_path();
        if (dir.empty())
            dir = fs::current_path(ec);
    }

    while (!dir.empty()) {
        fs::path candidate = dir / "project.toml";
        if (fs::is_regular_file(candidate, ec)) {
            outPath = candidate.string();
            return true;
        }

        fs::path parent = dir.parent_path();
        if (parent.empty() || parent == dir)
            break;
        dir = parent;
    }

    return false;
}

bool findProjectEntry(const std::string& projectDir, std::string& outEntry) {
    std::error_code ec;
    const fs::path root(projectDir);
    const fs::path candidates[] = {root / "src" / "main.vy", root / "main.vy"};

    for (const auto& candidate : candidates) {
        if (!fs::is_regular_file(candidate, ec))
            continue;
        fs::path resolved = fs::weakly_canonical(candidate, ec);
        outEntry = ec ? candidate.string() : resolved.string();
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// project.toml reader
// ---------------------------------------------------------------------------
bool readProjectToml(
    const std::string& path,
    ProjectConfig& out,
    std::string* error
) {
    out = ProjectConfig{};

    std::ifstream file(path);
    if (!file.is_open()) {
        if (error)
            *error = "could not open `" + path + "`";
        return false;
    }

    std::string section;
    std::string line;
    int lineNumber = 0;
    int lastLineNumber = 0;

    // Collect every [targets.<name>] section first: [targets] default can
    // appear before or after the target tables, so selection happens after
    // the whole file has been read.
    std::map<std::string, std::map<std::string, std::string>> targetTables;

    auto fail = [&](const std::string& message) {
        if (error) {
            *error =
                path + ":" + std::to_string(lastLineNumber) + ": " + message;
        }
        return false;
    };

    while (std::getline(file, line)) {
        ++lineNumber;
        lastLineNumber = lineNumber;

        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        std::string text = trim(stripComment(line));
        if (text.empty())
            continue;

        if (text.front() == '[') {
            if (text.back() != ']')
                return fail("malformed table header");
            section = trim(text.substr(1, text.size() - 2));
            if (section.empty())
                return fail("empty table header");
            continue;
        }

        size_t equals = findUnquotedChar(text, '=');
        if (equals == std::string::npos)
            return fail("expected `key = value`");

        std::string key = parseKeyName(trim(text.substr(0, equals)));
        std::string value = trim(text.substr(equals + 1));
        if (key.empty())
            return fail("empty key");

        if (section == "project") {
            if (key == "name")
                out.name = parseScalar(value);
            else if (key == "version")
                out.version = parseScalar(value);
        } else if (section == "dependencies") {
            // Preserve declaration order (the project box prints deps in
            // the order the user wrote them) and ignore duplicates.
            if (std::find(
                    out.dependencies.begin(),
                    out.dependencies.end(),
                    key
                ) == out.dependencies.end()) {
                out.dependencies.push_back(key);
            }
        } else if (section == "targets") {
            if (key == "default")
                out.default_target = parseScalar(value);
        } else if (section.rfind("targets.", 0) == 0) {
            std::string target = parseKeyName(section.substr(8));
            targetTables[target][key] = parseScalar(value);
        } else if (section == "build") {
            if (key == "deterministic")
                out.deterministic = parseBool(value);
        } else if (section == "bench") {
            if (key == "warmup" || key == "iterations") {
                std::string number = parseScalar(value);
                try {
                    int parsed = std::stoi(number);
                    if (key == "warmup")
                        out.bench_warmup = parsed;
                    else
                        out.bench_iterations = parsed;
                } catch (...) {
                    return fail("`" + key + "` must be an integer");
                }
            } else if (key == "report") {
                out.bench_report = parseScalar(value);
            }
        } else if (section == "device.cuda") {
            if (key == "sm_version") {
                std::string version = parseScalar(value);
                try {
                    out.cuda_sm = std::stoi(version);
                } catch (...) {
                    return fail("`sm_version` must be an integer");
                }
            }
        }
        // Unknown sections/keys are intentionally ignored.
    }

    // A project without an explicit default is still buildable: use the
    // first declared target rather than failing at the CLI layer.
    if (out.default_target.empty() && !targetTables.empty())
        out.default_target = targetTables.begin()->first;

    if (!out.default_target.empty()) {
        auto it = targetTables.find(out.default_target);
        if (it != targetTables.end()) {
            auto field = [&](const char* name) -> std::string {
                auto found = it->second.find(name);
                return found == it->second.end() ? std::string()
                                                 : found->second;
            };
            out.backend = field("backend");
            out.device = field("device");
            out.board = field("board");
        }
    }

    return true;
}

ProjectLoadResult loadProject(const std::string& startDir) {
    ProjectLoadResult result;

    std::string tomlPath;
    if (!findProjectToml(startDir, tomlPath))
        return result;

    result.found = true;
    std::string error;
    if (!readProjectToml(tomlPath, result.config, &error)) {
        result.error = error;
        return result;
    }

    result.config.toml_path = tomlPath;
    result.config.root_dir =
        std::filesystem::path(tomlPath).parent_path().string();
    resolveProjectDependencies(result.config);
    result.ok = true;
    return result;
}

// ---------------------------------------------------------------------------
// Dependencies
// ---------------------------------------------------------------------------
void resolveProjectDependencies(ProjectConfig& cfg) {
    cfg.dependency_paths.clear();
    cfg.dependency_paths.reserve(cfg.dependencies.size());

    for (const auto& dependency : cfg.dependencies) {
        cfg.dependency_paths.push_back(
            resolveDependency(dependency, cfg.root_dir)
        );
    }
}

// ---------------------------------------------------------------------------
// Build options
// ---------------------------------------------------------------------------
void applyProjectConfig(const ProjectConfig& cfg, BuildOptions& opts) {
    // Board targets always produce firmware. `build` and `run` both route
    // through ArduinoRun so the .hex is emitted without an explicit
    // --arduino-flash flag.
    if (!cfg.board.empty()) {
        opts.arduinoBoard = boardAlias(cfg.board);
        opts.mode = RunMode::ArduinoRun;
        return;
    }

    // A CUDA device still emits portable C today, but it must be compiled
    // for the host's native ISA. The [device.cuda] section is also a strong
    // enough signal on its own.
    if (cfg.device == "cuda" || cfg.cuda_sm > 0)
        opts.nativeIsa = true;
}
