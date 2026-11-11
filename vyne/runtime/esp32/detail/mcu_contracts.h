// runtime/arduino/detail/mcu_contracts.h
//
// Executable requires/ensures/@invariant for the AVR target. Mirrors
// runtime/detail/contracts.h, but the evidence log goes to Serial and a
// failed contract halts the board instead of calling exit(1).
//
// `ruleset { contracts = "off" }` emits no calls at all, which is the
// release-build path.

#pragma once
#include "mcu_types.h"

static inline void vyne_contract_record(
    const char* status,
    const char* kind,
    const char* name,
    const char* file,
    int line
) {
    ::Serial.print(status);
    ::Serial.print(",");
    ::Serial.print(kind);
    ::Serial.print(",");
    ::Serial.print(name);
    ::Serial.print(",");
    ::Serial.print(file);
    ::Serial.print(",");
    ::Serial.println((long)line);
}

static inline void vyne_contract_assert(
    int condition,
    const char* kind,
    const char* name,
    const char* file,
    int line,
    int evidence
) {
    if (condition) {
        if (evidence)
            vyne_contract_record("PASS", kind, name, file, line);
        return;
    }

    vyne_contract_record("FAIL", kind, name, file, line);
    ::Serial.print("Contract violation (VNE-111): ");
    ::Serial.print(kind);
    ::Serial.print(" contract '");
    ::Serial.print(name);
    ::Serial.print("' failed at ");
    ::Serial.print(file);
    ::Serial.print(":");
    ::Serial.println((long)line);
    while (1) {
    }
}
