#pragma once
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "../../../compiler/ast/ast.h"
#include "../../../compiler/ast/value.h"

void setupVurage(SymbolContainer& env, StringPool& pool);