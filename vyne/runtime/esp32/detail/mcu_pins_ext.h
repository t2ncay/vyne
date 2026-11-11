// runtime/esp32/detail/mcu_pins_ext.h
#pragma once
#include "mcu_types.h"
#include <soc/soc_caps.h>

// ---------------------------------------------------------------------
// Extended analog I/O.
//
// The ESP32 ADC is 12-bit (0..4095) by default and its "reference" is
// really an input attenuation. The setter API keeps the AVR names but
// shifts the meaning, which is documented on each function.
// ---------------------------------------------------------------------

static inline VyneValue arduino_analog_read_voltage(VyneValue pin) {
    // Assumes the default 11 dB attenuation (about 0..3.3 V) and the
    // default 12-bit ADC. If the sketch changed either, scale here.
    return vyne_float(analogRead((uint8_t)pin.as.i64) * (3.3 / 4095.0));
}

static inline VyneValue arduino_analog_reference(VyneValue mode) {
    // AVR's "reference voltage" concept maps to ESP32 input attenuation:
    //   0 = ADC_ATTEN_DB_11  -> roughly 0..3.3 V  (default)
    //   1 = ADC_ATTEN_DB_2_5 -> roughly 0..1.5 V
    //   2 = ADC_ATTEN_DB_0   -> roughly 0..1.1 V
    // Values outside 0..2 fall back to the default attenuation.
    adc_attenuation_t atten = ADC_ATTEN_DB_11;
    switch ((int)mode.as.i64) {
    case 0:
        atten = ADC_ATTEN_DB_11;
        break;
    case 1:
        atten = ADC_ATTEN_DB_2_5;
        break;
    case 2:
        atten = ADC_ATTEN_DB_0;
        break;
    default:
        atten = ADC_ATTEN_DB_11;
        break;
    }
    analogSetAttenuation(atten);
    return vyne_null();
}

static inline VyneValue arduino_analog_read_resolution(VyneValue bits) {
    // ESP32 supports 9..12 bits. Clamp rather than silently producing a
    // surprising shift, since the core's analogReadResolution() reconfigures
    // the hardware rather than scaling in software.
    int b = (int)bits.as.i64;
    if (b < 9)
        b = 9;
    if (b > 12)
        b = 12;
    analogReadResolution(b);
    return vyne_null();
}

static inline VyneValue arduino_analog_write_resolution(VyneValue bits) {
    // LEDC supports 1..16 bits. Re-attach every channel already in use so
    // subsequent analog_write() calls take the new range. Values above 16
    // are clamped; values below 1 are clamped to 1.
    int b = (int)bits.as.i64;
    if (b < 1)
        b = 1;
    if (b > 16)
        b = 16;
    g_vyne_pwm_bits = (uint8_t)b;
    _vyne_ledc_reattach_all((uint8_t)b);
    return vyne_null();
}

static inline VyneValue
arduino_analog_read_oversampled(VyneValue pin, VyneValue n) {
    // Averaging n samples improves SNR by ~sqrt(n). ESP32 has a real
    // 64-bit double, so the accumulator does not lose precision for the
    // sample counts that are practical on this MCU.
    int64_t count = n.as.i64;
    if (count <= 0)
        return vyne_float(0.0);
    int64_t sum = 0;
    for (int64_t i = 0; i < count; ++i)
        sum += analogRead((uint8_t)pin.as.i64);
    return vyne_float((double)sum / (double)count);
}

// ---------------------------------------------------------------------
// Extended digital I/O.
// ---------------------------------------------------------------------

static inline VyneValue arduino_pin_toggle(VyneValue pin) {
    int p = (int)pin.as.i64;
    digitalWrite(p, !digitalRead(p));
    return vyne_null();
}

static inline VyneValue
arduino_pulse_in(VyneValue pin, VyneValue state, VyneValue timeout_us) {
    unsigned long us = ::pulseIn(
        (uint8_t)pin.as.i64,
        (uint8_t)state.as.i64,
        (unsigned long)timeout_us.as.i64
    );
    return vyne_int((int64_t)us);
}

