// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
#include "bridge.h"
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../vendor/raylib/src/raylib.h"
#define RAYMATH_STATIC_INLINE
#include "../../../vendor/raylib/src/raymath.h"
#include "../../../vendor/raylib/src/rlgl.h"

typedef VyneRayValue VR;
typedef void (*VRDrop)(void *, size_t);
typedef struct {
    void *data;
    const char *type;
    size_t count, stride;
    uint32_t generation;
    int64_t owner;
    VRDrop drop;
    int allocated;
} VRSlot;
static VRSlot *vr_slots;
static size_t vr_capacity;
static jmp_buf vr_jump;
static char vr_error[256];
static const char *vr_function;
static VRSlot *vr_slot(int64_t id);
typedef struct VRDependency { int64_t from,to; int key; struct VRDependency *next; } VRDependency;
static VRDependency *vr_dependencies;
static void *vr_active_automation, *vr_active_batch;

// All entry points are main-thread only, matching raylib's graphics context.
// Temporary pointer conversions are freed on both normal and error paths.
typedef struct VRTemp { void *data; struct VRTemp *next; } VRTemp;
static VRTemp *vr_temps;
static void vr_fail(const char *message) {
    snprintf(vr_error, sizeof(vr_error), "vraylib.%s: %s", vr_function, message);
    longjmp(vr_jump, 1);
}
static void *vr_alloc(size_t n) {
    void *p = calloc(1, n ? n : 1);
    if (!p) vr_fail("native allocation failed");
    return p;
}
static void *vr_temp(size_t n) {
    VRTemp *t = vr_alloc(sizeof(*t));
    t->data = vr_alloc(n); t->next = vr_temps; vr_temps = t;
    return t->data;
}
static void vr_clear_temps(void) {
    while (vr_temps) {
        VRTemp *t = vr_temps; vr_temps = t->next;
        free(t->data); free(t);
    }
}
static VR vr_null(void) { return (VR){0}; }
static VR vr_int(int64_t n) { VR v = {0}; v.kind = VR_INT; v.integer = n; return v; }
static VR vr_real(double n) { VR v = {0}; v.kind = VR_REAL; v.real = n; return v; }
static VR vr_bool(int n) { VR v = vr_int(n != 0); v.kind = VR_BOOL; return v; }
static int64_t vr_integer(VR v) {
    if (v.kind == VR_INT || v.kind == VR_HANDLE || v.kind == VR_BOOL) return v.integer;
    if (v.kind == VR_REAL && isfinite(v.real) && v.real >= -9223372036854775808.0 && v.real < 9223372036854775808.0) return (int64_t)v.real;
    vr_fail("expected an integer"); return 0;
}
static double vr_number(VR v) {
    if (v.kind == VR_REAL) return v.real;
    return (double)vr_integer(v);
}
static int vr_i32(VR v) {
    int64_t n = vr_integer(v);
    if (n < INT_MIN || n > INT_MAX) vr_fail("integer is outside the C int range");
    return (int)n;
}
static unsigned int vr_u32(VR v) {
    int64_t n = vr_integer(v);
    if (n > UINT_MAX) {
        VRSlot *s=vr_slot(n);
        if (!strncmp(s->type,"GL:",3)) return *(unsigned int *)s->data;
    }
    if (n < 0 || (uint64_t)n > UINT_MAX) vr_fail("integer is outside the C unsigned int range");
    return (unsigned int)n;
}
static const char *vr_string(VR v) {
    if (v.kind == VR_NULL) return NULL;
    if (v.kind==VR_INT || v.kind==VR_HANDLE) {
        VRSlot *s=vr_slot(v.integer);
        if(strcmp(s->type,"char") || !memchr(s->data,0,s->count)) vr_fail("expected a terminated character buffer");
        return s->data;
    }
    if (v.kind != VR_TEXT || !v.text) vr_fail("expected a string or null");
    return v.text;
}
static const char *vr_required_string(VR v) {
    const char *s=vr_string(v);
    if(!s) vr_fail("required string argument is null");
    return s;
}
static VR vr_text(const char *s) {
    if (!s) return vr_null();
    VR v = {0}; v.kind = VR_TEXT;
    size_t n = strlen(s) + 1;
    char *copy = vr_alloc(n); memcpy(copy, s, n); v.text = copy;
    return v;
}
static VR vr_list(size_t n, const char *type) {
    VR v = {0}; v.kind = type ? VR_RECORD : VR_LIST; v.text = type;
    v.count = n; v.items = vr_alloc(sizeof(VR) * n); return v;
}
static VR vr_member(VR v, const char *name, size_t index) {
    if (v.kind != VR_RECORD && v.kind != VR_LIST) vr_fail("expected a value struct or array");
    if (v.kind == VR_RECORD) {
        for (size_t i = 0; i < v.count; ++i)
            if (v.items[i].name && !strcmp(name, v.items[i].name)) return v.items[i];
        vr_fail("value struct is missing a required field");
    }
    if (index >= v.count) vr_fail("value array is too short");
    return v.items[index];
}
static int64_t vr_id(size_t i) { return ((int64_t)vr_slots[i].generation << 32) | (int64_t)(i + 1); }
static VRSlot *vr_slot(int64_t id) {
    uint32_t low = (uint32_t)id;
    if (!low || low > vr_capacity) vr_fail("invalid native handle");
    VRSlot *s = &vr_slots[low - 1];
    if (!s->data || s->generation != (uint32_t)((uint64_t)id >> 32)) vr_fail("stale native handle");
    if (s->owner) (void)vr_slot(s->owner);
    return s;
}
static void vr_depend(int64_t from,int64_t to,int key) {
    (void)vr_slot(from); (void)vr_slot(to);
    for(VRDependency *d=vr_dependencies;d;d=d->next)
        if(d->from==from && d->key==key) { d->to=to; return; }
    VRDependency *d=vr_alloc(sizeof(*d));
    d->from=from; d->to=to; d->key=key; d->next=vr_dependencies; vr_dependencies=d;
}
static int vr_descendant(int64_t child,int64_t parent) {
    while(child) { if(child==parent) return 1; child=vr_slot(child)->owner; }
    return 0;
}
static VR vr_handle(void *data, const char *type, size_t count, size_t stride, VRDrop drop, int allocated, int64_t owner) {
    if (!data) return vr_int(0);
    if (!allocated && !drop) {
        for (size_t j=0; j<vr_capacity; ++j)
            if (vr_slots[j].data==data && vr_slots[j].owner==owner && !strcmp(vr_slots[j].type,type)) {
                VR v=vr_int(vr_id(j)); v.kind=VR_HANDLE; return v;
            }
    }
    size_t i = 0;
    while (i < vr_capacity && vr_slots[i].data) ++i;
    if (i == vr_capacity) {
        size_t old = vr_capacity, cap = old ? old * 2 : 64;
        VRSlot *p = realloc(vr_slots, cap * sizeof(*p));
        if (!p) vr_fail("handle registry allocation failed");
        vr_slots = p; memset(p + old, 0, (cap - old) * sizeof(*p)); vr_capacity = cap;
    }
    VRSlot *s = &vr_slots[i];
    if (++s->generation == 0) ++s->generation;
    s->data = data; s->type = type; s->count = count; s->stride = stride;
    s->drop = drop; s->allocated = allocated; s->owner = owner;
    VR v = vr_int(vr_id(i)); v.kind = VR_HANDLE; return v;
}
static void *vr_checked(VR v, const char *type, size_t count) {
    if (v.kind == VR_NULL || ((v.kind == VR_INT || v.kind == VR_HANDLE) && !v.integer)) return NULL;
    VRSlot *s = vr_slot(vr_integer(v));
    if (strcmp(type, "void") && strcmp(type, s->type)) vr_fail("native handle has the wrong element type");
    if (s->count < count) vr_fail("native buffer is too short");
    return s->data;
}
static void vr_invalidate(int64_t id) {
    for (size_t i = 0; i < vr_capacity; ++i)
        if (vr_slots[i].data && vr_slots[i].owner == id) vr_invalidate(vr_id(i));
    VRSlot *s = vr_slot(id);
    if (s->owner && s->allocated) free(s->data);
    s->data = NULL; s->drop = NULL; s->owner = 0;
    VRDependency **next=&vr_dependencies;
    while(*next) {
        VRDependency *d=*next;
        if(d->from==id || d->to==id) { *next=d->next; free(d); } else next=&d->next;
    }
}
static void vr_release(int64_t id, VRDrop expected) {
    VRSlot *s = vr_slot(id);
    if ((s->owner && !s->drop) || (!s->drop && !s->allocated)) vr_fail("borrowed resources cannot be unloaded");
    if (expected && s->drop != expected) vr_fail("unload function does not match the resource owner");
    for(VRDependency *d=vr_dependencies;d;d=d->next)
        if(vr_descendant(d->to,id) && !vr_descendant(d->from,id)) vr_fail("resource is referenced by a live model, material, or framebuffer");
    // Owned implicit subresources (model-imported textures and sound aliases)
    // are destroyed before their owner. Plain borrowed views are invalidated.
    for (size_t i=0; i<vr_capacity; ++i)
        if (vr_slots[i].data && vr_slots[i].owner==id && vr_slots[i].drop) vr_release(vr_id(i),NULL);
    s=vr_slot(id);
    void *p = s->data; size_t n = s->count; VRDrop drop = s->drop; int allocated = s->allocated;
    if(p==vr_active_automation) { SetAutomationEventList(NULL); vr_active_automation=NULL; }
    if(p==vr_active_batch) { rlSetRenderBatchActive(NULL); vr_active_batch=NULL; }
    if (drop) drop(p, n);
    if (allocated) free(p);
    s->allocated=0;
    vr_invalidate(id);
}
static void vr_memory_drop(void *p, size_t count) { (void)count; MemFree(p); }

