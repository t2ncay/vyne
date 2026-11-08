// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.

#pragma once
#include "arrays.h"
// ============================================================================
// TYPED ARRAYS
// ============================================================================\

static inline int64_t _vyne_next_pow2(int64_t n) {
    if (n <= 4) return 4;
    int64_t p = 8;
    while (p < n) p <<= 1;
    return p;
}

static inline VyneArray_f64 vyne_array_f64_create(int64_t n) {
    VyneArray_f64 a;
    a.size = n;
    a.cap  = _vyne_next_pow2(n);
    a.data = (double*)arena_alloc_aligned(
        sizeof(double) * (size_t)a.cap,
        VYNE_TYPED_ARRAY_ALIGN
    );
    memset(a.data, 0, sizeof(double) * (size_t)n);
    return a;
}

static inline VyneArray_i64 vyne_array_i64_create(int64_t n) {
    VyneArray_i64 a;
    a.size = n;
    a.cap  = _vyne_next_pow2(n);
    a.data = (int64_t*)arena_alloc_aligned(
        sizeof(int64_t) * (size_t)a.cap,
        VYNE_TYPED_ARRAY_ALIGN
    );
    memset(a.data, 0, sizeof(int64_t) * (size_t)n);
    return a;
}

static inline void vyne_array_f64_push(VyneArray_f64* a, double v) {
    if (VYNE_UNLIKELY(a->size >= a->cap)) {
        int64_t new_cap = a->cap * 2;
        double* old_data = a->data;
        arena_try_reclaim(old_data, sizeof(double) * (size_t)a->cap);
        double* new_data = (double*)arena_alloc_aligned(
            sizeof(double) * (size_t)new_cap,
            VYNE_TYPED_ARRAY_ALIGN
        );
        if (new_data != old_data) {
            memcpy(new_data, old_data, sizeof(double) * (size_t)a->size);
        }
        a->data = new_data;
        a->cap  = new_cap;
    }
    a->data[a->size++] = v;
}

static inline void vyne_array_i64_push(VyneArray_i64* a, int64_t v) {
    if (VYNE_UNLIKELY(a->size >= a->cap)) {
        int64_t new_cap = a->cap * 2;
        int64_t* old_data = a->data;

        arena_try_reclaim(old_data, sizeof(int64_t) * (size_t)a->cap);

        int64_t* new_data = (int64_t*)arena_alloc_aligned(
            sizeof(int64_t) * (size_t)new_cap,
            VYNE_TYPED_ARRAY_ALIGN
        );
        if (new_data != old_data) {
            memcpy(new_data, old_data, sizeof(int64_t) * (size_t)a->size);
        }
        a->data = new_data;
        a->cap  = new_cap;
    }
    a->data[a->size++] = v;
}

static inline double vyne_array_f64_get(const VyneArray_f64* a, int64_t i) {
    return (i < 0 || i >= a->size) ? 0.0 : a->data[i];
}

static inline int64_t vyne_array_i64_get(const VyneArray_i64* a, int64_t i) {
    return (i < 0 || i >= a->size) ? 0 : a->data[i];
}

static inline void vyne_array_f64_set(VyneArray_f64* a, int64_t i, double v) {
    if (i >= 0 && i < a->size) a->data[i] = v;
}

static inline void vyne_array_i64_set(VyneArray_i64* a, int64_t i, int64_t v) {
    if (i >= 0 && i < a->size) a->data[i] = v;
}

// --- Boundary conversions -------------------------------------------------
// Boxing: allocate a boxed VyneArray and copy elements in.
static inline VyneValue vyne_array_f64_to_value(const VyneArray_f64* a) {
    VyneArray_f64* w = (VyneArray_f64*)arena_alloc(sizeof(VyneArray_f64));
    w->data = a->data;
    w->size = a->size;
    w->cap  = a->cap;
    VyneValue v;
    v.type      = V_F64_ARRAY;
    v._reserved = 0;
    v.as.arr_f64 = w;
    return v;
}

