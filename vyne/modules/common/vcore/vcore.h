#pragma once
#include <ctime>
#include <chrono>
#include <thread>
#include <random>
#include <iostream>
#include <filesystem>

#ifdef _WIN32
    #include <process.h>
    #include <windows.h>
    #include <psapi.h>
    #define getpid _getpid
#elif __linux__
    #include <unistd.h>
    #include <fstream>
#elif __APPLE__
    #include <mach/mach.h>
#endif

#include "httplib.h"

#include "../../../compiler/ast/ast.h"
#include "../../../compiler/ast/value.h"

void setupVCore(SymbolContainer& env, StringPool& pool);