static void vr_span(VR v, size_t n, int bytes, int nullable) {
    if (v.kind==VR_NULL || ((v.kind==VR_INT || v.kind==VR_HANDLE) && !v.integer)) {
        if (n && !nullable) vr_fail("required pointer argument is null");
        return;
    }
    if (v.kind==VR_TEXT) {
        if (n>strlen(v.text)+1) vr_fail("string input is too short");
        return;
    }
    size_t have;
    if(v.kind==VR_LIST) have=v.count;
    else {
        VRSlot *s=vr_slot(vr_integer(v));
        if(bytes && s->count>SIZE_MAX/s->stride) vr_fail("buffer size overflow");
        have=s->count*(bytes ? s->stride : 1);
    }
    if(n>have) vr_fail("pointer argument exceeds its buffer capacity");
}
static size_t vr_uniform_bytes(int type,int count) {
    if(count<0) vr_fail("negative uniform count");
    int components=type==SHADER_UNIFORM_VEC2 || type==SHADER_UNIFORM_IVEC2 || type==SHADER_UNIFORM_UIVEC2 ? 2 :
                   type==SHADER_UNIFORM_VEC3 || type==SHADER_UNIFORM_IVEC3 || type==SHADER_UNIFORM_UIVEC3 ? 3 :
                   type==SHADER_UNIFORM_VEC4 || type==SHADER_UNIFORM_IVEC4 || type==SHADER_UNIFORM_UIVEC4 ? 4 : 1;
    return (size_t)count*components*4;
}

