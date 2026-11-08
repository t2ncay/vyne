// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.

// --- AFTER ---
#pragma once
#include "strings.h"

// Forward declarations for the typed-array container constructors and
// box/unbox boundary functions. These are defined in typed_arrays.h,
// which includes this file — so a prototype is the only way for the
// typed fast paths in vyne_array_deepcopy / vyne_array_push to see them
// before their definitions are processed.
//
// The struct types themselves (VyneArray_f64, VyneArray_i64) are fully
// defined in types.h, which is included earlier in the chain, so these
// prototypes are complete.
static inline VyneArray_f64 vyne_array_f64_create(int64_t n);
static inline VyneArray_f32 vyne_array_f32_create(int64_t n);
static inline VyneArray_i64 vyne_array_i64_create(int64_t n);
static inline VyneValue    vyne_array_f64_to_value(const VyneArray_f64* a);
static inline VyneValue    vyne_array_f32_to_value(const VyneArray_f32* a);
static inline VyneValue    vyne_array_i64_to_value(const VyneArray_i64* a);

// ============================================================================
// ARRAY OPERATIONS
//
// Arrays have three representations:
//   V_ARRAY     — boxed: VyneValue elements, dynamic per-slot type.
//   V_F64_ARRAY — typed: raw double* data, no per-element tag.
//   V_I64_ARRAY — typed: raw int64_t* data, no per-element tag.
//
// The typed variants make boxing a VyneArray_f64 / VyneArray_i64 an
// O(1) wrapper allocation instead of an O(N) per-element copy. Every
// operation below dispatches on all three cases.
// ============================================================================

// Size of any array representation. Returns 0 for non-arrays.
static inline int64_t _vyne_array_size(VyneValue v) {
    if (v.type == V_ARRAY)     return v.as.arr->size;
    if (v.type == V_F64_ARRAY) return v.as.arr_f64->size;
    if (v.type == V_F32_ARRAY) return v.as.arr_f32->size;
    if (v.type == V_I64_ARRAY) return v.as.arr_i64->size;
    return 0;
}

// Element at index i, boxed back into a VyneValue. Returns null on OOB
// or non-array. For typed arrays, the element is re-boxed on read; this
// is where per-element tag cost is paid, but only at call sites, not
// across the whole array.
static inline VyneValue _vyne_array_elem_at(VyneValue v, int64_t i) {
    if (v.type == V_ARRAY) {
        VyneArray* a = v.as.arr;
        if (i < 0 || i >= a->size) return vyne_null();
        return a->elements[i];
    }
    if (v.type == V_F64_ARRAY) {
        VyneArray_f64* a = v.as.arr_f64;
        if (i < 0 || i >= a->size) return vyne_null();
        return vyne_float(a->data[i]);
    }
    if (v.type == V_F32_ARRAY) {
        VyneArray_f32* a = v.as.arr_f32;
        if (i < 0 || i >= a->size) return vyne_null();
        return vyne_float32(a->data[i]);
    }
    if (v.type == V_I64_ARRAY) {
        VyneArray_i64* a = v.as.arr_i64;
        if (i < 0 || i >= a->size) return vyne_null();
        return vyne_int(a->data[i]);
    }
    return vyne_null();
}

static inline VyneValue vyne_array_get(VyneValue arr_val, VyneValue index_val) {
    if (index_val.type != V_INT64) return vyne_null();
    return _vyne_array_elem_at(arr_val, index_val.as.i64);
}

