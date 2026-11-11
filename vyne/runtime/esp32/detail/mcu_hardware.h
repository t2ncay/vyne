// runtime/esp32/detail/mcu_hardware.h
#pragma once
#include "mcu_types.h"

// ---------------------------------------------------------------------
// PWM / LEDC shim.
//
// The ESP32 has no `analogWrite`; LEDC is the hardware PWM peripheral.
// Arduino-ESP32 3.x exposes a pin-oriented API:
//
//     ledcAttach(pin, freq_hz, resolution_bits);
//     ledcWrite(pin, duty);
//
// Arduino-ESP32 2.x used an explicit channel:
//
//     ledcSetup(channel, freq_hz, resolution_bits);
//     ledcAttachPin(pin, channel);
//     ledcWrite(channel, duty);
//
// Both are supported below. The small pin table makes re-attaching a
// pin idempotent so `analog_write_resolution()` can reconfigure every
// channel the sketch has already used.
// ---------------------------------------------------------------------
#define VYNE_LEDC_MAX_PINS 16
static int g_vyne_ledc_pin[VYNE_LEDC_MAX_PINS];
static int g_vyne_ledc_channel[VYNE_LEDC_MAX_PINS];
static int g_vyne_ledc_count = 0;
static uint32_t g_vyne_ledc_freq = 5000; // 5 kHz, matching the AVR default

static inline int _vyne_ledc_slot(int pin) {
    for (int i = 0; i < g_vyne_ledc_count; ++i)
        if (g_vyne_ledc_pin[i] == pin)
            return i;
    if (g_vyne_ledc_count >= VYNE_LEDC_MAX_PINS)
        return -1;
    int slot = g_vyne_ledc_count++;
    g_vyne_ledc_pin[slot] = pin;
    g_vyne_ledc_channel[slot] = slot; // only used on the 2.x API
    return slot;
}

static inline void _vyne_ledc_attach(int pin, uint8_t bits) {
    if (bits < 1)
        bits = 1;
    if (bits > 16)
        bits = 16;
    int slot = _vyne_ledc_slot(pin);
    if (slot < 0)
        return;

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    // 3.x is pin-addressed. ledcAttach() reconfigures an existing pin.
    ledcAttach(pin, g_vyne_ledc_freq, bits);
    ledcWrite(pin, 0);
#else
    int ch = g_vyne_ledc_channel[slot];
    ledcSetup(ch, g_vyne_ledc_freq, bits);
    ledcAttachPin(pin, ch);
    ledcWrite(ch, 0);
#endif
}

static inline void _vyne_ledc_write(int pin, int64_t value, uint8_t bits) {
    if (bits < 1)
        bits = 1;
    if (bits > 16)
        bits = 16;
    int64_t max = ((int64_t)1 << bits) - 1;
    if (value < 0)
        value = 0;
    if (value > max)
        value = max;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(pin, (uint32_t)value);
#else
    int slot = _vyne_ledc_slot(pin);
    if (slot >= 0)
        ledcWrite(g_vyne_ledc_channel[slot], (uint32_t)value);
#endif
}

// Re-program every pin the sketch has ever PWM'd at the new bit width.
// Used by arduino.analog_write_resolution(). Duty values are not rescaled
// here; the next analog_write() call supplies a value in the new range.
static inline void _vyne_ledc_reattach_all(uint8_t bits) {
    for (int i = 0; i < g_vyne_ledc_count; ++i)
        _vyne_ledc_attach(g_vyne_ledc_pin[i], bits);
}

// ---------------------------------------------------------------------
// Base digital / analog I/O. Same Vyne-level signature as the AVR
// runtime; the ESP32 ADC is 12-bit by default and the LEDC shim above
// replaces analogWrite.
// ---------------------------------------------------------------------
static inline VyneValue arduino_pin_mode(VyneValue pin, VyneValue mode) {
    pinMode((uint8_t)pin.as.i64, (uint8_t)mode.as.i64);
    return vyne_null();
}

static inline VyneValue arduino_digital_write(VyneValue pin, VyneValue val) {
    digitalWrite((uint8_t)pin.as.i64, (uint8_t)val.as.i64);
    return vyne_null();
}

static inline VyneValue arduino_digital_read(VyneValue pin) {
    return vyne_int(digitalRead((uint8_t)pin.as.i64));
}

static inline VyneValue arduino_analog_read(VyneValue pin) {
    // Returns 0..4095 after the default 12-bit configuration. If the
    // sketch called analog_read_resolution(bits), the ESP32 core has
    // already reprogrammed the ADC, so this is a straight passthrough.
    return vyne_int(analogRead((uint8_t)pin.as.i64));
}

static inline VyneValue arduino_analog_write(VyneValue pin, VyneValue val) {
    uint8_t bits = g_vyne_pwm_bits;
    if (bits == 0)
        bits = 8;
    _vyne_ledc_attach((int)pin.as.i64, bits);
    _vyne_ledc_write((int)pin.as.i64, val.as.i64, bits);
    return vyne_null();
}

static inline VyneValue arduino_delay(VyneValue ms) {
    delay((uint32_t)ms.as.i64);
    return vyne_null();
}

static inline VyneValue arduino_millis(void) {
    return vyne_int((int64_t)millis());
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
// Timing and math. ESP32 has a real 64-bit double, so these use the
// same libm entry points as the desktop runtime.
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
    delayMicroseconds((uint32_t)us.as.i64);
    return vyne_null();
}

// ---------------------------------------------------------------------
// Arena introspection.
// ---------------------------------------------------------------------
static inline VyneValue arduino_arena_used(void) {
    return vyne_int((int64_t)arena_used());
}

// ---------------------------------------------------------------------
// Region checkpoint shim. Same contract as the AVR runtime: the handle
// is the arena offset carried in a VyneValue.
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
// Bounds-check panic. Prints the diagnostic over USB serial, then halts.
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
