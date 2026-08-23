// diagnostics.h - Clean Professional Version
#pragma once
#include <string>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <functional>

// Color constants
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

namespace Vyne {

// =============================================================================
// Diagnostic Types
// =============================================================================

enum class Severity { Note, Warning, Error, Critical, Performance };
enum class Category { Syntax, Type, Runtime, Performance, Memory, Security, Deprecation, Style };

struct Diagnostic {
    Severity severity;
    Category category;
    std::string message;
    std::string file;
    int line = 0;
    int column = 0;
    std::vector<std::string> suggestions;
    std::string code;
};

// =============================================================================
// FORWARD DECLARE DiagnosticEngine
// =============================================================================

class DiagnosticEngine;

// =============================================================================
// Configuration System - Declaration only (no implementation)
// =============================================================================

struct WarningConfig {
    bool unused_variable = true;
    bool unused_import = true;
    bool implicit_type = true;
    bool shadow_variable = true;
    bool unreachable_code = true;
    bool infinite_loop = true;
    bool memory_leak = true;
    bool performance = false;
    bool deprecated = true;
    bool style = false;
    
    // Just declare the method - don't define it here!
    void setLevel(const std::string& level);
    
    void ignore(const std::vector<std::string>& warnings);
    bool shouldShow(Category category) const;
};

// =============================================================================
// Diagnostic Engine - FULL DEFINITION
// =============================================================================

class DiagnosticEngine {
    static inline bool quietMode = true;
    static inline bool strictMode = true;
    static inline size_t memoryLimit = 0;
    static inline WarningConfig config;
    static inline std::vector<Diagnostic> diagnostics;

public:
    static void setQuietMode(bool quiet) { quietMode = quiet; }
    static bool isQuietMode() { return quietMode; }
    
    static void setStrictMode(bool strict) { strictMode = strict; }
    static bool isStrictMode() { return strictMode; }
    
    static void setMemoryLimit(size_t limit) { memoryLimit = limit; }
    static size_t getMemoryLimit() { return memoryLimit; }
    static bool getMemoryLimitEnabled() { return memoryLimit > 0; }
    
    static void setWarningLevel(const std::string& level) { config.setLevel(level); }
    static void ignoreWarnings(const std::vector<std::string>& warnings) { config.ignore(warnings); }
    
    static void emit(Diagnostic diag);
    static void emitError(const std::string& message, int line = 0, 
                          const std::string& code = "",
                          const std::vector<std::string>& suggestions = {});
    static void emitWarning(const std::string& message, int line = 0,
                            Category category = Category::Runtime,
                            const std::string& code = "",
                            const std::vector<std::string>& suggestions = {});
    