static inline void vyne_array_set(VyneValue arr_val, VyneValue index_val, VyneValue rhs) {
    if (index_val.type != V_INT64) return;
    int64_t idx = index_val.as.i64;

    if (arr_val.type == V_ARRAY) {
        VyneArray* arr = arr_val.as.arr;
        if (idx >= 0 && idx < arr->size) arr->elements[idx] = rhs;
        return;
    }
    if (arr_val.type == V_F64_ARRAY) {
        VyneArray_f64* a = arr_val.as.arr_f64;
        if (idx >= 0 && idx < a->size)
            a->data[idx] = (rhs.type == V_INT64) ? (double)rhs.as.i64
                         : (rhs.type == V_FLOAT64 || rhs.type == V_FLOAT32)
                               ? rhs.as.f64
                               : 0.0;
        return;
    }
    if (arr_val.type == V_F32_ARRAY) {
        VyneArray_f32* a = arr_val.as.arr_f32;
        if (idx >= 0 && idx < a->size)
            a->data[idx] = (rhs.type == V_INT64) ? (float)rhs.as.i64
                         : (rhs.type == V_FLOAT64 || rhs.type == V_FLOAT32)
                               ? (float)rhs.as.f64
                               : 0.0f;
        return;
    }
    if (arr_val.type == V_I64_ARRAY) {
        VyneArray_i64* a = arr_val.as.arr_i64;
        if (idx >= 0 && idx < a->size)
            a->data[idx] = (rhs.type == V_INT64) ? rhs.as.i64
                         : (rhs.type == V_FLOAT64 || rhs.type == V_FLOAT32)
                               ? (int64_t)rhs.as.f64
                               : 0;
        return;
    }
}

static inline void vyne_array_push(VyneValue arr_val, VyneValue val) {
    if (arr_val.type == V_ARRAY) {
        VyneArray* arr = arr_val.as.arr;
        if (VYNE_UNLIKELY(arr->size >= arr->capacity)) {
            int new_cap = arr->capacity * 2;
            VyneValue* old_elems = arr->elements;
            arena_try_reclaim(old_elems, sizeof(VyneValue) * arr->capacity);
            VyneValue* new_elems = (VyneValue*)arena_alloc(sizeof(VyneValue) * new_cap);
            if (new_elems != old_elems) {
                memcpy(new_elems, old_elems, sizeof(VyneValue) * arr->size);
            }
            arr->elements = new_elems;
            arr->capacity = new_cap;
        }
        arr->elements[arr->size++] = val;
        return;
    }
    if (arr_val.type == V_F64_ARRAY) {
        VyneArray_f64* a = arr_val.as.arr_f64;
        if (VYNE_UNLIKELY(a->size >= a->cap)) {
            int64_t new_cap = a->cap * 2;
            double* old_data = a->data;
            arena_try_reclaim(old_data, sizeof(double) * (size_t)a->cap);
            double* new_data = (double*)arena_alloc(sizeof(double) * (size_t)new_cap);
            if (new_data != old_data)
                memcpy(new_data, old_data, sizeof(double) * (size_t)a->size);
            a->data = new_data;
            a->cap  = new_cap;
        }
        a->data[a->size++] = (val.type == V_INT64) ? (double)val.as.i64
                           : (val.type == V_FLOAT64 || val.type == V_FLOAT32)
                                 ? val.as.f64
                                 : 0.0;
        return;
    }
    if (arr_val.type == V_F32_ARRAY) {
        VyneArray_f32* a = arr_val.as.arr_f32;
        if (VYNE_UNLIKELY(a->size >= a->cap)) {
            int64_t new_cap = a->cap * 2;
            float* old_data = a->data;
            arena_try_reclaim(old_data, sizeof(float) * (size_t)a->cap);
            float* new_data = (float*)arena_alloc(sizeof(float) * (size_t)new_cap);
            if (new_data != old_data)
                memcpy(new_data, old_data, sizeof(float) * (size_t)a->size);
            a->data = new_data;
            a->cap  = new_cap;
        }
        a->data[a->size++] = (val.type == V_INT64) ? (float)val.as.i64
                           : (val.type == V_FLOAT64 || val.type == V_FLOAT32)
                                 ? (float)val.as.f64
                                 : 0.0f;
        return;
    }
    if (arr_val.type == V_I64_ARRAY) {
        VyneArray_i64* a = arr_val.as.arr_i64;
        if (VYNE_UNLIKELY(a->size >= a->cap)) {
            int64_t new_cap = a->cap * 2;
            int64_t* old_data = a->data;
            arena_try_reclaim(old_data, sizeof(int64_t) * (size_t)a->cap);
            int64_t* new_data = (int64_t*)arena_alloc(sizeof(int64_t) * (size_t)new_cap);
            if (new_data != old_data)
                memcpy(new_data, old_data, sizeof(int64_t) * (size_t)a->size);
            a->data = new_data;
            a->cap  = new_cap;
        }
        a->data[a->size++] = (val.type == V_INT64) ? val.as.i64
                           : (val.type == V_FLOAT64 || val.type == V_FLOAT32)
                                 ? (int64_t)val.as.f64
                                 : 0;
        return;
    }
}

