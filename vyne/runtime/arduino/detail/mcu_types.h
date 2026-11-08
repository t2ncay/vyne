#pragma once
#include "mcu_arena.h"
#include <Arduino.h>

// Runtime analog I/O state. Written by arduino.analog_read_resolution() and
// arduino.analog_write_resolution(); read by
// arduino.analog_read()/analog_write() in mcu_hardware.h. Defaults preserve
// classic 10-bit ADC and 8-bit PWM, so existing sketches emit identical code.
//
// These are process-global and intentionally survive a region rewind —
// resolution is configuration, not scratch state. Setting them from inside a
// region does not roll back on exit.
static uint8_t g_vyne_adc_shift = 0; // 0 = 10-bit, 2 = 8-bit, etc.
static uint8_t g_vyne_pwm_bits = 8;  // 8 = raw 0..255, else software-scaled

// Branch-prediction hint used by the emitted scratch bounds checks.
// `__builtin_expect` compiles to a no-op tag on AVR but keeps the
// emitter's output identical to the desktop target — one code path
// for both.
#ifndef VYNE_UNLIKELY
#define VYNE_UNLIKELY(x) __builtin_expect(!!(x), 0)
#endif

typedef enum : uint8_t {
    V_NULL = 0,
    V_FLOAT64 = 1,
    V_INT64 = 2,
    V_STRING = 3,
    V_ARRAY = 4,
    V_BOOL = 5,
    V_STRUCT = 6,
    V_MAP = 10,
    V_F64_ARRAY = 11,
    V_I64_ARRAY = 12
} VyneType;

typedef struct VyneValue VyneValue;
struct VyneValue {
    VyneType type;
    uint32_t _reserved;
    union {
        double f64;  // Note: On AVR, double is 32-bit.
        int64_t i64; // Note: 64-bit math on AVR is SLOW.
        char* str;
        void* ptr;
        struct VyneArray* arr;
        struct VyneStruct* strct;
    } as;
};

// Forward declarations for the value constructors. `vyne_array_get`
// and `vyne_array_push` below return / wrap boxed VyneValues, so
// they need `vyne_null`, `vyne_int`, and `vyne_float` in scope.
// Those constructors are defined later in this header (after the
// array block). Forward-declaring here keeps the array block
// self-contained and readable top-to-bottom without moving large
// chunks of the file around.
static inline VyneValue vyne_null(void);
static inline VyneValue vyne_int(int64_t i);
static inline VyneValue vyne_float(double f);

// ============================================================
// Typed-array containers (AVR)
// ------------------------------------------------------------
// The compiler emits calls into this surface whenever Vyne source

// ============================================================
// Typed-array containers (AVR)
// ------------------------------------------------------------
// The compiler emits calls into this surface whenever Vyne source
// uses `Array<Float64>` or `Array<Int64>`. On desktop those calls
// resolve against runtime/detail/arrays.h; on MCU they resolve
// here. The surface is a strict subset — only the operations that
// the emitter actually produces for typed arrays are defined.
//
// Layout / boxing note:
//   VyneArray_f64 and VyneArray_i64 share the *field ordering* of
//   the untyped VyneArray (`<ptr>, int64_t size, int64_t cap`).
//   Boxing is a pointer pun — `v.as.arr = (VyneArray*)&container`.
//   Any code that reads `v.as.arr->size` from a V_F64_ARRAY or
//   V_I64_ARRAY value therefore reads the correct integer. Only
//   `.elements` differs between the three kinds, and every read
//   of `.elements` is guarded by a `.type == V_ARRAY` check at
//   the call site (see vyne_array_get / vyne_array_push below).
//
// Growth:
//   `_push` grows by allocating a fresh buffer from the arena and
//   copying. The old buffer is not freed — the arena has no
//   realloc; the enclosing `region` block's rewind is what
//   reclaims it. Wrap array-building code in `region { ... }` to
//   bound peak SRAM across repeated pushes.
// ============================================================

typedef struct VyneArray {
    VyneValue* elements;
    int64_t size;
    int64_t cap;
} VyneArray;

