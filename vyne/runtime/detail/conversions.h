// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.

#pragma once
#include "printing.h"
// ============================================================================
// TYPE CONVERSIONS
// ============================================================================

static inline VyneValue vyne_to_int(VyneValue v) {
    if (v.type == V_INT64) return v;
    if (v.type == V_FLOAT64 || v.type == V_FLOAT32)
        return vyne_int((int64_t)v.as.f64);
    if (v.type == V_STRING) return vyne_int(atoll(v.as.str));
    if (v.type == V_BOOL) return vyne_int(v.as.i64);
    return vyne_int(0);
}

static inline VyneValue vyne_to_float(VyneValue v) {
    if (v.type == V_FLOAT64) return v;
    if (v.type == V_FLOAT32) return vyne_float(v.as.f64);
    if (v.type == V_INT64) return vyne_float((double)v.as.i64);
    if (v.type == V_STRING) return vyne_float(atof(v.as.str));
    return vyne_float(0.0);
}

static inline VyneValue vyne_to_float32(VyneValue v) {
    if (v.type == V_FLOAT32) return v;
    if (v.type == V_FLOAT64) return vyne_float32((float)v.as.f64);
    if (v.type == V_INT64) return vyne_float32((float)v.as.i64);
    if (v.type == V_STRING) return vyne_float32((float)atof(v.as.str));
    return vyne_float32(0.0f);
}

static inline const char* vyne_get_type_name(VyneValue v) {
    switch (v.type) {
        case V_INT64:   return "Int64";
        case V_FLOAT64: return "Float64";
        case V_FLOAT32: return "Float32";
        case V_STRING:  return "String";
        case V_BOOL:    return "Boolean";
        case V_ARRAY:     return "Array";
        case V_F64_ARRAY: return "Array<Float64>";
        case V_F32_ARRAY: return "Array<Float32>";
        case V_I64_ARRAY: return "Array<Int64>";
        case V_MAP:       return "Map";
        case V_STRUCT:  return "Struct";
        case V_NULL:    return "Null";
        default:        return "Unknown";
    }
}

static inline int64_t vyne_get_sizeof(VyneValue v) {
    if (v.type == V_ARRAY)     return (int64_t)v.as.arr->size;
    if (v.type == V_F64_ARRAY) return v.as.arr_f64->size;
    if (v.type == V_F32_ARRAY) return v.as.arr_f32->size;
    if (v.type == V_I64_ARRAY) return v.as.arr_i64->size;
    if (v.type == V_MAP)       return (int64_t)v.as.map->size;
    if (v.type == V_STRING)    return (int64_t)strlen(v.as.str);
    if (v.type == V_STRUCT)    return (int64_t)v.as.strct->field_count;
    return 8;
}

static inline void _vyne_format_float(char* buf, double val) {
    sprintf(buf, "%g", val);
    if (strchr(buf, '.') == NULL && strchr(buf, 'e') == NULL)
        strcat(buf, ".0");
}