static inline VyneValue vyne_array_deepcopy(VyneValue arr) {
    if (arr.type == V_F64_ARRAY) {
        VyneArray_f64* s = arr.as.arr_f64;
        VyneArray_f64 copy = vyne_array_f64_create(s->size);
        memcpy(copy.data, s->data, sizeof(double) * (size_t)s->size);
        return vyne_array_f64_to_value(&copy);
    }
    if (arr.type == V_F32_ARRAY) {
        VyneArray_f32* s = arr.as.arr_f32;
        VyneArray_f32 copy = vyne_array_f32_create(s->size);
        memcpy(copy.data, s->data, sizeof(float) * (size_t)s->size);
        return vyne_array_f32_to_value(&copy);
    }
    if (arr.type == V_I64_ARRAY) {
        VyneArray_i64* s = arr.as.arr_i64;
        VyneArray_i64 copy = vyne_array_i64_create(s->size);
        memcpy(copy.data, s->data, sizeof(int64_t) * (size_t)s->size);
        return vyne_array_i64_to_value(&copy);
    }
    if (arr.type != V_ARRAY) return arr;
    VyneValue result = vyne_array_create(0);
    VyneArray* src = arr.as.arr;
    for (int i = 0; i < src->size; i++) {
        VyneValue elem = src->elements[i];
        if (elem.type == V_ARRAY) elem = vyne_array_deepcopy(elem);
        else if (elem.type == V_MAP) elem = vyne_map_deepcopy(elem);
        vyne_array_push(result, elem);
    }
    return result;
}

static inline VyneValue vyne_map_deepcopy(VyneValue mp) {
    if (mp.type != V_MAP) return mp;
    VyneValue result = vyne_map_create();
    VyneMap* src = mp.as.map;

    int copied = 0;
    for (int i = 0; i < src->capacity && copied < src->size; i++) {
        const char* key = src->entries[i].key;
        if (key == NULL || key == VYNE_TOMBSTONE) continue;
        VyneValue elem = src->entries[i].value;
        if (elem.type == V_ARRAY) elem = vyne_array_deepcopy(elem);
        else if (elem.type == V_MAP) elem = vyne_map_deepcopy(elem);
        vyne_map_set(result, vyne_string(key), elem);
        copied++;
    }
    return result;
}

static inline VyneValue vyne_array_pop(VyneValue arr_val) {
    if (arr_val.type == V_ARRAY) {
        VyneArray* arr = arr_val.as.arr;
        if (arr->size == 0) return vyne_null();
        return arr->elements[--arr->size];
    }
    if (arr_val.type == V_F64_ARRAY) {
        VyneArray_f64* a = arr_val.as.arr_f64;
        if (a->size == 0) return vyne_null();
        return vyne_float(a->data[--a->size]);
    }
    if (arr_val.type == V_F32_ARRAY) {
        VyneArray_f32* a = arr_val.as.arr_f32;
        if (a->size == 0) return vyne_null();
        return vyne_float32(a->data[--a->size]);
    }
    if (arr_val.type == V_I64_ARRAY) {
        VyneArray_i64* a = arr_val.as.arr_i64;
        if (a->size == 0) return vyne_null();
        return vyne_int(a->data[--a->size]);
    }
    return vyne_null();
}