typedef struct {
    double* data;
    int64_t size;
    int64_t cap;
} VyneArray_f64;

typedef struct {
    int64_t* data;
    int64_t size;
    int64_t cap;
} VyneArray_i64;

// --- Typed constructors -------------------------------------
// Allocate `n` elements from the arena and return a stack struct.
// Only the data buffer is arena-backed; the 24-byte container
// struct itself lives wherever the caller assigns it. n<=0 returns
// an empty container with data=NULL, matching the desktop
// vyne_array_f64_create(0) semantics.

static inline VyneArray_f64 vyne_array_f64_create(int64_t n) {
    VyneArray_f64 arr;
    arr.size = n;
    arr.cap = n;
    arr.data =
        (n > 0) ? (double*)arena_alloc((size_t)(n * sizeof(double))) : NULL;
    return arr;
}

static inline VyneArray_i64 vyne_array_i64_create(int64_t n) {
    VyneArray_i64 arr;
    arr.size = n;
    arr.cap = n;
    arr.data =
        (n > 0) ? (int64_t*)arena_alloc((size_t)(n * sizeof(int64_t))) : NULL;
    return arr;
}

// --- Typed push ---------------------------------------------
// Grows by 1.5x above 8 elements. The old buffer stays live in the
// arena until the enclosing region rewinds. Push outside a region
// will exhaust the 1 KB arena and halt the board — a deliberate
// choice: array lifetime is exactly what region scope is for.

static inline void vyne_array_f64_push(VyneArray_f64* arr, double v) {
    if (arr->size >= arr->cap) {
        int64_t newCap = arr->cap < 8 ? 8 : (arr->cap + arr->cap / 2);
        double* newData =
            (double*)arena_alloc((size_t)(newCap * sizeof(double)));
        for (int64_t i = 0; i < arr->size; ++i)
            newData[i] = arr->data[i];
        arr->data = newData;
        arr->cap = newCap;
    }
    arr->data[arr->size++] = v;
}

static inline void vyne_array_i64_push(VyneArray_i64* arr, int64_t v) {
    if (arr->size >= arr->cap) {
        int64_t newCap = arr->cap < 8 ? 8 : (arr->cap + arr->cap / 2);
        int64_t* newData =
            (int64_t*)arena_alloc((size_t)(newCap * sizeof(int64_t)));
        for (int64_t i = 0; i < arr->size; ++i)
            newData[i] = arr->data[i];
        arr->data = newData;
        arr->cap = newCap;
    }
    arr->data[arr->size++] = v;
}

// --- Box / unbox --------------------------------------------
// Boxing puns the pointer; nothing is allocated or copied. The
// caller must keep the source container's storage alive for as
// long as the boxed VyneValue is used (matching the emitter's
// pattern of `boxAny` on variables).

static inline VyneValue vyne_array_f64_to_value(VyneArray_f64* arr) {
    VyneValue v;
    v.type = V_F64_ARRAY;
    v.as.arr = (VyneArray*)arr;
    return v;
}

static inline VyneValue vyne_array_i64_to_value(VyneArray_i64* arr) {
    VyneValue v;
    v.type = V_I64_ARRAY;
    v.as.arr = (VyneArray*)arr;
    return v;
}

static inline VyneArray_f64 vyne_value_to_array_f64(VyneValue v) {
    if (v.type == V_F64_ARRAY)
        return *(VyneArray_f64*)v.as.arr;
    VyneArray_f64 empty;
    empty.data = NULL;
    empty.size = 0;
    empty.cap = 0;
    return empty;
}

static inline VyneArray_i64 vyne_value_to_array_i64(VyneValue v) {
    if (v.type == V_I64_ARRAY)
        return *(VyneArray_i64*)v.as.arr;
    VyneArray_i64 empty;
    empty.data = NULL;
    empty.size = 0;
    empty.cap = 0;
    return empty;
}

