// runtime/esp32/detail/mcu_serial.h
#pragma once
#include "mcu_types.h"
#include <soc/soc_caps.h>

// Serial I/O. UART0 keeps the exact AVR surface (`Serial`); UART1 and
// UART2 are ESP32-specific additions exposed through the `esp32.*`
// namespace. The ESP32 core pre-instantiates `Serial1`, and `Serial2`
// only on chips with three or more UART controllers (classic ESP32,
// S2, S3). On C3/C6 the serial2_* symbols are absent and a sketch that
// calls them fails at compile time.
//
// All blocking calls use the core's 1 s default timeout so a
// disconnected host cannot hang the loop forever.

static inline VyneValue arduino_serial_available(void) {
    return vyne_int((int64_t)::Serial.available());
}

static inline VyneValue arduino_serial_read(void) {
    return vyne_int((int64_t)::Serial.read());
}

static inline VyneValue arduino_serial_read_line(void) {
    // 64-byte bounded line reader with 1 s timeout. A fixed buffer avoids
    // heap churn and matches the AVR implementation exactly.
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

// ---------------------------------------------------------------------
// UART1 (and UART2 where present).
// ---------------------------------------------------------------------
static inline void _vyne_print_stream(Stream& out, VyneValue v) {
    if (v.type == V_INT64)
        out.print((long)v.as.i64);
    else if (v.type == V_FLOAT64)
        out.print((double)v.as.f64);
    else if (v.type == V_STRING)
        out.print(v.as.str);
    else if (v.type == V_BOOL)
        out.print(v.as.i64 ? "true" : "false");
    else if (v.type == V_NULL)
        out.print("null");
}

static inline VyneValue
esp32_serial1_begin(VyneValue baud, VyneValue rx_pin, VyneValue tx_pin) {
    // Passing -1 for rx/tx keeps the core's default pins for the board.
    ::Serial1.begin(
        (unsigned long)baud.as.i64,
        SERIAL_8N1,
        (int8_t)rx_pin.as.i64,
        (int8_t)tx_pin.as.i64
    );
    return vyne_null();
}

static inline VyneValue esp32_serial1_print(VyneValue v) {
    _vyne_print_stream(::Serial1, v);
    return vyne_null();
}

static inline VyneValue esp32_serial1_available(void) {
    return vyne_int((int64_t)::Serial1.available());
}

static inline VyneValue esp32_serial1_read(void) {
    return vyne_int((int64_t)::Serial1.read());
}

#if defined(SOC_UART_NUM) && SOC_UART_NUM > 2
static inline VyneValue
esp32_serial2_begin(VyneValue baud, VyneValue rx_pin, VyneValue tx_pin) {
    ::Serial2.begin(
        (unsigned long)baud.as.i64,
        SERIAL_8N1,
        (int8_t)rx_pin.as.i64,
        (int8_t)tx_pin.as.i64
    );
    return vyne_null();
}

static inline VyneValue esp32_serial2_print(VyneValue v) {
    _vyne_print_stream(::Serial2, v);
    return vyne_null();
}

static inline VyneValue esp32_serial2_available(void) {
    return vyne_int((int64_t)::Serial2.available());
}

static inline VyneValue esp32_serial2_read(void) {
    return vyne_int((int64_t)::Serial2.read());
}
#endif