static inline VyneValue arduino_shift_out(
    VyneValue data_pin,
    VyneValue clk_pin,
    VyneValue bit_order,
    VyneValue val,
    VyneValue nbits
) {
    // bit_order: 0 = LSBFIRST, non-zero = MSBFIRST.
    // nbits:    1..32 — open-coded because the built-in shiftOut() only
    //           handles 8 bits.
    int dp = (int)data_pin.as.i64;
    int cp = (int)clk_pin.as.i64;
    int64_t v = val.as.i64;
    int n = (int)nbits.as.i64;
    bool msb_first = (bit_order.as.i64 != 0);
    if (n <= 0)
        return vyne_null();

    for (int i = 0; i < n; ++i) {
        int bit =
            msb_first ? (int)((v >> (n - 1 - i)) & 1) : (int)((v >> i) & 1);
        digitalWrite(dp, bit);
        digitalWrite(cp, HIGH);
        digitalWrite(cp, LOW);
    }
    return vyne_null();
}

static inline VyneValue
arduino_pulse_count(VyneValue pin, VyneValue window_ms) {
    // Polling-based edge counter. For signals faster than a few tens of
    // kHz, use attach_interrupt + interrupt_count instead.
    int p = (int)pin.as.i64;
    unsigned long window = (unsigned long)window_ms.as.i64;
    unsigned long start = millis();
    int64_t count = 0;
    int last = digitalRead(p);

    while ((millis() - start) < window) {
        int cur = digitalRead(p);
        if (last == LOW && cur == HIGH)
            ++count;
        last = cur;
    }
    return vyne_int(count);
}

// ---------------------------------------------------------------------
// DAC (classic ESP32 and ESP32-S2 only). GPIO25 and GPIO26 on the
// classic ESP32. 8-bit values, 0..255.
// ---------------------------------------------------------------------
#if defined(SOC_DAC_SUPPORTED) && SOC_DAC_SUPPORTED
static inline VyneValue esp32_dac_write(VyneValue pin, VyneValue value) {
    int v = (int)value.as.i64;
    if (v < 0)
        v = 0;
    if (v > 255)
        v = 255;
    dacWrite((uint8_t)pin.as.i64, (uint8_t)v);
    return vyne_null();
}

static inline VyneValue esp32_dac_write_mv(VyneValue pin, VyneValue mv) {
    // 0..3300 mV maps linearly onto 0..255. Values outside the range are
    // clamped rather than wrapped.
    int64_t millivolts = mv.as.i64;
    if (millivolts < 0)
        millivolts = 0;
    if (millivolts > 3300)
        millivolts = 3300;
    int v = (int)((millivolts * 255) / 3300);
    dacWrite((uint8_t)pin.as.i64, (uint8_t)v);
    return vyne_null();
}
#endif

// ---------------------------------------------------------------------
// Built-in capacitive touch pins. Available on the classic ESP32 and
// ESP32-S3; absent on C3/C6 and on boards where the touch peripheral is
// not bonded out. `touchRead()` returns a raw count that falls when the
// pad is touched; the usable range is roughly 0..100.
// ---------------------------------------------------------------------
#if defined(SOC_TOUCH_SENSOR_SUPPORTED) && SOC_TOUCH_SENSOR_SUPPORTED
static uint16_t g_vyne_touch_threshold[64] = {0};

static inline void _vyne_touch_noop_isr(void) {
    // The Vyne surface is poll-based. The interrupt exists only so a
    // sketch can wake from light sleep; it deliberately does nothing.
}

static inline VyneValue esp32_touch_pin_read(VyneValue pin) {
    return vyne_int((int64_t)touchRead((uint8_t)pin.as.i64));
}

static inline VyneValue
esp32_touch_pin_threshold(VyneValue pin, VyneValue threshold) {
    int p = (int)pin.as.i64;
    if (p >= 0 && p < 64)
        g_vyne_touch_threshold[p] = (uint16_t)threshold.as.i64;
    // A threshold change only takes effect on the next attach. Call
    // touch_pin_attach() again to re-arm with the new value.
    return vyne_null();
}

static inline VyneValue
esp32_touch_pin_attach(VyneValue pin, VyneValue threshold) {
    int p = (int)pin.as.i64;
    uint16_t t = (uint16_t)threshold.as.i64;
    if (p >= 0 && p < 64)
        g_vyne_touch_threshold[p] = t;
    touchAttachInterrupt((uint8_t)p, _vyne_touch_noop_isr, t);
    return vyne_null();
}

static inline VyneValue esp32_touch_pin_detach(VyneValue pin) {
    touchDetachInterrupt((uint8_t)pin.as.i64);
    return vyne_null();
}
#endif