// --- Slice --------------------------------------------------
// hi is inclusive, matching the desktop SliceNode semantics and
// the emitter's `vyne_array_f64_slice(&src, lo, hi)` convention.
// Returns a fresh container; the source is unchanged.

static inline VyneArray_f64
vyne_array_f64_slice(VyneArray_f64* src, int64_t lo, int64_t hi) {
    if (lo < 0)
        lo = 0;
    if (hi >= src->size)
        hi = src->size - 1;
    int64_t n = (hi >= lo) ? (hi - lo + 1) : 0;
    VyneArray_f64 out = vyne_array_f64_create(n);
    for (int64_t i = 0; i < n; ++i)
        out.data[i] = src->data[lo + i];
    return out;
}

static inline VyneArray_i64
vyne_array_i64_slice(VyneArray_i64* src, int64_t lo, int64_t hi) {
    if (lo < 0)
        lo = 0;
    if (hi >= src->size)
        hi = src->size - 1;
    int64_t n = (hi >= lo) ? (hi - lo + 1) : 0;
    VyneArray_i64 out = vyne_array_i64_create(n);
    for (int64_t i = 0; i < n; ++i)
        out.data[i] = src->data[lo + i];
    return out;
}

// --- Untyped fallback ---------------------------------------
// Required by the emitter's `x := [];` boxed path and by mixed-
// type literals. Untyped arrays cost 16 bytes per element (one
// VyneValue per slot) on AVR vs. 4 / 8 for typed containers. The
// untyped VyneArray struct itself is arena-allocated so that push
// can grow it without the caller holding a stable address.

static inline VyneValue vyne_array_create(int64_t n) {
    VyneArray* arr = (VyneArray*)arena_alloc(sizeof(VyneArray));
    arr->size = 0;
    arr->cap = n;
    arr->elements =
        (n > 0) ? (VyneValue*)arena_alloc((size_t)(n * sizeof(VyneValue)))
                : NULL;
    VyneValue v;
    v.type = V_ARRAY;
    v.as.arr = arr;
    return v;
}

static inline void vyne_array_set(VyneValue arr, VyneValue idx, VyneValue val) {
    if (arr.type != V_ARRAY)
        return;
    if (idx.as.i64 < 0 || idx.as.i64 >= arr.as.arr->cap)
        return;
    arr.as.arr->elements[idx.as.i64] = val;
    if (idx.as.i64 >= arr.as.arr->size)
        arr.as.arr->size = idx.as.i64 + 1;
}

// Appends a value to any container kind. The emitter's boxed-push
// fallback can box a typed array as well as an untyped one, so
// this must dispatch on `.type`. Growth on the untyped branch
// rewrites `elements`/`cap` on the arena-allocated VyneArray
// struct, which is why `vyne_array_create` allocates it there
// rather than on the stack.
static inline void vyne_array_push(VyneValue v, VyneValue val) {
    if (v.type == V_ARRAY) {
        VyneArray* arr = v.as.arr;
        if (arr->size >= arr->cap) {
            int64_t newCap = arr->cap < 8 ? 8 : (arr->cap + arr->cap / 2);
            VyneValue* newEls =
                (VyneValue*)arena_alloc((size_t)(newCap * sizeof(VyneValue)));
            for (int64_t i = 0; i < arr->size; ++i)
                newEls[i] = arr->elements[i];
            arr->elements = newEls;
            arr->cap = newCap;
        }
        arr->elements[arr->size++] = val;
    } else if (v.type == V_F64_ARRAY) {
        VyneArray_f64* arr = (VyneArray_f64*)v.as.arr;
        double dv = (val.type == V_FLOAT64) ? val.as.f64 : (double)val.as.i64;
        vyne_array_f64_push(arr, dv);
    } else if (v.type == V_I64_ARRAY) {
        VyneArray_i64* arr = (VyneArray_i64*)v.as.arr;
        int64_t iv = (val.type == V_INT64) ? val.as.i64 : (int64_t)val.as.f64;
        vyne_array_i64_push(arr, iv);
    }
}

