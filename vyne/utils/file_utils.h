#pragma once
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <filesystem>

namespace FileUtils {
    inline std::string exeDir = "."; 

    static void setExeDir(const std::string& argv0) {
        exeDir = std::filesystem::absolute(argv0).parent_path().string();
    }

    static std::string getExternPath(const std::string& filePath) {
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