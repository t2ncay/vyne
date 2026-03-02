#include "vurage.h"

/**
 * Vurage Native Method Implementations
 */

namespace VurageNative {
    
}

void setupVurage(SymbolContainer& env, StringPool& pool) {
    const std::string& path = "global.vurage";
    
    if (env.find(path) == env.end()) {
        env[path] = SymbolTable();
    }

    auto& vurage = env[path];

    // vurage methods

    // vurage properties
}