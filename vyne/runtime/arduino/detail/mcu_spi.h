// runtime/arduino/detail/mcu_spi.h
//
// SPI bindings for the AVR target, backed by the bundled Arduino `SPI`
// library. The build integration compiles `vendor/arduino/libraries/SPI/src`
// and puts it on the include path; see cli/file_handler.cpp.
//
// Transaction model: call `spi.set(clock_hz, mode)` once (or whenever the
// bus configuration changes), then `spi.transfer` / `spi.transfer16` any
// number of times, then `spi.end_transaction()`. `spi.begin()` powers the
// peripheral; `spi.end()` releases it.
//
// Return-code contract, shared with the typed `vspi` surface:
//
//   -1   transfer failed / no data (not produced today; reserved)
//   -2   invalid clock (validated in vspi)
//   -3   invalid mode, value, or bit width (validated in vspi)

#pragma once
#include "mcu_types.h"
#include <SPI.h>

static inline VyneValue arduino_spi_begin(void) {
    SPI.begin();
    return vyne_null();
}

static inline VyneValue arduino_spi_end(void) {
    SPI.end();
    return vyne_null();
}

// Configure the bus for a transaction. `mode` is the SPI data mode 0..3;
// an out-of-range value falls back to mode 0 so a bad constant cannot hang
// the bus. A zero clock keeps the Arduino 4 MHz default.
static inline VyneValue arduino_spi_set(VyneValue clock_hz, VyneValue mode) {
    uint32_t hz = (uint32_t)clock_hz.as.i64;
    if (hz == 0)
        hz = 4000000UL;

    uint8_t m = (uint8_t)mode.as.i64;
    uint8_t dataMode = SPI_MODE0;
    if (m == 1)
        dataMode = SPI_MODE1;
    else if (m == 2)
        dataMode = SPI_MODE2;
    else if (m == 3)
        dataMode = SPI_MODE3;

    SPI.beginTransaction(SPISettings(hz, MSBFIRST, dataMode));
    return vyne_null();
}

static inline VyneValue arduino_spi_end_transaction(void) {
    SPI.endTransaction();
    return vyne_null();
}

// Full-duplex single-byte transfer. Returns the byte shifted in on MISO.
static inline VyneValue arduino_spi_transfer(VyneValue value) {
    return vyne_int((int64_t)SPI.transfer((uint8_t)value.as.i64));
}

// Full-duplex 16-bit transfer. Byte order follows the configured bit order
// (MSB first by default), matching the Arduino SPI library.
static inline VyneValue arduino_spi_transfer16(VyneValue value) {
    return vyne_int((int64_t)SPI.transfer16((uint16_t)value.as.i64));
}
