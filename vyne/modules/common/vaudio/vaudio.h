#pragma once

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    
    #undef RED
    #undef GREEN
    #undef YELLOW
    #undef BLUE
    #undef MAGENTA

    #define NOGDI 
    #include <windows.h>
    #undef Rectangle
    #undef CloseWindow
    #undef ShowCursor
    #undef LoadImage
    #undef DrawText
    #undef PlaySound
#endif

#include "../../../../vendor/raylib/include/raylib.h"
#include "../../../../vendor/raylib/include/rlgl.h"

#include <cstring>
#include <vector>
#include <string>

#include "../../../compiler/ast/ast.h"
#include "../../../compiler/ast/value.h"

void setupVAudio(SymbolContainer& env, StringPool& pool);