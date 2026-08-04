#pragma once

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif

    #ifndef NOMINMAX
    #define NOMINMAX
    #endif

    #define NOGDI
    #define NOUSER

    #include <windows.h>
#endif

#include "../../../../vendor/raylib/include/raylib.h"
#include "../../../../vendor/raylib/include/rlgl.h"
#include "../../../../vendor/raylib/include/raymath.h"

#include <cstring>
#include <vector>
#include <string>
#include <map>

#include "../../../compiler/ast/ast.h"
#include "../../../compiler/ast/value.h"

void setupVGLib(SymbolContainer& env, StringPool& pool);