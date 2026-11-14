// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
#pragma once
#include "../vyne_runtime.h"
#include "../vraylib/bridge.h"

// Defined by the emitter using this compilation's StringPool ids. Native return
// fields cannot use string hashes: ordinary Vyne structs use interned numeric ids.
static uint32_t vraylib_field_id(const char *name);
static void vraylib_cleanup(void) {
    VyneRayResult result=vyne_ray_call(VR_SHUTDOWN,NULL,0);
    vyne_ray_dispose(result.value);
}

static inline VyneRayValue vraylib_wire(VyneValue v) {
    VyneRayValue r = {0};
    switch (v.type) {
    case V_NULL: break;
    case V_INT64: r.kind = VR_INT; r.integer = v.as.i64; break;
    case V_BOOL: r.kind = VR_BOOL; r.integer = v.as.i64; break;
    case V_FLOAT64: case V_FLOAT32: r.kind = VR_REAL; r.real = v.as.f64; break;
    case V_STRING: r.kind = VR_TEXT; r.text = v.as.str; break;
    case V_STRUCT:
        r.kind = VR_RECORD; r.text = v.as.strct->type_name; r.count = v.as.strct->field_count;
        r.items = (VyneRayValue *)arena_alloc(sizeof(VyneRayValue) * r.count);
        for (size_t i = 0; i < r.count; ++i) {
            r.items[i] = vraylib_wire(v.as.strct->fields[i].value);
            r.items[i].name = v.as.strct->fields[i].name;
        }
        break;
    case V_ARRAY: case V_F64_ARRAY: case V_F32_ARRAY: case V_I64_ARRAY:
        r.kind = VR_LIST;
        r.count = v.type == V_ARRAY ? v.as.arr->size : v.type == V_F64_ARRAY ? v.as.arr_f64->size : v.type == V_F32_ARRAY ? v.as.arr_f32->size : v.as.arr_i64->size;
        r.items = (VyneRayValue *)arena_alloc(sizeof(VyneRayValue) * r.count);
        for (size_t i = 0; i < r.count; ++i) {
            VyneValue item = v.type == V_ARRAY ? v.as.arr->elements[i] : v.type == V_I64_ARRAY ? vyne_int(v.as.arr_i64->data[i]) : vyne_float(v.type == V_F64_ARRAY ? v.as.arr_f64->data[i] : v.as.arr_f32->data[i]);
            r.items[i] = vraylib_wire(item);
        }
        break;
    default: fprintf(stderr, "vraylib: unsupported argument type\n"); exit(1);
    }
    return r;
}
static inline VyneValue vraylib_box(VyneRayValue v) {
    switch (v.kind) {
    case VR_INT: case VR_HANDLE: return vyne_int(v.integer);
    case VR_REAL: return vyne_float(v.real);
    case VR_BOOL: return vyne_bool(v.integer != 0);
    case VR_TEXT: return vyne_string(v.text);
    case VR_LIST: {
        VyneValue r = vyne_array_create((int)v.count);
        for (size_t i = 0; i < v.count; ++i) r.as.arr->elements[i] = vraylib_box(v.items[i]);
        return r;
    }
    case VR_RECORD: {
        VyneValue r = vyne_struct_create(v.text);
        for (size_t i = 0; i < v.count; ++i)
            vyne_struct_set(r, vraylib_field_id(v.items[i].name), v.items[i].name, vraylib_box(v.items[i]));
        return r;
    }
    default: return vyne_null();
    }
}
static inline VyneValue vraylib_invoke(int id, size_t n, const VyneValue *args) {
    static int cleanup_registered;
    if (!cleanup_registered) { atexit(vraylib_cleanup); cleanup_registered=1; }
    // Input marshaling uses a call-local checkpoint. The returned value is boxed
    // only AFTER rewinding, so its lifetime follows the caller's enclosing region.
    ArenaCheckpoint checkpoint = arena_checkpoint();
    VyneRayValue *wire = (VyneRayValue *)arena_alloc(sizeof(VyneRayValue) * (n ? n : 1));
    for (size_t i = 0; i < n; ++i) wire[i] = vraylib_wire(args[i]);
    VyneRayResult result = vyne_ray_call(id, wire, n);
    arena_rewind(checkpoint);
    if (result.error) { vyne_throw(vyne_string(result.error)); return vyne_null(); }
    VyneValue value = vraylib_box(result.value);
    vyne_ray_dispose(result.value);
    return value;
}
#include "../vraylib/generated_runtime.inc"
