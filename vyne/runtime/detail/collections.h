// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.

#pragma once
#include "typed_arrays.h"

// ============================================================================
// SLICE — base[lo..hi], inclusive on both ends
// ============================================================================

static inline VyneValue vyne_slice_get(VyneValue base, VyneValue lo_v, VyneValue hi_v) {
    bool lo_open = (lo_v.type == V_NULL);
    bool hi_open = (hi_v.type == V_NULL);

    int64_t lo = lo_open ? 0
                         : ((lo_v.type == V_INT64) ? lo_v.as.i64 : (int64_t)lo_v.as.f64);
    // hi defaults to INT64_MAX; the per-branch clamp below brings it
    // down to len for open-ended slices.
    int64_t hi = hi_open ? INT64_MAX
                         : ((hi_v.type == V_INT64) ? hi_v.as.i64 : (int64_t)hi_v.as.f64);

    if (base.type == V_F64_ARRAY) {
        VyneArray_f64* a = base.as.arr_f64;
        int64_t len = a->size;
        if (lo < 0) lo = 0;
        if (lo > len) lo = len;
        if (hi < 0) hi = 0;
        if (hi > len) hi = len;
        if (lo >= hi) return vyne_array_f64_to_value(&(VyneArray_f64){NULL, 0, 0});
        VyneArray_f64 r = vyne_array_f64_slice(a, lo, hi);
        return vyne_array_f64_to_value(&r);
    }
    if (base.type == V_F32_ARRAY) {
        VyneArray_f32* a = base.as.arr_f32;
        int64_t len = a->size;
        if (lo < 0) lo = 0;
        if (lo > len) lo = len;
        if (hi < 0) hi = 0;
        if (hi > len) hi = len;
        if (lo >= hi) return vyne_array_f32_to_value(&(VyneArray_f32){NULL, 0, 0});
        VyneArray_f32 r = vyne_array_f32_slice(a, lo, hi);
        return vyne_array_f32_to_value(&r);
    }
    if (base.type == V_I64_ARRAY) {
        VyneArray_i64* a = base.as.arr_i64;
        int64_t len = a->size;
        if (lo < 0) lo = 0;
        if (lo > len) lo = len;
        if (hi < 0) hi = 0;
        if (hi > len) hi = len;
        if (lo >= hi) return vyne_array_i64_to_value(&(VyneArray_i64){NULL, 0, 0});
        VyneArray_i64 r = vyne_array_i64_slice(a, lo, hi);
        return vyne_array_i64_to_value(&r);
    }
    if (base.type == V_ARRAY) {
        VyneArray* a = base.as.arr;
        int64_t len = a->size;

        if (lo < 0) lo = 0;
        if (lo > len) lo = len;
        if (hi < 0) hi = 0;
        if (hi > len) hi = len;

        if (lo >= hi) return vyne_array_create(0);

        int n = (int)(hi - lo);
        VyneValue r = vyne_array_create(n);
        memcpy(r.as.arr->elements, a->elements + lo, sizeof(VyneValue) * n);
        return r;
    }

    if (base.type == V_STRING) {
        const char* s = base.as.str;
        int64_t len = (int64_t)strlen(s);

        if (lo < 0) lo = 0;
        if (lo > len) lo = len;
        if (hi < 0) hi = 0;
        if (hi > len) hi = len;

        if (lo >= hi) return vyne_string("");

        int64_t n = hi - lo;
        char* buf = (char*)arena_alloc((size_t)n + 1);
        memcpy(buf, s + lo, (size_t)n);
        buf[n] = '\0';
        return vyne_string(buf);
    }

    return vyne_null();
}

// Polymorphic dispatch for methods that exist on both arrays and maps
static inline VyneValue vyne_delete_any(VyneValue recv, VyneValue val) {
    if (recv.type == V_ARRAY || recv.type == V_F64_ARRAY ||
        recv.type == V_F32_ARRAY || recv.type == V_I64_ARRAY) {
        return vyne_bool(vyne_array_delete(recv, val));
    }
    if (recv.type == V_MAP && val.type == V_STRING) {
        vyne_map_delete(recv, val);
        return vyne_bool(true);
    }
    return vyne_bool(false);
}

static inline void vyne_clear_any(VyneValue recv) {
    if (recv.type == V_ARRAY || recv.type == V_F64_ARRAY ||
        recv.type == V_F32_ARRAY || recv.type == V_I64_ARRAY)
        vyne_array_clear(recv);
    else if (recv.type == V_MAP)
        vyne_map_clear(recv);
}

// ============================================================================
// RANGE
// ============================================================================

static inline VyneValue vyne_range_create(VyneValue start_val, VyneValue end_val) {
    if ((start_val.type == V_INT64 || start_val.type == V_FLOAT64 ||
         start_val.type == V_FLOAT32) &&
        (end_val.type == V_INT64 || end_val.type == V_FLOAT64 ||
         end_val.type == V_FLOAT32)) {

        if (start_val.type == V_INT64 && end_val.type == V_INT64) {
            int64_t start = start_val.as.i64;
            int64_t end   = end_val.as.i64;
            if (start <= end) {
                int64_t n64 = end - start + 1;
                if (n64 > INT32_MAX) return vyne_array_create(0);
                int n = (int)n64;

                VyneValue res = vyne_array_create(n);
                VyneValue* elems = res.as.arr->elements;
                for (int i = 0; i < n; ++i)
                    elems[i] = vyne_int(start + i);
                return res;
            }
        } else {
            double start = (start_val.type == V_INT64) ? (double)start_val.as.i64
                                                       : start_val.as.f64;
            double end   = (end_val.type == V_INT64) ? (double)end_val.as.i64
                                                     : end_val.as.f64;
            if (start <= end) {
                // Same conservative capacity guess; float ranges are rare
                // and we can't size them exactly without counting first.
                int n = (int)(end - start + 1.0);
                if (n < 0) n = 0;

                VyneValue res = vyne_array_create(n);
                VyneValue* elems = res.as.arr->elements;
                double v = start;
                for (int i = 0; i < n && v <= end; ++i, v += 1.0)
                    elems[i] = vyne_float(v);
                return res;
            }
        }
    }
    return vyne_array_create(0);
}

