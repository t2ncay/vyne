#include "packager.h"
#include "../vyne/utils/sha256.h"
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>

namespace fs = std::filesystem;

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define CYAN    "\033[36m"
#define MAGENTA "\033[35m"

VynePackager::VynePackager(const std::string& scriptPath) : mainScript(scriptPath) {}

void VynePackager::build() {
    fs::path scriptPath(mainScript);
    std::string outDir = scriptPath.stem().string() + "_release";
    
    if (!fs::exists(outDir)) {
        fs::create_directory(outDir);
    }
    
    std::cout << CYAN << "Vyne Builder: Recursive Scan & Deploying to /" << outDir << RESET << "\n";

    try {
        fs::copy_file("vynec.exe", outDir + "/vynec.exe", fs::copy_options::overwrite_existing);
        fs::copy_file("urage.dll", outDir + "/urage.dll", fs::copy_options::overwrite_existing);
    } catch (...) {}

    std::set<std::string> processedFiles;
    scanDependencies(mainScript, outDir, processedFiles);

    // for hashing gamefiles
    std::ofstream manifest(outDir + "/checksums.dat");
    std::cout << MAGENTA << "\n[INTEGRITY] Generating SHA-256 Manifest..." << RESET << "\n";

    for (const auto& entry : fs::recursive_directory_iterator(outDir)) {
        if (entry.is_regular_file()) {
            std::string relativePath = fs::relative(entry.path(), outDir).string();
            if (relativePath == "checksums.dat" || relativePath == "run.bat") continue;

            std::string fileHash = SHA256::hashFile(entry.path().string());
            manifest << relativePath << " " << fileHash << "\n";
            std::cout << CYAN << "  [HASHED] " << RESET << relativePath << " -> " << fileHash.substr(0, 8) << "...\n";
        }
    }
    manifest.close();

    std::ofstream bat(outDir + "/run.bat");
    bat << "@echo off\nvynec.exe " << mainScript << "\npause";
    
    std::cout << GREEN << "\nBUILD SUCCESS: Recursive bundle is ready!" << RESET << "\n";
}

void VynePackager::scanDependencies(const std::string& filePath, const std::string& outDir, std::set<std::string>& processed) {
    if (processed.count(filePath) || !fs::exists(filePath)) return;
    processed.insert(filePath);

    copyFileWithStructure(filePath, outDir);

    std::ifstream file(filePath);
    std::string line;
    
    std::regex assetRegex("\"([^\"]*\\.(png|jpg|jpeg|wav|mp3|fs|vs|ttf|dat|obj|ogg|glb))\"");
    std::regex vyRegex("use\\s+\"([^\"]*\\.vy)\"");

    while (std::getline(file, line)) {
        auto assets_begin = std::sregex_iterator(line.begin(), line.end(), assetRegex);
        auto assets_end = std::sregex_iterator();
        for (std::sregex_iterator i = assets_begin; i != assets_end; ++i) {
            copyFileWithStructure((*i)[1].str(), outDir);
        }

        auto vy_begin = std::sregex_iterator(line.begin(), line.end(), vyRegex);
        auto vy_end = std::sregex_iterator();
        for (std::sregex_iterator i = vy_begin; i != vy_end; ++i) {
            std::string nestedVy = (*i)[1].str();
            std::cout << CYAN << "[DEPENDENCY] " << RESET << nestedVy << "\n";
            scanDependencies(nestedVy, outDir, processed);
        }
    }
}

void VynePackager::copyFileWithStructure(const std::string& path, const std::string& outDir) {
    if (!fs::exists(path)) {
        std::cout << RED << "[MISSING] " << RESET << path << "\n";
        return;
    }

    fs::path p(path);
    fs::path dest = fs::path(outDir) / p;

    if (p.has_parent_path()) {
        fs::create_directories(fs::path(outDir) / p.parent_path());
    }

    fs::copy_file(path, dest, fs::copy_options::overwrite_existing);
    std::cout << YELLOW << "[BUNDLED] " << RESET << path << "\n";
}