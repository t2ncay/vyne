// runtime/esp32/detail/mcu_timing.h
#pragma once
#include "mcu_types.h"
#include <esp_timer.h>

// Timing primitives beyond millis/micros/delay.

static inline VyneValue arduino_pulse_width(VyneValue pin) {
    // Measures the HIGH portion of the next pulse. For an active-LOW
    // sensor, call pulse_in(pin, LOW, timeout_us) directly.
    unsigned long us = ::pulseIn((uint8_t)pin.as.i64, HIGH);
    return vyne_int((int64_t)us);
}

static inline VyneValue arduino_timer_read(void) {
    // Semantically different from AVR's TCNT1. ESP32 has no user-visible
    // Timer1 counter; `esp_timer_get_time()` is a 64-bit microsecond
    // monotonic clock since boot. It does not wrap in any practical
    // deployment window, so it is safe to subtract across long loops.
    return vyne_int((int64_t)esp_timer_get_time());
}
