// runtime/arduino/detail/mcu_serial.h
#pragma once
#include "mcu_types.h"

// Serial I/O for the Uno R3. Complements the two primitives already in
// mcu_hardware.h (serial_begin, serial_print). These are the missing
// seven calls every interactive sketch needs.
//
// All blocking calls respect a 1 s internal timeout so a disconnected
// host does not hang the loop forever. The timeout is not user-tunable —
// if you need a different one, poll available() in a bounded loop.

static inline VyneValue arduino_serial_available(void) {
    return vyne_int((int64_t)::Serial.available());
}

static inline VyneValue arduino_serial_read(void) {
    // Returns -1 when the RX buffer is empty, matching Arduino.
    return vyne_int((int64_t)::Serial.read());
}

static inline VyneValue arduino_serial_read_line(void) {
    // 64-byte bounded line reader with 1 s timeout. Not using String —
    // String allocates from the AVR heap and every allocation is a
    // potential fragmentation event on 2 KB of SRAM. The fixed buffer
    // lives in .bss and the returned VyneValue copies into the arena.
    static char buf[64];
    size_t len = 0;
    unsigned long start = millis();

    while ((millis() - start) < 1000UL && len < sizeof(buf) - 1) {
        if (::Serial.available()) {
            int c = ::Serial.read();
            if (c < 0)
                continue;
            if (c == '\n')
                break;
            if (c == '\r')
                continue;
            buf[len++] = (char)c;
        }
    }
    buf[len] = '\0';
    return vyne_string(buf); // arena copy
}

static inline VyneValue arduino_serial_parse_int(void) {
    // Blocking, 1 s default timeout from the Arduino core.
    return vyne_int((int64_t)::Serial.parseInt());
}

static inline VyneValue arduino_serial_parse_float(void) {
    return vyne_float((double)::Serial.parseFloat());
}

static inline VyneValue arduino_serial_println(VyneValue v) {
    _vyne_print_internal(v);
    ::Serial.println();
    return vyne_null();
}

static inline VyneValue arduino_serial_flush(void) {
    ::Serial.flush();
    return vyne_null();
}
