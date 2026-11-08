// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.

#pragma once
#include "conversions.h"

static inline bool vyne_values_equal(VyneValue a, VyneValue b) {
    if (a.type != b.type) {
        bool an = (a.type == V_INT64 || a.type == V_FLOAT64 ||
                   a.type == V_FLOAT32);
        bool bn = (b.type == V_INT64 || b.type == V_FLOAT64 ||
                   b.type == V_FLOAT32);
        if (an && bn) {
            double av = (a.type == V_INT64) ? (double)a.as.i64 : a.as.f64;
            double bv = (b.type == V_INT64) ? (double)b.as.i64 : b.as.f64;
            return av == bv;
        }
        return false;
    }
    switch (a.type) {
        case V_NULL:    return true;
        case V_BOOL:
        case V_INT64:   return a.as.i64 == b.as.i64;
        case V_FLOAT64:
        case V_FLOAT32: return a.as.f64 == b.as.f64;
        case V_F32_ARRAY: {
            VyneArray_f32* af = a.as.arr_f32;
            VyneArray_f32* bf = b.as.arr_f32;
            if (af->size != bf->size) return false;
            for (int64_t i = 0; i < af->size; ++i)
                if (af->data[i] != bf->data[i]) return false;
            return true;
        }
        case V_STRING:  return strcmp(a.as.str, b.as.str) == 0;
        case V_F64_ARRAY: {
            VyneArray_f64* af = a.as.arr_f64;
            VyneArray_f64* bf = b.as.arr_f64;
            if (af->size != bf->size) return false;
            for (int64_t i = 0; i < af->size; ++i)
                if (af->data[i] != bf->data[i]) return false;
            return true;
        }
        case V_I64_ARRAY: {
            VyneArray_i64* ai = a.as.arr_i64;
            VyneArray_i64* bi = b.as.arr_i64;
            if (ai->size != bi->size) return false;
            for (int64_t i = 0; i < ai->size; ++i)
                if (ai->data[i] != bi->data[i]) return false;
            return true;
        }
        case V_MAP: {
            VyneMap* am = a.as.map;
            VyneMap* bm = b.as.map;
            if (am == bm) return true;
            if (am->size != bm->size) return false;
            int checked = 0;
            for (int i = 0; i < am->capacity && checked < am->size; i++) {
                const char* key = am->entries[i].key;
                if (key == NULL || key == VYNE_TOMBSTONE) continue;
                int idx = _vyne_map_find(bm, key);
                if (idx < 0) return false;
                if (!vyne_values_equal(am->entries[i].value, bm->entries[idx].value))
                    return false;
                checked++;
            }
            return true;
        }
        default:        return false;
    }
}

// ============================================================================