static inline VyneValue vyne_to_string(VyneValue v) {
    char buf[256];
    switch (v.type) {
        case V_INT64:   sprintf(buf, "%lld", v.as.i64); break;
        case V_FLOAT64: _vyne_format_float(buf, v.as.f64); break;
        case V_FLOAT32: _vyne_format_float(buf, (double)(float)v.as.f64); break;
        case V_BOOL:    strcpy(buf, v.as.i64 ? "true" : "false"); break;
        case V_NULL:    strcpy(buf, "null"); break;
        case V_STRING:  return v;
        case V_ARRAY: {
            VyneArray* arr = v.as.arr;
            int n = arr->size;
            // Render each element once: the measuring and copy passes used
            // to invoke vyne_to_string twice per element, allocating a
            // second string for every nested value.
            VyneValue* rendered = (VyneValue*)arena_alloc(
                sizeof(VyneValue) * (size_t)(n > 0 ? n : 1)
            );
            size_t total = 2; // '[' + ']'
            for (int i = 0; i < n; i++) {
                rendered[i] = vyne_to_string(arr->elements[i]);
                total += strlen(rendered[i].as.str);
                if (i > 0) total += 2; // ", "
            }
            char* tmp = (char*)arena_alloc(total + 1);
            size_t pos = 0;
            tmp[pos++] = '[';
            for (int i = 0; i < n; i++) {
                if (i > 0) { tmp[pos++] = ','; tmp[pos++] = ' '; }
                size_t slen = strlen(rendered[i].as.str);
                memcpy(tmp + pos, rendered[i].as.str, slen);
                pos += slen;
            }
            tmp[pos++] = ']';
            tmp[pos]   = '\0';
            return vyne_string_own(tmp);
        }
        case V_F64_ARRAY: {
            VyneArray_f64* a = v.as.arr_f64;
            size_t total = 3;
            for (int64_t i = 0; i < a->size; ++i) total += 24; // generous
            char* tmp = (char*)arena_alloc(total);
            size_t pos = 0;
            tmp[pos++] = '[';
            for (int64_t i = 0; i < a->size; ++i) {
                if (i > 0) { tmp[pos++] = ','; tmp[pos++] = ' '; }
                char fbuf[32];
                _vyne_format_float(fbuf, a->data[i]);
                size_t slen = strlen(fbuf);
                memcpy(tmp + pos, fbuf, slen); pos += slen;
            }
            tmp[pos++] = ']';
            tmp[pos]   = '\0';
            return vyne_string_own(tmp);
        }
        case V_F32_ARRAY: {
            VyneArray_f32* a = v.as.arr_f32;
            size_t total = 3;
            for (int64_t i = 0; i < a->size; ++i) total += 24;
            char* tmp = (char*)arena_alloc(total);
            size_t pos = 0;
            tmp[pos++] = '[';
            for (int64_t i = 0; i < a->size; ++i) {
                if (i > 0) { tmp[pos++] = ','; tmp[pos++] = ' '; }
                char fbuf[32];
                _vyne_format_float(fbuf, (double)a->data[i]);
                size_t slen = strlen(fbuf);
                memcpy(tmp + pos, fbuf, slen); pos += slen;
            }
            tmp[pos++] = ']';
            tmp[pos]   = '\0';
            return vyne_string_own(tmp);
        }
        case V_I64_ARRAY: {
            VyneArray_i64* a = v.as.arr_i64;
            size_t total = 3;
            for (int64_t i = 0; i < a->size; ++i) total += 24;
            char* tmp = (char*)arena_alloc(total);
            size_t pos = 0;
            tmp[pos++] = '[';
            for (int64_t i = 0; i < a->size; ++i) {
                if (i > 0) { tmp[pos++] = ','; tmp[pos++] = ' '; }
                int written = snprintf(tmp + pos, 24, "%lld", (long long)a->data[i]);
                if (written > 0) pos += (size_t)written;
            }
            tmp[pos++] = ']';
            tmp[pos]   = '\0';
            return vyne_string_own(tmp);
        }
        case V_MAP: {
            VyneMap* m = v.as.map;

            // Single pass: build one string per slot, store into a
            // small stack-allocated scratch array (or a pointer array
            // on the arena for larger maps).
            int n = m->size;
            VyneValue* vals = (VyneValue*)arena_alloc(sizeof(VyneValue) * (n > 0 ? n : 1));
            const char** keys = (const char**)arena_alloc(sizeof(char*) * (n > 0 ? n : 1));

            int k = 0;
            size_t total = 3;
            for (int i = 0; i < m->capacity && k < n; i++) {
                const char* key = m->entries[i].key;
                if (key == NULL || key == VYNE_TOMBSTONE) continue;
                keys[k]   = key;
                vals[k]   = vyne_to_string(m->entries[i].value);
                total    += strlen(key) + 6 + strlen(vals[k].as.str) + 2;
                k++;
            }

            char* tmp = (char*)arena_alloc(total);
            size_t pos = 0;
            tmp[pos++] = '{';
            for (int i = 0; i < n; i++) {
                if (i > 0) { tmp[pos++] = ','; tmp[pos++] = ' '; }
                tmp[pos++] = '"';
                size_t kl = strlen(keys[i]);
                memcpy(tmp + pos, keys[i], kl); pos += kl;
                tmp[pos++] = '"';
                tmp[pos++] = ':';
                tmp[pos++] = ' ';
                size_t vl = strlen(vals[i].as.str);
                memcpy(tmp + pos, vals[i].as.str, vl); pos += vl;
            }
            tmp[pos++] = '}';
            tmp[pos] = '\0';
            return vyne_string_own(tmp);
        }
        case V_STRUCT: {
            VyneStruct* s = v.as.strct;
            int n = s->field_count;
            VyneValue* rendered = (VyneValue*)arena_alloc(
                sizeof(VyneValue) * (size_t)(n > 0 ? n : 1)
            );
            size_t type_len = strlen(s->type_name);
            size_t total = type_len + 4;
            for (int i = 0; i < n; i++) {
                rendered[i] = vyne_to_string(s->fields[i].value);
                total += strlen(s->fields[i].name) + 2;
                total += strlen(rendered[i].as.str) + 2;
            }
            char* tmp = (char*)arena_alloc(total);
            size_t pos = 0;
            memcpy(tmp + pos, s->type_name, type_len);
            pos += type_len;
            tmp[pos++]=' '; tmp[pos++]='{'; tmp[pos++]=' ';
            for (int i = 0; i < n; i++) {
                if (i) { tmp[pos++]=','; tmp[pos++]=' '; }
                size_t kl = strlen(s->fields[i].name);
                memcpy(tmp + pos, s->fields[i].name, kl); pos += kl;
                tmp[pos++]=':'; tmp[pos++]=' ';
                size_t vl = strlen(rendered[i].as.str);
                memcpy(tmp + pos, rendered[i].as.str, vl); pos += vl;
            }
            tmp[pos++]=' '; tmp[pos++]='}'; tmp[pos]='\0';
            return vyne_string_own(tmp);
        }
        default: strcpy(buf, "[object]"); break;
    }
    return vyne_string(buf);
}
