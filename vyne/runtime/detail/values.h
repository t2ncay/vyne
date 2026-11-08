// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.

#pragma once
#include "types.h"
// ============================================================================
// VALUE CREATION
// ============================================================================

static inline VyneValue vyne_null(void) {
    VyneValue val = { .type = V_NULL };
    val.as.i64 = 0;
    return val;
}

static inline VyneValue vyne_int(int64_t v) {
    VyneValue val = { .type = V_INT64 };
    val.as.i64 = v;
    return val;
}

static inline VyneValue vyne_bool(bool v) {
    VyneValue val = { .type = V_BOOL };
    val.as.i64 = v ? 1 : 0;
    return val;
}

static inline VyneValue vyne_float(double v) {
    VyneValue val = { .type = V_FLOAT64 };
    val.as.f64 = v;
    return val;
}

static inline VyneValue vyne_float32(float v) {
    VyneValue val = { .type = V_FLOAT32 };
    val.as.f64 = (double)v;
    return val;
}

static inline VyneValue vyne_string(const char* s) {
    VyneValue val = { .type = V_STRING };
    if (s) {
        size_t len = strlen(s) + 1;
        char* copy = (char*)arena_alloc(len);
        memcpy(copy, s, len);
        val.as.str = copy;
    } else {
        val.as.str = NULL;
    }
    return val;
}

static inline VyneValue vyne_string_own(char* s) {
    VyneValue val = { .type = V_STRING };
    val.as.str = s;
    return val;
}

// Use for C string literals only. The pointer is borrowed from .rodata
// and must outlive the program. No copy, no arena allocation.
static inline VyneValue vyne_string_static(const char* s) {
    VyneValue val = { .type = V_STRING };
    val.as.str = (char*)s;
    return val;
}

// 256 one-byte strings, all zero-terminated. Populated lazily on first use.
static char _vyne_char_pool[256][2];
static int  _vyne_char_pool_ready = 0;

static inline void _vyne_init_char_pool(void) {
    if (_vyne_char_pool_ready) return;
    for (int i = 1; i < 256; i++) _vyne_char_pool[i][0] = (char)i;
    _vyne_char_pool_ready = 1;
}

static inline VyneValue vyne_char_at(const char* s, int idx) {
    _vyne_init_char_pool();
    VyneValue val = { .type = V_STRING };
    val.as.str = _vyne_char_pool[(uint8_t)s[idx]];
    return val;
}

static inline VyneValue vyne_array_create(int initial_size) {
    VyneValue val = { .type = V_ARRAY };
    VyneArray* arr = (VyneArray*)arena_alloc(sizeof(VyneArray));
    int cap = initial_size > 0 ? initial_size : 4;
    arr->elements = (VyneValue*)arena_alloc(sizeof(VyneValue) * cap);
    arr->size = initial_size;
    arr->capacity = cap;
    for (int i = 0; i < initial_size; i++)
        arr->elements[i] = vyne_null();
    val.as.arr = arr;
    return val;
}

static inline VyneValue vyne_struct_create(const char* type_name) {
    VyneValue val = { .type = V_STRUCT };
    VyneStruct* s = (VyneStruct*)arena_alloc(sizeof(VyneStruct));
    s->type_name = type_name;
    s->fields = NULL;
    s->field_count = 0;
    s->methods = NULL;
    s->method_count = 0;
    s->field_cache[0] = -1;
    s->field_cache[1] = -1;
    s->field_cache[2] = -1;
    s->field_cache[3] = -1;
    val.as.strct = s;
    return val;
}

