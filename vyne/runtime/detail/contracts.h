// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.

#pragma once
// runtime/detail/contracts.h — executable requires/ensures/@invariant.
//
// `ruleset { contracts = "check" | "evidence" | "off" }` controls whether
// codegen emits calls to vyne_contract_assert. "check" catches violations
// at runtime; "evidence" also appends a machine-readable log line; "off"
// emits nothing for release builds.
//
// The log is CSV: status,kind,name,file,line
//   status  PASS | FAIL
//   kind    requires | ensures | invariant
//   name    function or loop name
//   file    source file
//   line    source line
//
// Destination: $VYNE_CONTRACT_LOG if set, else vyne_contracts.log in the
// process working directory. The file is truncated when first opened in
// a process so a run's evidence is self-contained.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE* g_vyne_contract_log = NULL;
static int g_vyne_contract_log_attempted = 0;

static inline FILE* vyne_contract_log_file(void) {
    if (!g_vyne_contract_log_attempted) {
        g_vyne_contract_log_attempted = 1;
        const char* path = getenv("VYNE_CONTRACT_LOG");
        g_vyne_contract_log = fopen(
            (path && *path) ? path : "vyne_contracts.log", "w"
        );
    }
    return g_vyne_contract_log;
}

static inline void vyne_contract_record(
    const char* status,
    const char* kind,
    const char* name,
    const char* file,
    int line
) {
    FILE* f = vyne_contract_log_file();
    if (!f)
        return;
    fprintf(f, "%s,%s,%s,%s,%d\n", status, kind, name, file, line);
    fflush(f);
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
    fprintf(
        stderr,
        "Contract violation (VNE-111): %s contract '%s' failed at %s:%d\n"
        "  Fix the call site, restore the invariant, or compile with "
        "`ruleset { contracts = \"off\" }` for a release build.\n",
        kind,
        name,
        file,
        line
    );
    exit(1);
}