static inline VyneValue vyne_array_reverse(VyneValue arr_val) {
    if (arr_val.type == V_ARRAY) {
        VyneArray* arr = arr_val.as.arr;
        if (arr->size <= 1) return arr_val;
        int start = 0, end = arr->size - 1;
        while (start < end) {
            VyneValue temp = arr->elements[start];
            arr->elements[start] = arr->elements[end];
            arr->elements[end] = temp;
            start++; end--;
        }
        return arr_val;
    }
    if (arr_val.type == V_F64_ARRAY) {
        VyneArray_f64* a = arr_val.as.arr_f64;
        if (a->size <= 1) return arr_val;
        int64_t start = 0, end = a->size - 1;
        while (start < end) {
            double t = a->data[start];
            a->data[start] = a->data[end];
            a->data[end] = t;
            start++; end--;
        }
        return arr_val;
    }
    if (arr_val.type == V_F32_ARRAY) {
        VyneArray_f32* a = arr_val.as.arr_f32;
        if (a->size <= 1) return arr_val;
        int64_t start = 0, end = a->size - 1;
        while (start < end) {
            float t = a->data[start];
            a->data[start] = a->data[end];
            a->data[end] = t;
            start++; end--;
        }
        return arr_val;
    }
    if (arr_val.type == V_I64_ARRAY) {
        VyneArray_i64* a = arr_val.as.arr_i64;
        if (a->size <= 1) return arr_val;
        int64_t start = 0, end = a->size - 1;
        while (start < end) {
            int64_t t = a->data[start];
            a->data[start] = a->data[end];
            a->data[end] = t;
            start++; end--;
        }
        return arr_val;
    }
    return arr_val;
}

static inline bool vyne_array_contains(VyneValue arr_val, VyneValue target) {
    if (arr_val.type == V_ARRAY) {
        VyneArray* arr = arr_val.as.arr;
        for (int i = 0; i < arr->size; i++)
            if (vyne_values_equal(arr->elements[i], target)) return true;
        return false;
    }
    if (arr_val.type == V_F64_ARRAY) {
        VyneArray_f64* a = arr_val.as.arr_f64;
        if (target.type != V_FLOAT64 && target.type != V_FLOAT32 &&
            target.type != V_INT64) return false;
        double t = (target.type == V_INT64) ? (double)target.as.i64 : target.as.f64;
        for (int64_t i = 0; i < a->size; ++i)
            if (a->data[i] == t) return true;
        return false;
    }
    if (arr_val.type == V_F32_ARRAY) {
        VyneArray_f32* a = arr_val.as.arr_f32;
        if (target.type != V_FLOAT64 && target.type != V_FLOAT32 &&
            target.type != V_INT64) return false;
        float t = (target.type == V_INT64) ? (float)target.as.i64
                                           : (float)target.as.f64;
        for (int64_t i = 0; i < a->size; ++i)
            if (a->data[i] == t) return true;
        return false;
    }
    if (arr_val.type == V_I64_ARRAY) {
        VyneArray_i64* a = arr_val.as.arr_i64;
        if (target.type != V_INT64 && target.type != V_FLOAT64 &&
            target.type != V_FLOAT32) return false;
        int64_t t = (target.type == V_INT64) ? target.as.i64
                                             : (int64_t)target.as.f64;
        for (int64_t i = 0; i < a->size; ++i)
            if (a->data[i] == t) return true;
        return false;
    }
    return false;
}

// ============================================================================
// EXTENDED ARRAY OPERATIONS
// ============================================================================

static inline VyneValue vyne_array_pop_front(VyneValue arr_val) {
    if (arr_val.type == V_ARRAY) {
        VyneArray* arr = arr_val.as.arr;
        if (arr->size == 0) return vyne_null();
        VyneValue front = arr->elements[0];
        for (int i = 1; i < arr->size; i++) arr->elements[i - 1] = arr->elements[i];
        arr->size--;
        return front;
    }
    if (arr_val.type == V_F64_ARRAY) {
        VyneArray_f64* a = arr_val.as.arr_f64;
        if (a->size == 0) return vyne_null();
        double front = a->data[0];
        memmove(a->data, a->data + 1, sizeof(double) * (size_t)(a->size - 1));
        a->size--;
        return vyne_float(front);
    }
    if (arr_val.type == V_F32_ARRAY) {
        VyneArray_f32* a = arr_val.as.arr_f32;
        if (a->size == 0) return vyne_null();
        float front = a->data[0];
        memmove(a->data, a->data + 1, sizeof(float) * (size_t)(a->size - 1));
        a->size--;
        return vyne_float32(front);
    }
    if (arr_val.type == V_I64_ARRAY) {
        VyneArray_i64* a = arr_val.as.arr_i64;
        if (a->size == 0) return vyne_null();
        int64_t front = a->data[0];
        memmove(a->data, a->data + 1, sizeof(int64_t) * (size_t)(a->size - 1));
        a->size--;
        return vyne_int(front);
    }
    return vyne_null();
}

