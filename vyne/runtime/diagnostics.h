#pragma once
#include <string>
#include <iostream>

#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

namespace Vyne {
    using MemoryUsageFetcher = size_t (*)();
    inline MemoryUsageFetcher globalUsageFetcher = nullptr;

    inline bool quietMode = true;
    inline bool typeStrictMode = true;

    inline bool memoryLimitEnabled = false;
    inline size_t memoryLimit = 0;
 
    inline void setQuietMode(bool quiet) { quietMode = quiet; }
    inline bool isQuietMode() { return quietMode; }

    inline void setTypeStrictMode(bool ts) { typeStrictMode = ts; }
    inline bool isTypeStrict() { return typeStrictMode; }

    inline void setMemoryLimit(size_t limit) { memoryLimit = limit; }
    inline size_t getMemoryLimit() { return memoryLimit; }

    inline void setMemoryLimitEnabled(bool state) { memoryLimitEnabled = state; }
    inline size_t getMemoryLimitEnabled() { return memoryLimitEnabled; }

    inline void warn(const std::string& message, int line = 0) {
        if (quietMode) return;
        std::cerr << COLOR_YELLOW << "[ Warning ] " << message;
        if (line > 0) std::cerr << " [line " << line << "]";
        std::cerr << COLOR_RESET << "\n";
    }
    
    [[noreturn]] inline void error(const std::string& message, int line = 0) {
        throw std::runtime_error("\033[31m [ Error ] " + message + " [line " + std::to_string(line) +  " ]\033[0m\n");
    }

    inline void checkMemoryUsage() {
        if (memoryLimit > 0 && globalUsageFetcher != nullptr) {
            size_t current = globalUsageFetcher();
            if (current > memoryLimit) {
                error("Memory limit exceeded! Limit: " + std::to_string(memoryLimit) + 
                      " bytes, Current: " + std::to_string(current));
            }
        }
    }
}