// Definitions and the exhaustive dispatcher are generated from the pinned headers.
#include "generated_types.inc"
#include "custom_ownership.inc"
static void vr_close_graphics(void);
static void vr_close_audio(void);
#include "generated_calls.inc"

static int vr_graphics_type(const char *type) {
    return !strncmp(type,"GL:",3) || !strcmp(type,"Texture") || !strcmp(type,"RenderTexture") ||
           !strcmp(type,"Font") || !strcmp(type,"Mesh") || !strcmp(type,"Shader") ||
           !strcmp(type,"Material") || !strcmp(type,"Model") || !strcmp(type,"rlRenderBatch");
}
static void vr_forget_context_dependencies(int graphics) {
    VRDependency **next=&vr_dependencies;
    while(*next) {
        VRDependency *d=*next;
        VRSlot *root=vr_slot(d->from);
        while(root->owner) root=vr_slot(root->owner);
        int belongs=graphics ? vr_graphics_type(root->type) :
            (!strcmp(root->type,"Sound") || !strcmp(root->type,"Music") || !strcmp(root->type,"AudioStream"));
        if(belongs) { *next=d->next; free(d); } else next=&d->next;
    }
}
static void vr_close_graphics(void) {
    // All graphics owners are closing together, so cross-model borrowing cycles
    // no longer need to prevent deletion. Unrelated audio dependencies remain.
    vr_forget_context_dependencies(1);
    int *default_locations=rlGetShaderLocsDefault();
    for(size_t i=0;i<vr_capacity;++i)
        if(vr_slots[i].data==default_locations && default_locations && !vr_slots[i].drop && !vr_slots[i].allocated) vr_invalidate(vr_id(i));
    for(size_t i=0;i<vr_capacity;++i)
        if(vr_slots[i].data && !vr_slots[i].owner && (!strcmp(vr_slots[i].type,"Model") || !strcmp(vr_slots[i].type,"Material") || !strcmp(vr_slots[i].type,"GL:Framebuffer"))) vr_release(vr_id(i),NULL);
    for(size_t i=vr_capacity;i>0;--i)
        if(vr_slots[i-1].data && !vr_slots[i-1].owner && vr_graphics_type(vr_slots[i-1].type) && (vr_slots[i-1].drop || vr_slots[i-1].allocated)) vr_release(vr_id(i-1),NULL);
    if(IsWindowReady()) CloseWindow();
}
static void vr_close_audio(void) {
    vr_forget_context_dependencies(0);
    for(size_t i=vr_capacity;i>0;--i) {
        VRSlot *s=&vr_slots[i-1];
        if(s->data && !s->owner && (!strcmp(s->type,"Sound") || !strcmp(s->type,"Music") || !strcmp(s->type,"AudioStream"))) vr_release(vr_id(i-1),NULL);
    }
    if(IsAudioDeviceReady()) CloseAudioDevice();
}

