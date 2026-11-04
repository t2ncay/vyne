// runtime/arduino/detail/mcu_hardware.h
#pragma once
#include "mcu_types.h"

// These names match the `native_maps.h` additions (Step 5)
static inline VyneValue arduino_pin_mode(VyneValue pin, VyneValue mode) {
    pinMode(pin.as.i64, mode.as.i64);
    return vyne_null();
}

static inline VyneValue arduino_digital_write(VyneValue pin, VyneValue val) {
    digitalWrite(pin.as.i64, val.as.i64);
    return vyne_null();
}

static inline VyneValue arduino_digital_read(VyneValue pin) {
    return vyne_int(digitalRead(pin.as.i64));
}

static inline VyneValue arduino_analog_read(VyneValue pin) {
    return vyne_int(analogRead(pin.as.i64));
}

static inline VyneValue arduino_analog_write(VyneValue pin, VyneValue val) {
    analogWrite(pin.as.i64, val.as.i64);
    return vyne_null();
}

static inline VyneValue arduino_delay(VyneValue ms) {
    delay(ms.as.i64);
    return vyne_null();
}

static inline VyneValue arduino_millis(void) {
    return vyne_int(millis());
}

static inline VyneValue arduino_serial_begin(VyneValue baud) {
    ::Serial.begin((unsigned long)baud.as.i64);
    return vyne_null();
}

static inline VyneValue arduino_serial_print(VyneValue v) {
    _vyne_print_internal(v);
    return vyne_null();
}

// ---------------------------------------------------------------------
// Timing, math, and delay. On AVR `double` is 32-bit, but the emitted
// Float64 semantics are preserved — every function below has the same
// source-level signature the desktop modules expose.
// ---------------------------------------------------------------------
static inline VyneValue arduino_micros(void) {
    return vyne_int((int64_t)micros());
}

static inline VyneValue arduino_sin(VyneValue x) {
    return vyne_float(sin(x.as.f64));
}

static inline VyneValue arduino_cos(VyneValue x) {
    return vyne_float(cos(x.as.f64));
}

static inline VyneValue arduino_sqrt(VyneValue x) {
    return vyne_float(sqrt(x.as.f64));
}

static inline VyneValue arduino_delay_microseconds(VyneValue us) {
    delayMicroseconds((unsigned int)us.as.i64);
    return vyne_null();
}

// ---------------------------------------------------------------------
// Arena introspection. Reads the MCU arena's current offset so a
// sketch can print its own footprint. The `drift = after - before`
// around a region body is the load-bearing demo line.
// ---------------------------------------------------------------------
static inline VyneValue arduino_arena_used(void) {
    return vyne_int((int64_t)arena_used());
}

// ---------------------------------------------------------------------
// Region checkpoint shim. `RegionNode::compile` (in codegen/regions.cpp)
// emits `vmem_runtime_checkpoint()` and `vmem_runtime_rewind(...)` on
// every target. On the Arduino target, `vmem.h` is intentionally not
// included — the MCU runtime aliases the pair onto the single MCU arena
// instead. The handle is a VyneValue carrying the byte offset as
// `.as.i64`, matching the desktop signature the emitter expects.
// ---------------------------------------------------------------------
static inline VyneValue vmem_runtime_checkpoint(void) {
    ArenaCheckpoint cp = arena_checkpoint();
    VyneValue v;
    v.type = V_INT64;
    v.as.i64 = (int64_t)cp.offset;
    return v;
}

static inline void vmem_runtime_rewind(VyneValue cp) {
    ArenaCheckpoint c;
    c.offset = (size_t)cp.as.i64;
    arena_rewind(c);
}

// ---------------------------------------------------------------------
// Bounds-check panic. `codegen/collections.cpp` and
// `codegen/maps_strings_scratch.cpp` emit `fprintf(stderr, ...) +
// exit(1)` on desktop. Neither is usable on AVR. `vyne_panic_bounds`
// is the MCU replacement — print the diagnostic, then halt. The
// `while (1) {}` is the standard AVR firmware-halt idiom.
// ---------------------------------------------------------------------
static inline void
vyne_panic_bounds(const char* dimLabel, int shapeBound, int line, int64_t got) {
    ::Serial.print("VNE-072 line ");
    ::Serial.print(line);
    ::Serial.print(": scratch index out of bounds: ");
    ::Serial.print(dimLabel);
    ::Serial.print(" of shape [");
    ::Serial.print(shapeBound);
    ::Serial.print("] got ");
    ::Serial.print((long)got);
    ::Serial.println();
    while (1) {
    }
}