static inline VyneValue vyne_array_i64_to_value(const VyneArray_i64* a) {
    VyneArray_i64* w = (VyneArray_i64*)arena_alloc(sizeof(VyneArray_i64));
    w->data = a->data;
    w->size = a->size;
    w->cap  = a->cap;
    VyneValue v;
    v.type      = V_I64_ARRAY;
    v._reserved = 0;
    v.as.arr_i64 = w;
    return v;
}

// --- Slice: half-open [lo, hi), matches vyne_slice_get's memcpy count ---
static inline VyneArray_f64 vyne_array_f64_slice(const VyneArray_f64* a,
                                                 int64_t lo, int64_t hi) {
    if (lo < 0) lo = 0;
    if (lo > a->size) lo = a->size;
    if (hi < 0) hi = 0;
    if (hi > a->size) hi = a->size;
    if (lo >= hi) return vyne_array_f64_create(0);

    int64_t n = hi - lo;
    VyneArray_f64 r = vyne_array_f64_create(n);
    memcpy(r.data, a->data + lo, sizeof(double) * (size_t)n);
    return r;
}

static inline VyneArray_i64 vyne_array_i64_slice(const VyneArray_i64* a,
                                                 int64_t lo, int64_t hi) {
    if (lo < 0) lo = 0;
    if (lo > a->size) lo = a->size;
    if (hi < 0) hi = 0;
    if (hi > a->size) hi = a->size;
    if (lo >= hi) return vyne_array_i64_create(0);

    int64_t n = hi - lo;
    VyneArray_i64 r = vyne_array_i64_create(n);
    memcpy(r.data, a->data + lo, sizeof(int64_t) * (size_t)n);
    return r;
}

// --- Unbox: VyneValue -> typed array.
//   V_F64_ARRAY -> O(1): struct copy, shares the data pointer.
//   V_I64_ARRAY -> O(N): elementwise int64 -> double conversion.
//   V_ARRAY     -> O(N): per-element tag check (existing behavior).
//   anything    -> empty array.
static inline VyneArray_f64 vyne_value_to_array_f64(VyneValue v) {
    if (v.type == V_F64_ARRAY) return *v.as.arr_f64;   // O(1)
    if (v.type == V_I64_ARRAY) {
        VyneArray_i64* s = v.as.arr_i64;
        VyneArray_f64 r = vyne_array_f64_create(s->size);
        for (int64_t i = 0; i < s->size; ++i)
            r.data[i] = (double)s->data[i];
        return r;
    }
    if (v.type != V_ARRAY) return vyne_array_f64_create(0);
    int64_t n = v.as.arr->size;
    VyneArray_f64 r = vyne_array_f64_create(n);
    for (int64_t i = 0; i < n; ++i) {
        VyneValue e = v.as.arr->elements[i];
        r.data[i] = (e.type == V_FLOAT64) ? e.as.f64 : (double)e.as.i64;
    }
    return r;
}

static inline VyneArray_i64 vyne_value_to_array_i64(VyneValue v) {
    if (v.type == V_I64_ARRAY) return *v.as.arr_i64;   // O(1)
    if (v.type == V_F64_ARRAY) {
        VyneArray_f64* s = v.as.arr_f64;
        VyneArray_i64 r = vyne_array_i64_create(s->size);
        for (int64_t i = 0; i < s->size; ++i)
            r.data[i] = (int64_t)s->data[i];
        return r;
    }
    if (v.type != V_ARRAY) return vyne_array_i64_create(0);
    int64_t n = v.as.arr->size;
    VyneArray_i64 r = vyne_array_i64_create(n);
    for (int64_t i = 0; i < n; ++i) {
        VyneValue e = v.as.arr->elements[i];
        r.data[i] = (e.type == V_INT64) ? e.as.i64 : (int64_t)e.as.f64;
    }
    return r;
}