    static void checkMemoryUsage();
    static const std::vector<Diagnostic>& getDiagnostics() { return diagnostics; }
    static void clearDiagnostics() { diagnostics.clear(); }

private:
    static void printDiagnostic(const Diagnostic& diag);
};

// =============================================================================
// WarningConfig IMPLEMENTATION - After DiagnosticEngine is fully defined
// =============================================================================

inline void WarningConfig::setLevel(const std::string& level) {
    if (level == "all") {
        unused_variable = true; unused_import = true; implicit_type = true;
        shadow_variable = true; unreachable_code = true; infinite_loop = true;
        memory_leak = true; performance = false; deprecated = true; style = false;
        DiagnosticEngine::setQuietMode(false);  // Now DiagnosticEngine is fully defined!
    } else if (level == "none") {
        unused_variable = false; unused_import = false; implicit_type = false;
        shadow_variable = false; unreachable_code = false; infinite_loop = false;
        memory_leak = false; performance = false; deprecated = false; style = false;
        DiagnosticEngine::setQuietMode(true);
    } else if (level == "error_only") {
        unused_variable = false; unused_import = false; implicit_type = false;
        shadow_variable = false; unreachable_code = false; 
        infinite_loop = true; memory_leak = true;
        performance = false; deprecated = false; style = false;
        DiagnosticEngine::setQuietMode(false);
    }
}

inline void WarningConfig::ignore(const std::vector<std::string>& warnings) {
    for (const auto& w : warnings) {
        if (w == "unused_variable") unused_variable = false;
        else if (w == "unused_import") unused_import = false;
        else if (w == "implicit_type") implicit_type = false;
        else if (w == "shadow_variable") shadow_variable = false;
        else if (w == "unreachable_code") unreachable_code = false;
        else if (w == "infinite_loop") infinite_loop = false;
        else if (w == "memory_leak") memory_leak = false;
        else if (w == "performance") performance = false;
        else if (w == "deprecated") deprecated = false;
        else if (w == "style") style = false;
    }
}

inline bool WarningConfig::shouldShow(Category category) const {
    switch(category) {
        case Category::Performance: return performance;
        case Category::Memory: return memory_leak;
        case Category::Deprecation: return deprecated;
        case Category::Style: return style;
        default: return true;
    }
}

// =============================================================================
// DiagnosticEngine Implementation (the ones not defined inline)
// =============================================================================

inline void DiagnosticEngine::emit(Diagnostic diag) {
    if (quietMode && diag.severity != Severity::Critical) return;
    if (diag.severity == Severity::Warning && !config.shouldShow(diag.category)) return;
    
    diagnostics.push_back(diag);
    printDiagnostic(diag);
}

inline void DiagnosticEngine::emitError(const std::string& message, int line, 
                                        const std::string& code,
                                        const std::vector<std::string>& suggestions) {
    emit({Severity::Error, Category::Runtime, message, "", line, 0, suggestions, code});
    throw std::runtime_error(message + " [line " + std::to_string(line) + "]");
}

inline void DiagnosticEngine::emitWarning(const std::string& message, int line,
                                          Category category,
                                          const std::string& code,
                                          const std::vector<std::string>& suggestions) {
    emit({Severity::Warning, category, message, "", line, 0, suggestions, code});
}

inline void DiagnosticEngine::checkMemoryUsage() {
    if (memoryLimit > 0) {
        // Check memory usage
    }
}

inline void DiagnosticEngine::printDiagnostic(const Diagnostic& diag) {
    static const std::unordered_map<Severity, std::pair<std::string, std::string>> severityInfo = {
        {Severity::Note, {COLOR_CYAN, "[Note]"}},
        {Severity::Warning, {COLOR_YELLOW, "[Warning]"}},
        {Severity::Error, {COLOR_RED, "[Error]"}},
        {Severity::Critical, {COLOR_RED, "[Critical]"}},
        {Severity::Performance, {COLOR_MAGENTA, "[Performance]"}},
    };
    
    auto it = severityInfo.find(diag.severity);
    if (it != severityInfo.end()) {
        std::cerr << it->second.first << it->second.second << COLOR_RESET;
    }
    
    if (!diag.code.empty()) {
        std::cerr << " " << COLOR_BOLD << diag.code << COLOR_RESET << ":";
    }
    
    std::cerr << " " << diag.message;
    if (diag.line > 0) {
        std::cerr << " [line " << diag.line << "]";
    }
    std::cerr << "\n";
    
    if (!diag.suggestions.empty()) {
        std::cerr << COLOR_CYAN << "  Suggestions:\n" << COLOR_RESET;
        for (const auto& suggestion : diag.suggestions) {
            std::cerr << "    - " << suggestion << "\n";
        }
    }
}

// =============================================================================
// Memory Usage Callback System
// =============================================================================

using MemoryUsageFetcher = std::function<size_t()>;
inline MemoryUsageFetcher globalUsageFetcher = nullptr;

// =============================================================================
// Convenience Functions
// =============================================================================

inline void setQuietMode(bool quiet) { DiagnosticEngine::setQuietMode(quiet); }
inline bool isQuietMode() { return DiagnosticEngine::isQuietMode(); }
inline void setTypeStrictMode(bool strict) { DiagnosticEngine::setStrictMode(strict); }
inline bool isTypeStrict() { return DiagnosticEngine::isStrictMode(); }
inline void setMemoryLimit(size_t limit) { DiagnosticEngine::setMemoryLimit(limit); }
inline size_t getMemoryLimit() { return DiagnosticEngine::getMemoryLimit(); }
inline bool getMemoryLimitEnabled() { return DiagnosticEngine::getMemoryLimitEnabled(); }
inline void setWarningLevel(const std::string& level) { DiagnosticEngine::setWarningLevel(level); }
inline void ignoreWarnings(const std::vector<std::string>& warnings) { DiagnosticEngine::ignoreWarnings(warnings); }
inline void checkMemoryUsage() { DiagnosticEngine::checkMemoryUsage(); }
inline void warn(const std::string& message, int line = 0) {
    DiagnosticEngine::emitWarning(message, line);
}

template<typename... Args>
[[noreturn]] inline void error(const std::string& message, int line = 0) {
    DiagnosticEngine::emitError(message, line);
    throw std::runtime_error(message);
}

} // namespace Vyne

// =============================================================================
// Global Convenience Functions
// =============================================================================

inline void emitError(const std::string& message, int line = 0, 
                      const std::string& code = "",
                      const std::vector<std::string>& suggestions = {}) {
    Vyne::DiagnosticEngine::emitError(message, line, code, suggestions);
}

inline void emitWarning(const std::string& message, int line = 0,
                        Vyne::Category category = Vyne::Category::Runtime,
                        const std::string& code = "",
                        const std::vector<std::string>& suggestions = {}) {
    Vyne::DiagnosticEngine::emitWarning(message, line, category, code, suggestions);
}

inline void emit(Vyne::Diagnostic diag) {
    Vyne::DiagnosticEngine::emit(diag);
}

inline void setStrictMode(bool strict) {
    Vyne::DiagnosticEngine::setStrictMode(strict);
}

inline void setMemoryLimit(size_t limit) {
    Vyne::DiagnosticEngine::setMemoryLimit(limit);
}

inline void setQuietMode(bool quiet) {
    Vyne::DiagnosticEngine::setQuietMode(quiet);
}

inline bool isQuietMode() {
    return Vyne::DiagnosticEngine::isQuietMode();
}

inline bool isTypeStrict() {
    return Vyne::DiagnosticEngine::isStrictMode();
}

inline void setWarningLevel(const std::string& level) {
    Vyne::DiagnosticEngine::setWarningLevel(level);
}

inline void warn(const std::string& message, int line = 0) {
    Vyne::DiagnosticEngine::emitWarning(message, line);
}

inline bool getMemoryLimitEnabled() {
    return Vyne::DiagnosticEngine::getMemoryLimitEnabled();
}

inline void checkMemoryUsage() {
    Vyne::DiagnosticEngine::checkMemoryUsage();
}