// Element read on any container kind. Returns a boxed VyneValue
// for untyped arrays (already boxed) and boxes the scalar for
// typed arrays. Returns null for out-of-range index or unknown
// container kind, matching the desktop runtime's behaviour.
static inline VyneValue vyne_array_get(VyneValue v, VyneValue idx) {
    int64_t i = idx.as.i64;
    if (v.type == V_ARRAY) {
        if (i < 0 || i >= v.as.arr->size)
            return vyne_null();
        return v.as.arr->elements[i];
    } else if (v.type == V_F64_ARRAY) {
        VyneArray_f64* arr = (VyneArray_f64*)v.as.arr;
        if (i < 0 || i >= arr->size)
            return vyne_null();
        return vyne_float(arr->data[i]);
    } else if (v.type == V_I64_ARRAY) {
        VyneArray_i64* arr = (VyneArray_i64*)v.as.arr;
        if (i < 0 || i >= arr->size)
            return vyne_null();
        return vyne_int(arr->data[i]);
    }
    return vyne_null();
}

// Size helper for the boxed fallback iteration paths emitted by
// ForNode::getCExpr. Handles all three container kinds so a
// `through` over a typed array works without knowing its
// representation at compile time.
static inline int64_t _vyne_array_size(VyneValue v) {
    if (v.type == V_ARRAY)
        return v.as.arr->size;
    if (v.type == V_F64_ARRAY)
        return ((VyneArray_f64*)v.as.arr)->size;
    if (v.type == V_I64_ARRAY)
        return ((VyneArray_i64*)v.as.arr)->size;
    return 0;
}

// Keep constructors identical to desktop
static inline VyneValue vyne_null(void) {
    VyneValue v;
    v.type = V_NULL;
    v.as.i64 = 0;
    return v;
}
static inline VyneValue vyne_int(int64_t i) {
    VyneValue v;
    v.type = V_INT64;
    v.as.i64 = i;
    return v;
}
static inline VyneValue vyne_float(double f) {
    VyneValue v;
    v.type = V_FLOAT64;
    v.as.f64 = f;
    return v;
}
static inline VyneValue vyne_bool(bool b) {
    VyneValue v;
    v.type = V_BOOL;
    v.as.i64 = b ? 1 : 0;
    return v;
}

static inline VyneValue vyne_string(const char* s) {
    VyneValue v;
    v.type = V_STRING;
    size_t len = strlen(s) + 1;
    char* copy = (char*)arena_alloc(len);
    memcpy(copy, s, len);
    v.as.str = copy;
    return v;
}

// Literals are stored in .rodata (flash). We just point to them
// without copying to the arena, exactly like the desktop runtime.
static inline VyneValue vyne_string_static(const char* s) {
    VyneValue v;
    v.type = V_STRING;
    v.as.str = (char*)s;
    return v;
}

static inline void _vyne_print_internal(VyneValue v) {
    // Qualified lookup only. On several AVR cores, `Serial.print`
    // participates in unqualified lookup together with `std::print`
    // (C++23) and the `Print` base class's overload set, and the
    // argument kinds below (`long`, `double`, `char*`, `const char*`,
    // `const char[5]`) are all candidates for at least one ambiguous
    // overload. `::Serial` forces the Arduino global object.
    if (v.type == V_INT64)
        ::Serial.print((long)v.as.i64);
    else if (v.type == V_FLOAT64)
        ::Serial.print((double)v.as.f64);
    else if (v.type == V_STRING)
        ::Serial.print(v.as.str);
    else if (v.type == V_BOOL)
        ::Serial.print(v.as.i64 ? "true" : "false");
    else if (v.type == V_NULL)
        ::Serial.print("null");
}

static inline void vyne_out(VyneValue v) {
    _vyne_print_internal(v);
    Serial.println();
}

// Module deployment is a desktop-only introspection hook. External
// libraries ship `deploy <module>;` in their facades; on AVR the call
// lowers to this no-op so the same library source compiles on both
// targets without a conditional.
static inline void vyne_deploy_module(const char* name) {
    (void)name;
}
