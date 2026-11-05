// runtime/arduino/detail/mcu_pins_ext.h
#pragma once
#include "mcu_types.h"

// ---------------------------------------------------------------------
// Extended analog I/O. Items 1-5 of the request.
// The setters here write g_vyne_adc_shift / g_vyne_pwm_bits (defined in
// mcu_types.h), which the base analog_read/analog_write in
// mcu_hardware.h respect. That keeps the getter API single-entry-point.
// ---------------------------------------------------------------------

static inline VyneValue arduino_analog_read_voltage(VyneValue pin) {
    // 5.0 V / 1023 counts, matching the Uno's factory ADC reference.
    // If the sketch changed reference via arduino.analog_reference(),
    // the caller is responsible for using the right scale; we don't
    // try to model the reference in software.
    return vyne_float(analogRead(pin.as.i64) * (5.0 / 1023.0));
}

static inline VyneValue arduino_analog_reference(VyneValue mode) {
    // 0 = DEFAULT (5 V), 1 = INTERNAL (1.1 V on Uno), 2 = EXTERNAL.
    switch ((int)mode.as.i64) {
    case 0:
        analogReference(DEFAULT);
        break;
    case 1:
        analogReference(INTERNAL);
        break; // Uno-specific 1.1 V
    case 2:
        analogReference(EXTERNAL);
        break;
    default:
        analogReference(DEFAULT);
        break;
    }
    return vyne_null();
}

static inline VyneValue arduino_analog_read_resolution(VyneValue bits) {
    int b = (int)bits.as.i64;
    if (b >= 10)
        g_vyne_adc_shift = 0;
    else if (b <= 1)
        g_vyne_adc_shift = 9;
    else
        g_vyne_adc_shift = (uint8_t)(10 - b);
    return vyne_null();
}

static inline VyneValue arduino_analog_write_resolution(VyneValue bits) {
    // AVR PWM is physically 8-bit. Values below 8 are software-scaled in
    // arduino_analog_write(); values above 8 are clamped with a warning
    // via Serial because silently rounding would hide a real bug.
    int b = (int)bits.as.i64;
    if (b > 8) {
        ::Serial.println(F("VNE-090: analog_write_resolution >8 clamped to 8"));
        b = 8;
    }
    if (b < 1)
        b = 1;
    g_vyne_pwm_bits = (uint8_t)b;
    return vyne_null();
}

static inline VyneValue
arduino_analog_read_oversampled(VyneValue pin, VyneValue n) {
    // Averaging n samples improves SNR by ~sqrt(n), recovering ~1.5 bits
    // of effective resolution for n=8 and ~2.5 for n=64 on the Uno's ADC.
    // Note: AVR double is 32-bit, so beyond n ≈ 4096 the accumululator
    // saturates its mantissa and further averaging stops helping.
    int64_t count = n.as.i64;
    if (count <= 0)
        return vyne_float(0.0);
    int64_t sum = 0;
    for (int64_t i = 0; i < count; ++i)
        sum += analogRead(pin.as.i64);
    return vyne_float((double)sum / (double)count);
}

// ---------------------------------------------------------------------
// Extended digital I/O. Items 6-9 of the request.
// ---------------------------------------------------------------------

static inline VyneValue arduino_pin_toggle(VyneValue pin) {
    int p = (int)pin.as.i64;
    digitalWrite(p, !digitalRead(p));
    return vyne_null();
}

static inline VyneValue
arduino_pulse_in(VyneValue pin, VyneValue state, VyneValue timeout_us) {
    // state: HIGH (1) or LOW (0). Returns pulse width in microseconds,
    // or 0 on timeout. Uses pulseIn's built-in behaviour.
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
    // nbits:    1..32 — the built-in shiftOut() only handles 8,
    //           so we open-code the loop. No validation of nbits here;
    //           the emitter's arg count check runs before this.
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
    // Polling-based edge counter. The achievable rate on a 16 MHz Uno is
    // roughly 5 kHz for a tight loop; above that, use attach_interrupt +
    // interrupt_count instead.
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
