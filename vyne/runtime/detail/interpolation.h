// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.

#pragma once
#include "modules.h"
// ============================================================================
// INTERPOLATED STRING
// ============================================================================

static inline VyneValue vyne_interpolate(const char* parts[], int part_count, VyneValue* values) {
    // Render each interpolated value once. The measuring and copy passes
    // used to call vyne_to_string twice per value, allocating a fresh
    // string for every scalar, array, or map in the literal.
    int value_count = part_count / 2;
    VyneValue* rendered = (VyneValue*)arena_alloc(
        sizeof(VyneValue) * (size_t)(value_count > 0 ? value_count : 1)
    );
    for (int i = 0; i < value_count; i++) {
        rendered[i] = vyne_to_string(values[i]);
    }

    size_t total_len = 1;
    for (int i = 0; i < part_count; i++) {
        if (i % 2 == 0) {
            total_len += strlen(parts[i]);
        } else {
            total_len += strlen(rendered[i / 2].as.str);
        }
    }

    char* result = (char*)arena_alloc(total_len);
    size_t pos = 0;
    for (int i = 0; i < part_count; i++) {
        if (i % 2 == 0) {
            const char* s = parts[i];
            size_t len = strlen(s);
            memcpy(result + pos, s, len);
            pos += len;
        } else {
            size_t len = strlen(rendered[i / 2].as.str);
            memcpy(result + pos, rendered[i / 2].as.str, len);
            pos += len;
        }
    }
    result[pos] = '\0';
    return vyne_string_own(result);
}
