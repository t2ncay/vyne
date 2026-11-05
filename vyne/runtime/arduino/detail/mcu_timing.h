// runtime/arduino/detail/mcu_timing.h
#pragma once
#include "mcu_types.h"

// Timing primitives beyond millis/micros/delay, which live in
// mcu_hardware.h. pulse_width is the auto-detect form of pulse_in;
// timer_read exposes Timer1's raw counter for sketches that configure
// the timer themselves.

static inline VyneValue arduino_pulse_width(VyneValue pin) {
    // Measures the HIGH portion of the next pulse. For an active-LOW
    // sensor, call pulse_in(pin, LOW, timeout_us) directly.
    unsigned long us = ::pulseIn((uint8_t)pin.as.i64, HIGH);
    return vyne_int((int64_t)us);
}

static inline VyneValue arduino_timer_read(void) {
    // Timer1 raw 16-bit counter. Returns 0 until the sketch configures
    // Timer1 (TCCR1A/B, prescaler) via avr-libc registers. Wraps at
    // 65536 counts; tick interval depends on the configured prescaler.
    return vyne_int((int64_t)TCNT1);
}
