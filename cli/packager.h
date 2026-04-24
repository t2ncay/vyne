#ifndef VYNE_PACKAGER_H
#define VYNE_PACKAGER_H

#include <string>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

class VynePackager {
public:
    explicit VynePackager(const std::string& scriptPath);

    void build(const std::string& outDir);

private:
    std::string mainScript;

    void copyAsset(const std::string& path, const std::string& outDir);
    
    void bundleBinaries(const std::string& outDir);
};

#endif