static inline VyneValue vyne_array_back(VyneValue arr_val) {
    if (arr_val.type == V_ARRAY) {
        VyneArray* arr = arr_val.as.arr;
        if (arr->size == 0) return vyne_null();
        return arr->elements[arr->size - 1];
    }
    if (arr_val.type == V_F64_ARRAY) {
        VyneArray_f64* a = arr_val.as.arr_f64;
        if (a->size == 0) return vyne_null();
        return vyne_float(a->data[a->size - 1]);
    }
    if (arr_val.type == V_F32_ARRAY) {
        VyneArray_f32* a = arr_val.as.arr_f32;
        if (a->size == 0) return vyne_null();
        return vyne_float32(a->data[a->size - 1]);
    }
    if (arr_val.type == V_I64_ARRAY) {
        VyneArray_i64* a = arr_val.as.arr_i64;
        if (a->size == 0) return vyne_null();
        return vyne_int(a->data[a->size - 1]);
    }
    return vyne_null();
}

static inline bool vyne_array_delete(VyneValue arr_val, VyneValue target) {
    if (arr_val.type == V_ARRAY) {
        VyneArray* arr = arr_val.as.arr;
        for (int i = 0; i < arr->size; i++) {
            if (vyne_values_equal(arr->elements[i], target)) {
                for (int j = i + 1; j < arr->size; j++) arr->elements[j - 1] = arr->elements[j];
                arr->size--;
                return true;
            }
        }
        return false;
    }
    if (arr_val.type == V_F64_ARRAY) {
        VyneArray_f64* a = arr_val.as.arr_f64;
        if (target.type != V_FLOAT64 && target.type != V_FLOAT32 &&
            target.type != V_INT64) return false;
        double t = (target.type == V_INT64) ? (double)target.as.i64 : target.as.f64;
        for (int64_t i = 0; i < a->size; ++i) {
            if (a->data[i] == t) {
                memmove(a->data + i, a->data + i + 1,
                        sizeof(double) * (size_t)(a->size - i - 1));
                a->size--;
                return true;
            }
        }
        return false;
    }
    if (arr_val.type == V_F32_ARRAY) {
        VyneArray_f32* a = arr_val.as.arr_f32;
        if (target.type != V_FLOAT64 && target.type != V_FLOAT32 &&
            target.type != V_INT64) return false;
        float t = (target.type == V_INT64) ? (float)target.as.i64
                                           : (float)target.as.f64;
        for (int64_t i = 0; i < a->size; ++i) {
            if (a->data[i] == t) {
                memmove(a->data + i, a->data + i + 1,
                        sizeof(float) * (size_t)(a->size - i - 1));
                a->size--;
                return true;
            }
        }
        return false;
    }
    if (arr_val.type == V_I64_ARRAY) {
        VyneArray_i64* a = arr_val.as.arr_i64;
        if (target.type != V_INT64 && target.type != V_FLOAT64 &&
            target.type != V_FLOAT32) return false;
        int64_t t = (target.type == V_INT64) ? target.as.i64
                                             : (int64_t)target.as.f64;
        for (int64_t i = 0; i < a->size; ++i) {
            if (a->data[i] == t) {
                memmove(a->data + i, a->data + i + 1,
                        sizeof(int64_t) * (size_t)(a->size - i - 1));
                a->size--;
                return true;
            }
        }
        return false;
    }
    return false;
}

