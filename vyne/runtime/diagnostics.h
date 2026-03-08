// runtime/diagnostics.h
#pragma once
#include <string>
#include <iostream>

namespace Vyne {
    inline bool quietMode = true;
    inline bool typeStrictMode = true;
 
    inline void setQuietMode(bool quiet) { quietMode = quiet; }
    inline bool isQuietMode() { return quietMode; }

    inline void setTypeStrictMode(bool ts) { typeStrictMode = ts; }
    inline bool isTypeStrict() { return typeStrictMode; }

    inline void warn(const std::string& message, int line = 0) {
        if (quietMode) return;
        std::cerr << "\033[33m[ Warning ] " << message;
        if (line > 0) std::cerr << " [line " << line << "]";
        std::cerr << "\033[0m\n";
    }
    
    [[noreturn]] inline void error(const std::string& message, int line = 0) {
        throw std::runtime_error("\033[31m [ Error ] " + message + " [line " + std::to_string(line) +  " ]\033[0m\n");
    }
}