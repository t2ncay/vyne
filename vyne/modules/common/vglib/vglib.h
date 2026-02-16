#pragma once
#include <ctime>
#include <random>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <cmath>

#include "../../../compiler/ast/ast.h"
#include "../../../compiler/ast/value.h"

void setupVGLib(SymbolContainer& env, StringPool& pool);