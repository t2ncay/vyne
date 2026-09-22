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
// Source Code Getter
// =============================================================================

inline std::string vyne_getSourceLine(const std::string& src, int targetLine) {
    if (targetLine <= 0 || src.empty()) return "";
    int current = 1;
    size_t start = 0;
    for (size_t i = 0; i <= src.size(); ++i) {
        if (i == src.size() || src[i] == '\n') {
            if (current == targetLine) {
                size_t end = i;
                if (end > start && src[end - 1] == '\r') end--;
                return src.substr(start, end - start);
            }
            current++;
            start = i + 1;
        }
    }
    return "";
}

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
    static inline bool warningsAsErrors = false;
    static inline bool pedanticMode = false;
    static inline bool verboseMode = false;
    static inline size_t memoryLimit = 0;
    static inline WarningConfig config;
    static inline std::vector<Diagnostic> diagnostics;
    static inline std::string sourceText;

public:
    static void setQuietMode(bool quiet) { quietMode = quiet; }
    static bool isQuietMode() { return quietMode; }

    static void setWarningsAsErrors(bool v) { warningsAsErrors = v; }
    static bool areWarningsErrors() { return warningsAsErrors; }

    static void setPedanticMode(bool v) { pedanticMode = v; }
    static bool isPedantic() { return pedanticMode; }

    static void setVerboseMode(bool v) { verboseMode = v; }
    static bool isVerbose() { return verboseMode; }

    static void setSourceText(const std::string& s) { sourceText = s; }
    static void printSummary();
    
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
    if (diag.severity == Severity::Warning && warningsAsErrors) {
        diag.severity = Severity::Error;
    }

    bool isError = (diag.severity == Severity::Error ||
                    diag.severity == Severity::Critical);

    if (diag.severity == Severity::Note && !verboseMode) return;

    if (quietMode && !isError && !verboseMode) return;

    if (diag.severity == Severity::Warning && !pedanticMode &&
        !config.shouldShow(diag.category)) return;

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
    std::string sevColor = COLOR_RESET;
    if (it != severityInfo.end()) {
        sevColor = it->second.first;
        std::cerr << sevColor << it->second.second << COLOR_RESET;
    }

    if (!diag.code.empty()) {
        std::cerr << " " << COLOR_BOLD << diag.code << COLOR_RESET << ":";
    }

    std::cerr << " " << diag.message;
    if (diag.line > 0) {
        std::cerr << " [line " << diag.line << "]";
    }
    std::cerr << "\n";

    // --- Source-context rendering with caret ---
    if (!sourceText.empty() && diag.line > 0) {
        std::string snippet = vyne_getSourceLine(sourceText, diag.line);
        if (!snippet.empty()) {
            std::string lineStr = std::to_string(diag.line);
            std::string gutter(lineStr.size(), ' ');
            std::cerr << COLOR_CYAN << "  " << gutter << " |" << COLOR_RESET << "\n";
            std::cerr << COLOR_CYAN << "  " << lineStr << " | " << COLOR_RESET
                      << snippet << "\n";
            int col = (diag.column > 0) ? diag.column - 1 : 0;
            std::cerr << COLOR_CYAN << "  " << gutter << " | " << COLOR_RESET;
            for (int i = 0; i < col; ++i) std::cerr << ' ';
            std::cerr << sevColor << COLOR_BOLD << "^" << COLOR_RESET << "\n";
        }
    }

    if (!diag.suggestions.empty()) {
        std::cerr << COLOR_CYAN << "  Suggestions:\n" << COLOR_RESET;
        for (const auto& suggestion : diag.suggestions) {
            std::cerr << "    - " << suggestion << "\n";
        }
    }
}

inline void DiagnosticEngine::printSummary() {
    int notes = 0, warnings = 0, errors = 0, critical = 0, perf = 0;
    for (const auto& d : diagnostics) {
        switch (d.severity) {
            case Severity::Note:        notes++;    break;
            case Severity::Warning:     warnings++; break;
            case Severity::Error:       errors++;   break;
            case Severity::Critical:    critical++; break;
            case Severity::Performance: perf++;     break;
        }
    }

    int total = notes + warnings + errors + critical + perf;
    if (total == 0) return;

    std::cerr << "\n" << COLOR_BOLD << "[Summary]" << COLOR_RESET << " ";

    bool first = true;
    auto emitPart = [&](int count, const char* label, const char* color) {
        if (count == 0) return;
        if (!first) std::cerr << ", ";
        first = false;
        std::cerr << color << count << " " << label << COLOR_RESET;
    };

    emitPart(errors,   "error(s)",     COLOR_RED);
    emitPart(warnings, "warning(s)",   COLOR_YELLOW);
    emitPart(notes,    "note(s)",      COLOR_CYAN);
    emitPart(perf,     "perf hint(s)", COLOR_MAGENTA);
    emitPart(critical, "critical",     COLOR_RED);

    std::cerr << "\n";
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