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

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"

void init_REPL(std::string& input, SymbolContainer& env);