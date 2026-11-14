// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Tuncay Gafarli
#include "raylib_build.h"
#include "../vyne/utils/sha256.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <thread>

namespace fs = std::filesystem;
namespace {
std::string quote(const std::string& s) {
    if (s.find_first_of("\"\r\n") != std::string::npos)
        throw std::runtime_error("raylib build path contains unsupported characters");
#ifndef _WIN32
    std::string out = "'";
    for (char c : s) out += c == '\'' ? "'\\''" : std::string(1, c);
    return out + "'";
#else
    return "\"" + s + "\"";
#endif
}
std::string compilerIdentity() {
#ifdef _WIN32
    FILE* p = _popen("gcc -v 2>&1", "r");
#else
    FILE* p = popen("gcc -v 2>&1", "r");
#endif
    if (!p) throw std::runtime_error("raylib: gcc was not found");
    std::string result;
    char buf[1024];
    while (std::fgets(buf, sizeof(buf), p)) result += buf;
#ifdef _WIN32
    int rc = _pclose(p);
#else
    int rc = pclose(p);
#endif
    if (rc) throw std::runtime_error("raylib requires gcc on PATH, matching Vyne's desktop C backend");
    return result;
}
fs::path cacheRoot() {
    if (const char* p = std::getenv("VYNE_RAYLIB_CACHE")) return fs::path(p);
#ifdef _WIN32
    if (const char* p = std::getenv("LOCALAPPDATA")) return fs::path(p) / "vyne" / "raylib";
#else
    if (const char* p = std::getenv("XDG_CACHE_HOME")) return fs::path(p) / "vyne" / "raylib";
    if (const char* p = std::getenv("HOME")) return fs::path(p) / ".cache" / "vyne" / "raylib";
#endif
    return fs::temp_directory_path() / "vyne-raylib";
}
}

std::string raylibLinkFlags(const std::string& exeDir) {
    const fs::path root = fs::path(exeDir);
    const fs::path source = root / "vyne/runtime/vraylib";
    if (!fs::is_regular_file(root / "vendor/raylib/src/raylib.h"))
        throw std::runtime_error("raylib 6.2 sources missing beside vynec: vendor/raylib/src/raylib.h");
    std::vector<fs::path> files;
    for (const auto& dir : {root / "vendor/raylib", source}) {
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (!entry.is_regular_file()) continue;
            const auto ext = entry.path().extension().string();
            if (ext == ".c" || ext == ".h" || ext == ".inc" || ext == ".cmake" || ext == ".txt")
                files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    // Include compiler identity, the source path (CMake embeds it), and all native
    // source contents, not just timestamps. Updating any binding invalidates cache.
    uint64_t hash = 14695981039346656037ull;
    auto feed = [&](const std::string& s) { for (unsigned char c : s) { hash ^= c; hash *= 1099511628211ull; } };
    feed("raylib-6.2-gl33-static-v2"); feed(root.string()); feed(compilerIdentity());
    for (const auto& p : files) { feed(fs::relative(p, root).generic_string()); feed(SHA256::hashFile(p.string())); }
    const fs::path cache = fs::absolute(cacheRoot() / std::to_string(hash));
    const fs::path bridge = cache / "lib/libvyne_raylib.a";
    const fs::path raylib = cache / "lib/libraylib.a";
    const fs::path ready = cache / "complete";
    auto complete = [&] { return fs::is_regular_file(ready) && fs::is_regular_file(bridge) && fs::is_regular_file(raylib); };
    if (!complete()) {
        fs::create_directories(cache);
        const fs::path lock = cache / "build.lock";
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(5);
        while (!fs::create_directory(lock)) {
            if (std::chrono::steady_clock::now() > deadline)
                throw std::runtime_error("raylib build cache lock timed out: " + lock.string());
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        struct Unlock { fs::path path; ~Unlock() { std::error_code ec; fs::remove(path,ec); } } unlock{lock};
        if (!complete()) {
            std::cout << "  raylib     building vendored 6.2 (OpenGL 3.3)\n";
#ifdef _WIN32
            const char* generator = "MinGW Makefiles";
#else
            const char* generator = "Unix Makefiles";
#endif
            const std::string configure = "cmake -S " + quote(source.string()) + " -B " + quote(cache.string()) +
                " -G " + quote(generator) + " -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc";
            if (std::system(configure.c_str()) || std::system(("cmake --build " + quote(cache.string()) + " --parallel 4").c_str()))
                throw std::runtime_error("raylib native build failed; install CMake, make, and the platform graphics development libraries (see vraylib/README.md)");
            std::ofstream stamp(ready);
            stamp << "raylib 6.2 / OpenGL 3.3\n";
            if (!stamp) throw std::runtime_error("cannot publish raylib cache completion marker");
        }
    }
    std::string flags = " " + quote(bridge.string()) + " " + quote(raylib.string());
#ifdef _WIN32
    flags += " -lopengl32 -lgdi32 -lwinmm -lshell32 -luser32";
#elif defined(__APPLE__)
    flags += " -framework Cocoa -framework IOKit -framework CoreVideo -framework OpenGL -framework QuartzCore -lpthread";
#else
    flags += " -lGL -lX11 -lpthread -ldl -lm";
#endif
    return flags;
}
