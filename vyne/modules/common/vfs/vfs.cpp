#include "vfs.h"

/**
 * VFS Native Method Implementations
 */

namespace VFsNative {
    
}

void setupVFs(SymbolContainer& env, StringPool& pool) {
    const std::string& path = "vfs";
    
    if (env.find(path) == env.end()) {
        env[path] = SymbolTable();
    }

    auto& vfs = env[path];

    // vfs methods

    // vfs properties
}