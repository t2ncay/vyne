#pragma once
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <chrono>
#include <format>
#include <cstdio>

#include "../vyne/compiler/lexer/lexer.h"
#include "../vyne/compiler/parser/parser.h"
#include "../vyne/compiler/ast/ast.h"
#include "../vyne/compiler/ast/value.h"
#include "../vyne/compiler/codegen/codegen.h"

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"
#define MAGENTA "\033[35m"

int runFile(const std::string& filename, SymbolContainer& env, const std::string& mode);

template<typename... Args>
static inline void vprint(std::string_view fmt, Args&&... args) {
    std::string s = std::vformat(fmt, std::make_format_args(args...));
    std::printf("%s", s.c_str());
}

template<typename... Args>
static inline void vprintln(std::string_view fmt, Args&&... args) {
    std::string s = std::vformat(fmt, std::make_format_args(args...));
    std::printf("%s\n", s.c_str());
}