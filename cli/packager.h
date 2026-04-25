#ifndef VYNE_PACKAGER_H
#define VYNE_PACKAGER_H

#include <string>
#include <iostream>
#include <filesystem>
#include <set>

class VynePackager {
public:
    explicit VynePackager(const std::string& scriptPath);

    void build();

private:
    std::string mainScript;

    void scanDependencies(const std::string& filePath, const std::string& outDir, std::set<std::string>& processed);
    
    void copyFileWithStructure(const std::string& path, const std::string& outDir);
};

#endif