static inline VyneValue vyne_array_delete_at(VyneValue arr_val, int64_t idx) {
    if (arr_val.type == V_ARRAY) {
        VyneArray* arr = arr_val.as.arr;
        if (idx < 0 || idx >= arr->size) return vyne_null();
        VyneValue removed = arr->elements[idx];
        for (int i = (int)idx + 1; i < arr->size; i++) arr->elements[i - 1] = arr->elements[i];
        arr->size--;
        return removed;
    }
    if (arr_val.type == V_F64_ARRAY) {
        VyneArray_f64* a = arr_val.as.arr_f64;
        if (idx < 0 || idx >= a->size) return vyne_null();
        double removed = a->data[idx];
        memmove(a->data + idx, a->data + idx + 1,
                sizeof(double) * (size_t)(a->size - idx - 1));
        a->size--;
        return vyne_float(removed);
    }
    if (arr_val.type == V_F32_ARRAY) {
        VyneArray_f32* a = arr_val.as.arr_f32;
        if (idx < 0 || idx >= a->size) return vyne_null();
        float removed = a->data[idx];
        memmove(a->data + idx, a->data + idx + 1,
                sizeof(float) * (size_t)(a->size - idx - 1));
        a->size--;
        return vyne_float32(removed);
    }
    if (arr_val.type == V_I64_ARRAY) {
        VyneArray_i64* a = arr_val.as.arr_i64;
        if (idx < 0 || idx >= a->size) return vyne_null();
        int64_t removed = a->data[idx];
        memmove(a->data + idx, a->data + idx + 1,
                sizeof(int64_t) * (size_t)(a->size - idx - 1));
        a->size--;
        return vyne_int(removed);
    }
    return vyne_null();
}

static inline int _vyne_cmp_values(const void* a, const void* b) {
    VyneValue va = *(const VyneValue*)a;
    VyneValue vb = *(const VyneValue*)b;
    if ((va.type == V_INT64 || va.type == V_FLOAT64 || va.type == V_FLOAT32) &&
        (vb.type == V_INT64 || vb.type == V_FLOAT64 || vb.type == V_FLOAT32)) {
        double da = (va.type == V_INT64) ? (double)va.as.i64 : va.as.f64;
        double db = (vb.type == V_INT64) ? (double)vb.as.i64 : vb.as.f64;
        if (da < db) return -1;
        if (da > db) return  1;
        return 0;
    }
    return 0;
}

static inline int _vyne_cmp_num(VyneValue a, VyneValue b) {
    double da = (a.type == V_INT64) ? (double)a.as.i64 : a.as.f64;
    double db = (b.type == V_INT64) ? (double)b.as.i64 : b.as.f64;
    return (da < db) ? -1 : (da > db) ? 1 : 0;
}

static inline int _vyne_cmp_float(const void* a, const void* b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    return (fa < fb) ? -1 : (fa > fb) ? 1 : 0;
}