void vyne_ray_dispose(VR v) {
    if (v.kind == VR_TEXT) free((void *)v.text);
    if (v.kind == VR_LIST || v.kind == VR_RECORD) {
        for (size_t i = 0; i < v.count; ++i) vyne_ray_dispose(v.items[i]);
        free(v.items);
    }
}

static VR vr_helper(int id, const VR *a, size_t count) {
    static const unsigned char arities[]={2,2,3,1,3,3,0,0,3,2,1,1,1,1};
    static const char *names[]={"buffer","buffer_read","buffer_write","release","field","set_field","live_handles","shutdown","model_texture","model_shader","render_texture","buffer_size","valid","environment"};
    if(id<=-1 && id>=-14) vr_function=names[-id-1];
    if(id>-1 || id<-14 || count!=arities[-id-1]) vr_fail("incorrect helper argument count");
    switch (id) {
    case VR_BUFFER: {
        // Store a canonical static type label, never a caller's arena string.
        const char *type = vr_canonical_type(vr_required_string(a[0])); size_t stride = vr_type_size(type);
        int64_t n = vr_integer(a[1]);
        if (n < 0 || (uint64_t)n > SIZE_MAX / stride) vr_fail("invalid buffer size");
        return vr_handle(vr_alloc(stride * (size_t)n), type, (size_t)n, stride, NULL, 1, 0);
    }
    case VR_READ: case VR_WRITE: {
        VRSlot *s = vr_slot(vr_integer(a[0])); int64_t i = vr_integer(a[1]);
        if (i < 0 || (uint64_t)i >= s->count) vr_fail("buffer index is out of bounds");
        void *p = (char *)s->data + (size_t)i * s->stride;
        if (id == VR_READ) return vr_read_type(s->type, p, vr_integer(a[0]));
        if (strchr(s->type,'*') && a[2].kind==VR_TEXT) vr_fail("persistent pointer buffers require character buffer handles, not temporary strings");
        // Resource structs cannot be shallow-copied into buffers: that duplicates ownership.
        if (vr_resource_type(s->type)) vr_fail("resource buffers are read-only");
        vr_write_type(s->type, p, a[2]); return vr_null();
    }
    case VR_RELEASE: vr_release(vr_integer(a[0]), NULL); return vr_null();
    case VR_FIELD: return vr_get_field(a[0], vr_required_string(a[1]), vr_integer(a[2]));
    case VR_SET_FIELD: vr_set_field(a[0], vr_required_string(a[1]), a[2]); return vr_null();
    case VR_LIVE: {
        size_t n = 0; for (size_t i = 0; i < vr_capacity; ++i) if (vr_slots[i].data) ++n;
        return vr_int((int64_t)n);
    }
    case VR_BUFFER_SIZE: return vr_int((int64_t)vr_slot(vr_integer(a[0]))->count);
    case VR_VALID: {
        int64_t h = vr_integer(a[0]); uint32_t low = (uint32_t)h;
        return vr_bool(low && low <= vr_capacity && vr_slots[low - 1].data && vr_slots[low - 1].generation == (uint32_t)((uint64_t)h >> 32));
    }
    case VR_ENVIRONMENT: return vr_text(getenv(vr_required_string(a[0])));
    case VR_MODEL_TEXTURE: {
        Model *m = vr_checked(a[0], "Model", 1); Texture *t = vr_checked(a[1], "Texture", 1);
        int i = vr_i32(a[2]);
        if (!m || !t || i < 0 || i >= m->materialCount) vr_fail("invalid material or texture");
        SetMaterialTexture(&m->materials[i], MATERIAL_MAP_DIFFUSE, *t);
        vr_depend(vr_integer(a[0]),vr_integer(a[1]),i*2);
        return vr_null();
    }
    case VR_MODEL_SHADER: {
        Model *m = vr_checked(a[0], "Model", 1); Shader *s = vr_checked(a[1], "Shader", 1);
        if (!m || !s) vr_fail("invalid model or shader");
        for (int i = 0; i < m->materialCount; ++i) { m->materials[i].shader = *s; vr_depend(vr_integer(a[0]),vr_integer(a[1]),i*2+1); }
        return vr_null();
    }
    case VR_RENDER_TEXTURE: {
        RenderTexture *r = vr_checked(a[0], "RenderTexture", 1);
        if (!r) vr_fail("invalid render texture");
        return vr_handle(&r->texture, "Texture", 1, sizeof(Texture), NULL, 0, vr_integer(a[0]));
    }
    case VR_SHUTDOWN:
        vr_close_graphics();
        vr_close_audio();
        for (size_t i = vr_capacity; i > 0; --i)
            if (vr_slots[i-1].data && !vr_slots[i-1].owner && (vr_slots[i-1].drop || vr_slots[i-1].allocated)) vr_release(vr_id(i-1), NULL);
        for (size_t i = 0; i < vr_capacity; ++i) vr_slots[i].data = NULL;
        return vr_null();
    }
    vr_fail("unknown helper"); return vr_null();
}
VyneRayResult vyne_ray_call(int id, const VR *args, size_t count) {
    vr_function = "native"; vr_error[0] = 0;
    if (setjmp(vr_jump)) { vr_clear_temps(); return (VyneRayResult){vr_null(), vr_error}; }
    VR v = id < 0 ? vr_helper(id, args, count) : vr_dispatch(id, args, count);
    vr_clear_temps(); return (VyneRayResult){v, NULL};
}
