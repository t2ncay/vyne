// runtime/arduino/detail/mcu_i2c.h
//
// I2C / TWI bindings for the AVR target, backed by the bundled Arduino
// `Wire` library. This is the first binding that depends on a bundled
// library rather than on avr-libc alone; the AVR build integration adds
// every `vendor/arduino/libraries/*/src` directory to the include path
// and compiles its `.c` / `.cpp` sources (see cli/file_handler.cpp).
//
// Return-code contract, shared with the typed `vi2c` surface:
//
//    0      success (Wire.endTransmission returned ACK)
//    1..4   Wire.endTransmission failure (data-too-long, NACK addr,
//           NACK data, other)
//   -1      read failed: no byte available / NACK on register select
//   -2      invalid device address (validated in vi2c before the call)
//   -3      invalid register or value (validated in vi2c)
//
// The C layer casts to uint8_t; it does not validate. Validation lives in
// the Vyne wrapper so a caller gets a diagnostic instead of a silent
// truncation to 8 bits.

#pragma once
#include "mcu_types.h"
#include <Wire.h>

// Start the TWI peripheral as a bus master (the common case).
static inline VyneValue arduino_i2c_begin(void) {
    Wire.begin();
    return vyne_null();
}

static inline VyneValue arduino_i2c_end(void) {
    Wire.end();
    return vyne_null();
}

// Set the bus clock. 100 kHz is the I2C standard-mode default; 400 kHz is
// fast mode. A zero argument keeps the Arduino default rather than
// programming a zero divider.
static inline VyneValue arduino_i2c_set_clock(VyneValue hz) {
    uint32_t clock = (uint32_t)hz.as.i64;
    if (clock == 0)
        clock = 100000UL;
    Wire.setClock(clock);
    return vyne_null();
}

// Write one byte to a device. Returns the endTransmission status.
static inline VyneValue arduino_i2c_write(VyneValue addr, VyneValue value) {
    Wire.beginTransmission((uint8_t)addr.as.i64);
    Wire.write((uint8_t)value.as.i64);
    return vyne_int((int64_t)Wire.endTransmission());
}

// Read one byte from a device. Returns -1 when nothing is available.
static inline VyneValue arduino_i2c_read(VyneValue addr) {
    uint8_t received = Wire.requestFrom((uint8_t)addr.as.i64, (uint8_t)1);
    if (received == 0 || Wire.available() <= 0)
        return vyne_int(-1);
    return vyne_int((int64_t)Wire.read());
}

// Write one byte to a register. Returns the endTransmission status.
static inline VyneValue
arduino_i2c_write_reg(VyneValue addr, VyneValue reg, VyneValue value) {
    Wire.beginTransmission((uint8_t)addr.as.i64);
    Wire.write((uint8_t)reg.as.i64);
    Wire.write((uint8_t)value.as.i64);
    return vyne_int((int64_t)Wire.endTransmission());
}

// Read one byte from a register using a repeated start:
//   START, address+W, register, repeated START, address+R, data, STOP.
// Returns -1 when the register select is NACKed or no byte arrives.
static inline VyneValue arduino_i2c_read_reg(VyneValue addr, VyneValue reg) {
    Wire.beginTransmission((uint8_t)addr.as.i64);
    Wire.write((uint8_t)reg.as.i64);
    if (Wire.endTransmission(false) != 0)
        return vyne_int(-1);
    uint8_t received = Wire.requestFrom((uint8_t)addr.as.i64, (uint8_t)1);
    if (received == 0 || Wire.available() <= 0)
        return vyne_int(-1);
    return vyne_int((int64_t)Wire.read());
}

static inline VyneValue arduino_i2c_available(void) {
    return vyne_int((int64_t)Wire.available());
}
