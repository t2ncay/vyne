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
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <filesystem>
#include <cstdlib>

#include "../runtime/diagnostics.h"

namespace FileUtils {
    inline std::string exeDir = "."; 

    static void setExeDir(const std::string& argv0) {
        try {
            std::filesystem::path executable(argv0);
            if (!executable.has_parent_path()) {
                if (const char* env = std::getenv("PATH")) {
                    std::stringstream paths(env);
                    std::string root;
#ifdef _WIN32
                    constexpr char separator = ';';
#else
                    constexpr char separator = ':';
#endif
                    while (std::getline(paths, root, separator)) {
                        auto candidate = std::filesystem::path(root) / executable;
#ifdef _WIN32
                        if (!candidate.has_extension()) candidate += ".exe";
#endif
                        if (std::filesystem::is_regular_file(candidate)) { executable = candidate; break; }
                    }
                }
            }
            std::filesystem::path p = std::filesystem::weakly_canonical(std::filesystem::absolute(executable));
            exeDir = p.parent_path().string();
            if (exeDir.empty()) exeDir = ".";
        } catch (...) {
            exeDir = ".";
        }
    }

    static std::string getExeDir() {
        return exeDir;
    }

    static std::string getShareDir() {
        const auto exe = std::filesystem::path(exeDir);
        for (const auto& root : {exe / "share/vyne", exe.parent_path() / "share/vyne"})
            if (std::filesystem::is_directory(root)) return root.string();
        return (exe / "share/vyne").string();
    }

    static std::string getExternalPath(const std::string& filePath) {
        const auto source = std::filesystem::path(exeDir) / "vyne/modules/external";
        const auto installed = std::filesystem::path(getShareDir()) / "modules/external";
        for (const auto& root : {source, installed}) {
            const auto candidate = root / filePath;
            if (std::filesystem::exists(candidate)) return candidate.string();
        }
        return (source / filePath).string();
    }

    [[maybe_unused]] static std::string getExternPath(const std::string& filePath) {
        std::filesystem::path base(exeDir);
        return (base / "modules" / "external" / filePath).string();
    }

    static std::string readFile(const std::string& path) {
        if (std::filesystem::is_directory(path)) {
            throw std::runtime_error("IO Error: Targeted path is a DIRECTORY, not a file. Path: '" + path + "'");
        }

        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("IO Error: Could not open file at '" + path + "'");
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
}
