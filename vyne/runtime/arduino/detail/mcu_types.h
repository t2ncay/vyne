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
