// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.

#pragma once
#include "arena.h"
// ============================================================================
// VALUE TYPES
// ============================================================================

typedef struct VyneValue VyneValue;
struct VyneArray;
struct VyneArray_f64;
struct VyneArray_f32;
struct VyneArray_i64;
struct VyneStruct;

typedef VyneValue (*VyneMethodFn)(int argc, VyneValue* args);

typedef enum {
    V_NULL = 0,
    V_FLOAT64 = 1,
    V_INT64 = 2,
    V_STRING = 3,
    V_ARRAY = 4,
    V_BOOL = 5,
    V_STRUCT = 6,
    V_FUNCTION = 7,
    V_MODULE = 8,
    V_REFERENCE = 9,
    V_MAP = 10,
    V_F64_ARRAY = 11,   // NEW: .as.arr_f64 — raw double* data, O(1) box/unbox
    V_I64_ARRAY = 12,
    V_FLOAT32 = 13,
    V_F32_ARRAY = 14    // NEW: .as.arr_i64 — raw int64_t* data, O(1) box/unbox
} VyneType;

struct VyneValue {
    VyneType type;
    uint32_t  _reserved;
    union {
        double f64;
        int64_t i64;
        char* str;
        struct VyneArray* arr;
        struct VyneArray_f64* arr_f64;
        struct VyneArray_f32* arr_f32;
        struct VyneArray_i64* arr_i64;
        struct VyneStruct* strct;
        struct VyneFunction* fn;
        struct VyneMap* map;
        void* ptr;
    } as;
};

// ============================================================================
// ARRAY
// ============================================================================

typedef struct VyneArray {
    VyneValue* elements;
    int size;
    int capacity;
} VyneArray;

typedef struct VyneArray_f64 {   // named so the union can forward-reference it
    double*  data;
    int64_t  size;
    int64_t  cap;
} VyneArray_f64;

typedef struct VyneArray_i64 {   // named so the union can forward-reference it
    int64_t* data;
    int64_t  size;
    int64_t  cap;
} VyneArray_i64;

typedef struct VyneArray_f32 {   // Float32 payload, same layout as f64
    float*   data;
    int64_t  size;
    int64_t  cap;
} VyneArray_f32;

// ============================================================================
// MAP
// ============================================================================
typedef struct VyneMapEntry {
    const char* key;    // NULL = empty, TOMBSTONE = deleted
    VyneValue value;
} VyneMapEntry;

#define VYNE_TOMBSTONE ((const char*)1)

typedef struct VyneMap {
    VyneMapEntry* entries;   // 2^n slots
    int size;
    int capacity;
    int tombstones;
} VyneMap;

// Control byte sentinels. H2 values are 0x00..0x7F from the top 7 bits
// of the key hash; 0x80 is empty, 0xFE is deleted. Chosen so a SIMD
// compare against (hash >> 25) finds candidates in a 16-byte group.
#define VYNE_MAP_EMPTY   0x80u
#define VYNE_MAP_DELETED 0xFEu

// TODO IMPLEMENT TOMBSTONES

// ============================================================================
// STRING BUILDER
// ----------------------------------------------------------------------------
// Amortized-O(1) append for workloads that accumulate a large string
// incrementally. Replaces the `s = s + x` idiom, which is O(n²) because
// every `+` allocates a fresh buffer and copies both operands.
//
// Lifetime: the builder struct and its buffer both live in the arena,
// exactly like an Array. Creating one outside any region means its
// storage survives until program exit; creating one inside a region
// means the whole thing is reclaimed at the rewind. The usage rule is
// the same rule the user already applies to arrays and maps.
//
// `size` does NOT include the trailing null. The buffer is always kept
// null-terminated at data[size] so build() can memcpy size+1 bytes.
// `capacity` is the allocated byte count, which is >= size+1 after any
// successful append. Both are size_t so a builder can in principle be
// larger than 2 GB on a 64-bit host.
// ============================================================================

typedef struct VyneStringBuilder {
    char*  data;
    size_t size;
    size_t capacity;
} VyneStringBuilder;

// ============================================================================
// STRUCT
// ============================================================================

typedef struct VyneField {
    uint32_t id;
    const char* name;
    VyneValue value;
} VyneField;

typedef struct VyneStruct {
    const char* type_name;
    VyneField* fields;
    int field_count;
    struct VyneMethodEntry** methods;
    int method_count;
    // Four-slot MRU field cache. Slot 0 is the most recently used
    // field index; slot 3 is the least recently used. Real struct
    // access rotates through a small set of fields
    // (`m.row; m.col; m.data; m.row; ...`), and a one-slot cache
    // thrashes on exactly that pattern.
    //
    // Each slot holds an index into `fields[]`, or -1 for empty.
    // Uninitialized slots are safe by construction: the get-side
    // bounds check rejects out-of-range indices, and an in-range
    // index whose field id matches the requested id is a correct
    // hit regardless of how it got into the slot.
    //
    // vyne_struct_set does NOT invalidate the cache. An in-place
    // update never moves a field; an append puts the new field at
    // index `field_count`, which no cached slot can already point
    // at (all cached indices are < field_count at insert time).
    int16_t field_cache[4];
} VyneStruct;

static inline bool vyne_values_equal(VyneValue a, VyneValue b);
static inline VyneValue vyne_to_string(VyneValue v);

static inline VyneValue vyne_array_get(VyneValue arr_val, VyneValue index_val);
static inline void vyne_array_set(VyneValue arr_val, VyneValue index_val, VyneValue rhs);
static inline void vyne_array_push(VyneValue arr_val, VyneValue val);
static inline VyneValue vyne_map_deepcopy(VyneValue mp);

// ============================================================================
// FUNCTION
// ============================================================================

typedef struct VyneFunction {
    int arity;
    int param_count;
    const char** param_names;
    VyneType* param_types;
    bool* is_reference;
    VyneValue* constants;
    uint8_t* bytecode;
    size_t bytecode_size;
    VyneMethodFn native_fn;
    bool is_native;
    const char* name;
    int frame_size;
    const char* expected_return_type;
} VyneFunction;

// ============================================================================
// METHOD TABLE
// ============================================================================

typedef struct VyneMethodEntry {
    const char* type_name;
    const char* method_name;
    VyneMethodFn fn;
} VyneMethodEntry;

static VyneMethodEntry g_method_table[VYNE_MAX_METHODS];
static int g_method_count = 0;