static inline void _vyne_insertion_sort(VyneValue* a, int n) {
    for (int i = 1; i < n; i++) {
        VyneValue k = a[i];
        int j = i - 1;
        while (j >= 0 && _vyne_cmp_num(a[j], k) > 0) {
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = k;
    }
}

// Comparators for typed arrays. qsort avoids the boxing cost entirely;
// for the boxed case, _vyne_cmp_values already exists.
static inline int _vyne_cmp_double(const void* pa, const void* pb) {
    double a = *(const double*)pa;
    double b = *(const double*)pb;
    if (a < b) return -1;
    if (a > b) return  1;
    return 0;
}

static inline int _vyne_cmp_int64(const void* pa, const void* pb) {
    int64_t a = *(const int64_t*)pa;
    int64_t b = *(const int64_t*)pb;
    if (a < b) return -1;
    if (a > b) return  1;
    return 0;
}

static inline void vyne_array_sort(VyneValue arr_val) {
    if (arr_val.type == V_ARRAY) {
        VyneArray* arr = arr_val.as.arr;
        if (arr->size <= 1) return;

        // Everything numeric? Fine. Mixed? Bail out with the old no-op behavior.
        for (int i = 0; i < arr->size; i++) {
            if (arr->elements[i].type != V_INT64 &&
                arr->elements[i].type != V_FLOAT64 &&
                arr->elements[i].type != V_FLOAT32) return;
        }

        if (arr->size < 16) {
            _vyne_insertion_sort(arr->elements, arr->size);
            return;
        }
        qsort(arr->elements, arr->size, sizeof(VyneValue), _vyne_cmp_values);
        return;
    }
    if (arr_val.type == V_F64_ARRAY) {
        VyneArray_f64* a = arr_val.as.arr_f64;
        if (a->size > 1)
            qsort(a->data, (size_t)a->size, sizeof(double), _vyne_cmp_double);
        return;
    }
    if (arr_val.type == V_F32_ARRAY) {
        VyneArray_f32* a = arr_val.as.arr_f32;
        if (a->size > 1)
            qsort(a->data, (size_t)a->size, sizeof(float), _vyne_cmp_float);
        return;
    }
    if (arr_val.type == V_I64_ARRAY) {
        VyneArray_i64* a = arr_val.as.arr_i64;
        if (a->size > 1)
            qsort(a->data, (size_t)a->size, sizeof(int64_t), _vyne_cmp_int64);
        return;
    }
}

static inline void vyne_array_clear(VyneValue arr_val) {
    if (arr_val.type == V_ARRAY)     { arr_val.as.arr->size = 0; return; }
    if (arr_val.type == V_F64_ARRAY) { arr_val.as.arr_f64->size = 0; return; }
    if (arr_val.type == V_F32_ARRAY) { arr_val.as.arr_f32->size = 0; return; }
    if (arr_val.type == V_I64_ARRAY) { arr_val.as.arr_i64->size = 0; return; }
}

static inline void vyne_array_place_all(VyneValue arr_val, VyneValue val, int64_t count) {
    if (count < 0) return;

    if (arr_val.type == V_ARRAY) {
        VyneArray* arr = arr_val.as.arr;
        if (count > arr->capacity) {
            int new_cap = count < 4 ? 4 : (int)count;
            VyneValue* old_elems = arr->elements;
            arena_try_reclaim(old_elems, sizeof(VyneValue) * arr->capacity);
            VyneValue* new_elems = (VyneValue*)arena_alloc(sizeof(VyneValue) * new_cap);
            arr->elements = new_elems;
            arr->capacity = new_cap;
        }
        for (int64_t i = 0; i < count; i++) arr->elements[i] = val;
        arr->size = (int)count;
        return;
    }
    if (arr_val.type == V_F64_ARRAY) {
        VyneArray_f64* a = arr_val.as.arr_f64;
        if (count > a->cap) {
            int64_t new_cap = count < 4 ? 4 : count;
            double* old_data = a->data;
            arena_try_reclaim(old_data, sizeof(double) * (size_t)a->cap);
            double* new_data = (double*)arena_alloc(sizeof(double) * (size_t)new_cap);
            a->data = new_data;
            a->cap  = new_cap;
        }
        double v = (val.type == V_INT64) ? (double)val.as.i64
                 : (val.type == V_FLOAT64 || val.type == V_FLOAT32)
                       ? val.as.f64
                       : 0.0;
        for (int64_t i = 0; i < count; i++) a->data[i] = v;
        a->size = count;
        return;
    }
    if (arr_val.type == V_F32_ARRAY) {
        VyneArray_f32* a = arr_val.as.arr_f32;
        if (count > a->cap) {
            int64_t new_cap = count < 4 ? 4 : count;
            float* old_data = a->data;
            arena_try_reclaim(old_data, sizeof(float) * (size_t)a->cap);
            float* new_data = (float*)arena_alloc(sizeof(float) * (size_t)new_cap);
            a->data = new_data;
            a->cap  = new_cap;
        }
        float v = (val.type == V_INT64) ? (float)val.as.i64
                : (val.type == V_FLOAT64 || val.type == V_FLOAT32)
                      ? (float)val.as.f64
                      : 0.0f;
        for (int64_t i = 0; i < count; i++) a->data[i] = v;
        a->size = count;
        return;
    }
    if (arr_val.type == V_I64_ARRAY) {
        VyneArray_i64* a = arr_val.as.arr_i64;
        if (count > a->cap) {
            int64_t new_cap = count < 4 ? 4 : count;
            int64_t* old_data = a->data;
            arena_try_reclaim(old_data, sizeof(int64_t) * (size_t)a->cap);
            int64_t* new_data = (int64_t*)arena_alloc(sizeof(int64_t) * (size_t)new_cap);
            a->data = new_data;
            a->cap  = new_cap;
        }
        int64_t v = (val.type == V_INT64) ? val.as.i64
                  : (val.type == V_FLOAT64 || val.type == V_FLOAT32)
                        ? (int64_t)val.as.f64
                        : 0;
        for (int64_t i = 0; i < count; i++) a->data[i] = v;
        a->size = count;
        return;
    }
}

