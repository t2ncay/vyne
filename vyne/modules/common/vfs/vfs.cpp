#include "vfs.h"

/**
 * VFS Native Method Implementations
 */

namespace VFsNative {
    
}

void setupVFs(SymbolContainer& env, StringPool& pool) {
    const std::string& path = "global.vfs";
    
    if (env.find(path) == env.end()) {
        env[path] = SymbolTable();
    }

    auto& vfs = env[path];

    // vfs methods

    // vfs properties
    vfs[pool.intern("cwd")]             = Value(std::filesystem::current_path().string()).setReadOnly();
}