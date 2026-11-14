// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
#pragma once
#include "../vyne_runtime.h"
#include "../vraylib/bridge.h"
#include "vmem.h"

static ArenaCheckpoint vraylib_frame_checkpoint;
static int vraylib_frame_active, vraylib_frame_vmem_top;
static int64_t vraylib_frame_session;
static VyneValue (*vraylib_previous_promoter)(VyneValue);

typedef struct VRExceptionCopy {
    const void *source;
    VyneValue value;
    struct VRExceptionCopy *next;
} VRExceptionCopy;
static VyneValue vraylib_escape_clone_impl(VyneValue v, VRExceptionCopy **memo) {
    const void *source=v.type==V_STRUCT ? (void *)v.as.strct : v.type==V_ARRAY ? (void *)v.as.arr : v.type==V_MAP ? (void *)v.as.map : NULL;
    if(source) {
        for(VRExceptionCopy *p=*memo;p;p=p->next) if(p->source==source && p->value.type==v.type) return p->value;
    }
    if (v.type == V_STRUCT) {
        VyneValue copy=vyne_struct_create(v.as.strct->type_name);
        VRExceptionCopy *p=arena_alloc(sizeof(*p)); *p=(VRExceptionCopy){source,copy,*memo}; *memo=p;
        for(int i=0;i<v.as.strct->field_count;++i) {
            VyneField f=v.as.strct->fields[i];
            vyne_struct_set(copy,f.id,f.name,vraylib_escape_clone_impl(f.value,memo));
        }
        return copy;
    }
    if(v.type==V_ARRAY) {
        VyneValue copy=vyne_array_create(v.as.arr->size);
        VRExceptionCopy *p=arena_alloc(sizeof(*p)); *p=(VRExceptionCopy){source,copy,*memo}; *memo=p;
        for(int i=0;i<v.as.arr->size;++i) copy.as.arr->elements[i]=vraylib_escape_clone_impl(v.as.arr->elements[i],memo);
        return copy;
    }
    if(v.type==V_MAP) {
        VyneValue copy=v;
        copy.as.map=(VyneMap *)arena_alloc(sizeof(VyneMap));
        *copy.as.map=*v.as.map;
        copy.as.map->entries=(VyneMapEntry *)arena_alloc(sizeof(VyneMapEntry)*v.as.map->capacity);
        VRExceptionCopy *p=arena_alloc(sizeof(*p)); *p=(VRExceptionCopy){source,copy,*memo}; *memo=p;
        for(int i=0;i<v.as.map->capacity;++i) {
            VyneMapEntry entry=v.as.map->entries[i];
            copy.as.map->entries[i]=entry;
            if(entry.key && entry.key!=VYNE_TOMBSTONE) {
                copy.as.map->entries[i].key=vyne_string(entry.key).as.str;
                copy.as.map->entries[i].value=vraylib_escape_clone_impl(entry.value,memo);
            }
        }
        return copy;
    }
    return vmem_deep_clone(v);
}
static VyneValue vraylib_escape_clone(VyneValue v) {
    VRExceptionCopy *memo=NULL;
    return vraylib_escape_clone_impl(v,&memo);
}
static VyneValue vraylib_promote_exception(VyneValue v) {
    if(g_persistent_depth>0) return vraylib_escape_clone(v);
    vyne_persistent_enter();
    VyneValue copy=vraylib_escape_clone(v);
    vyne_persistent_leave();
    return copy;
}
static void vraylib_finish_arena(void) {
    if(!vraylib_frame_active) return;
    while(g_persistent_depth>0) vyne_persistent_leave();
    if(g_arena.head) arena_rewind(vraylib_frame_checkpoint);
    for(int i=vraylib_frame_vmem_top;i<g_vmem_top;++i) g_vmem_slots[i].active=0;
    g_vmem_top=vraylib_frame_vmem_top;
    vraylib_frame_active=0;
    g_vyne_exception_promoter=vraylib_previous_promoter;
}

// Defined by the emitter using this compilation's StringPool ids. Native return
// fields cannot use string hashes: ordinary Vyne structs use interned numeric ids.
static uint32_t vraylib_field_id(const char *name);
static void vraylib_cleanup(void) {
    VyneRayResult result=vyne_ray_call(VR_SHUTDOWN,NULL,0);
    vyne_ray_dispose(result.value);
    vraylib_finish_arena();
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
    int64_t session_arg=n && args[0].type==V_INT64 ? args[0].as.i64 : 0;
    static int cleanup_registered;
    if (!cleanup_registered) { atexit(vraylib_cleanup); cleanup_registered=1; }
    if(id==-30 && !vraylib_frame_active && g_persistent_depth>0) {
        vyne_throw(vyne_string("Managed frames must start outside a persistent region"));
        return vyne_null();
    }
    // Input marshaling uses a call-local checkpoint. The returned value is boxed
    // only AFTER rewinding, so its lifetime follows the caller's enclosing region.
    ArenaCheckpoint checkpoint = arena_checkpoint();
    VyneRayValue *wire = (VyneRayValue *)arena_alloc(sizeof(VyneRayValue) * (n ? n : 1));
    for (size_t i = 0; i < n; ++i) wire[i] = vraylib_wire(args[i]);
    VyneRayResult result = vyne_ray_call(id, wire, n);
    arena_rewind(checkpoint);
    if (result.error) { vyne_throw(vyne_string(result.error)); return vyne_null(); }
    if(id==-30 || id==-31 || (id==-18 && session_arg==vraylib_frame_session) || id==VR_SHUTDOWN) vraylib_finish_arena();
    VyneValue value = vraylib_box(result.value);
    vyne_ray_dispose(result.value);
    if(id==-30 && value.type==V_BOOL && value.as.i64) {
        vraylib_frame_checkpoint=arena_checkpoint(); vraylib_frame_vmem_top=g_vmem_top;
        vraylib_frame_session=session_arg; vraylib_frame_active=1;
        vraylib_previous_promoter=g_vyne_exception_promoter;
        g_vyne_exception_promoter=vraylib_promote_exception;
    }
    return value;
}
#include "../vraylib/generated_runtime.inc"
