#pragma once
#include <ctime>
#include <random>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

#include "../../../compiler/ast/ast.h"
#include "../../../compiler/ast/value.h"

void setupVFs(SymbolContainer& env, StringPool& pool);