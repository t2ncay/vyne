#include "packager.h"
#include <filesystem>
#include <fstream>
#include <regex>

namespace fs = std::filesystem;

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"

VynePackager::VynePackager(const std::string& scriptPath) : mainScript(scriptPath) {}

void VynePackager::build() {
    fs::path scriptPath(mainScript);
    std::string outDir = scriptPath.stem().string() + "_release";
    
    if (!fs::exists(outDir)) {
        fs::create_directory(outDir);
    }
    
    std::cout << CYAN << "Vyne Builder: Deploying project to /" << outDir << RESET << "\n";

    try {
        fs::copy_file("vynec.exe", outDir + "/vynec.exe", fs::copy_options::overwrite_existing);
        fs::copy_file("urage.dll", outDir + "/urage.dll", fs::copy_options::overwrite_existing);
    } catch (const std::exception& e) {
        std::cout << MAGENTA << "Warning: Engine binaries (vynec/urage) not found in root." << RESET << "\n";
    }
    
    std::ifstream file(mainScript);
    if (!file.is_open()) {
        std::cerr << RED << "Error: Could not open " << mainScript << RESET << "\n";
        return;
    }

    std::string line;
    std::regex pathRegex("\"([^\"]*\\.(png|jpg|jpeg|wav|mp3|fs|vs|ttf|dat|obj))\"");

    while (std::getline(file, line)) {
        auto words_begin = std::sregex_iterator(line.begin(), line.end(), pathRegex);
        auto words_end = std::sregex_iterator();

        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            copyAsset((*i)[1].str(), outDir);
        }
    }

    fs::copy_file(mainScript, outDir + "/" + mainScript, fs::copy_options::overwrite_existing);

    std::ofstream bat(outDir + "/run.bat");
    bat << "@echo off\nvynec.exe --ast " << mainScript << "\npause";
    
    std::cout << GREEN << "\nBUILD SUCCESS: /" << outDir << " is ready for delivery!" << RESET << "\n";
}

void VynePackager::copyAsset(const std::string& path, const std::string& outDir) {
    if (fs::exists(path)) {
        fs::path p(path);
        if (p.has_parent_path()) {
            fs::create_directories(fs::path(outDir) / p.parent_path());
        }
        fs::copy_file(path, fs::path(outDir) / path, fs::copy_options::overwrite_existing);
        std::cout << YELLOW << "[BUNDLED] " << RESET << path << "\n";
    } else {
        std::cout << RED << "[MISSING] " << RESET << path << "\n";
    }
}