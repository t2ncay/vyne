// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Tuncay Gafarli

#include "cli/process_status.h"
#include <cstdlib>
#include <iostream>

static void check(int actual, int expected, const char* name) {
    if (actual != expected) {
        std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
        std::exit(1);
    }
}

int main() {
    check(Vyne::decodeSystemExitStatus(-1), 1, "command processor launch failure");
#ifdef _WIN32
    check(Vyne::decodeSystemExitStatus(0), 0, "success");
    check(Vyne::decodeSystemExitStatus(7), 7, "Windows exit 7");
    check(Vyne::decodeSystemExitStatus(23), 23, "Windows exit 23");
    check(Vyne::decodeSystemExitStatus(255), 255, "Windows exit 255");
    check(Vyne::decodeSystemExitStatus(513), 513, "Windows wider exit status");
    check(Vyne::decodeSystemExitStatus(std::system("exit /b 23")), 23,
          "real Windows system exit");
#else
    check(Vyne::decodeSystemExitStatus(std::system("exit 0")), 0, "success");
    check(Vyne::decodeSystemExitStatus(std::system("exit 7")), 7, "POSIX exit 7");
    check(Vyne::decodeSystemExitStatus(std::system("exit 23")), 23, "POSIX exit 23");
    check(Vyne::decodeSystemExitStatus(std::system("exit 255")), 255, "POSIX exit 255");
    check(Vyne::decodeSystemExitStatus(std::system("kill -TERM $$")), 143,
          "POSIX command processor terminated by signal");
    check(Vyne::decodeSystemExitStatus(0x7f), 1, "unexpected stopped status");
#endif
    std::cout << "V01 process status contract: PASS\n";
}
