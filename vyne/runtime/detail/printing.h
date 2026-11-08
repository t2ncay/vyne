// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.

#pragma once
#include "structs.h"
// ============================================================================
// VALUE UTILITIES
// ============================================================================

static inline bool vyne_is_truthy(VyneValue v) {
    if (v.type == V_NULL) return false;
    if (v.type == V_BOOL || v.type == V_INT64) return v.as.i64 != 0;
    if (v.type == V_FLOAT64 || v.type == V_FLOAT32) return v.as.f64 != 0.0;
    return true;
}

static inline void _vyne_print_internal(VyneValue v) {
    switch (v.type) {
        case V_INT64:   printf("%lld", v.as.i64); break;
        case V_BOOL:    printf("%s", v.as.i64 ? "true" : "false"); break;
        case V_NULL:    printf("null"); break;
        case V_STRING:  printf("%s", v.as.str ? v.as.str : "null"); break;
        case V_FLOAT64: {
            char buf[64];
            sprintf(buf, "%g", v.as.f64);
            if (strchr(buf, '.') == NULL && strchr(buf, 'e') == NULL)
                strcat(buf, ".0");
            printf("%s", buf);
            break;
        }
        case V_FLOAT32: {
            char buf[64];
            sprintf(buf, "%g", (double)(float)v.as.f64);
            if (strchr(buf, '.') == NULL && strchr(buf, 'e') == NULL)
                strcat(buf, ".0");
            printf("%s", buf);
            break;
        }
        case V_F32_ARRAY: {
            printf("[");
            VyneArray_f32* a = v.as.arr_f32;
            for (int64_t i = 0; i < a->size; i++) {
                _vyne_print_internal(vyne_float32(a->data[i]));
                if (i < a->size - 1) printf(", ");
            }
            printf("]");
            break;
        }
        case V_ARRAY: {
            printf("[");
            VyneArray* arr = v.as.arr;
            for (int i = 0; i < arr->size; i++) {
                _vyne_print_internal(arr->elements[i]);
                if (i < arr->size - 1) printf(", ");
            }
            printf("]");
            break;
        }
        case V_F64_ARRAY: {
            printf("[");
            VyneArray_f64* a = v.as.arr_f64;
            for (int64_t i = 0; i < a->size; i++) {
                _vyne_print_internal(vyne_float(a->data[i]));
                if (i < a->size - 1) printf(", ");
            }
            printf("]");
            break;
        }
        case V_I64_ARRAY: {
            printf("[");
            VyneArray_i64* a = v.as.arr_i64;
            for (int64_t i = 0; i < a->size; i++) {
                _vyne_print_internal(vyne_int(a->data[i]));
                if (i < a->size - 1) printf(", ");
            }
            printf("]");
            break;
        }
        case V_MAP: {
            VyneMap* m = v.as.map;
            printf("{");
            int printed = 0;
            for (int i = 0; i < m->capacity && printed < m->size; i++) {
                const char* key = m->entries[i].key;
                if (key == NULL || key == VYNE_TOMBSTONE) continue;
                if (printed > 0) printf(", ");
                printf("\"%s\": ", key);
                _vyne_print_internal(m->entries[i].value);
                printed++;
            }
            printf("}");
            break;
        }
        case V_STRUCT: {
            VyneStruct* s = v.as.strct;
            printf("%s { ", s->type_name);
            for (int i = 0; i < s->field_count; i++) {
                printf("%s: ", s->fields[i].name);
                _vyne_print_internal(s->fields[i].value);
                if (i < s->field_count - 1) printf(", ");
            }
            printf(" }");
            break;
        }
        default: printf("<unknown>");
    }
}

static inline void vyne_out(VyneValue v) {
    _vyne_print_internal(v);
    printf("\n");
}

