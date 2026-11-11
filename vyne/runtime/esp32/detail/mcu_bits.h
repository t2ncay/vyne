// runtime/arduino/detail/mcu_bits.h
#pragma once
#include "mcu_types.h"

// Bit manipulation. All operate on Int64; on AVR this is 8 bytes emulated
// with 32-bit ops, so keep operands within 32-bit range where the extra
// precision is not needed.

static inline VyneValue arduino_bit_set(VyneValue x, VyneValue n) {
    return vyne_int(x.as.i64 | ((int64_t)1 << n.as.i64));
}
static inline VyneValue arduino_bit_clear(VyneValue x, VyneValue n) {
    return vyne_int(x.as.i64 & ~((int64_t)1 << n.as.i64));
}
static inline VyneValue arduino_bit_toggle(VyneValue x, VyneValue n) {
    return vyne_int(x.as.i64 ^ ((int64_t)1 << n.as.i64));
}
static inline VyneValue arduino_bit_read(VyneValue x, VyneValue n) {
    return vyne_int((x.as.i64 >> n.as.i64) & 1);
}

static inline VyneValue arduino_low_byte(VyneValue x) {
    return vyne_int(x.as.i64 & 0xFF);
}
static inline VyneValue arduino_high_byte(VyneValue x) {
    return vyne_int((x.as.i64 >> 8) & 0xFF);
}
static inline VyneValue arduino_make_word(VyneValue hi, VyneValue lo) {
    return vyne_int(((hi.as.i64 & 0xFF) << 8) | (lo.as.i64 & 0xFF));
}

static inline VyneValue arduino_popcount(VyneValue x) {
    // Kernighan's algorithm — one loop iteration per set bit. For dense
    // data (few zero bits) a 256-byte lookup table is faster, but it
    // costs 256 bytes of scarce flash + needs a PROGMEM read.
    int64_t v = x.as.i64;
    int64_t c = 0;
    while (v) {
        v &= (v - 1);
        ++c;
    }
    return vyne_int(c);
}

static inline VyneValue arduino_crc8(VyneValue data, VyneValue seed) {
    // Dallas/Maxim CRC-8 (polynomial 0x31, MSB-first, init typically 0).
    // One byte per call so the caller can chain:
    //   crc := 0;
    //   through i :: 0..n-1 -> loop { crc = arduino.crc8(byte, crc); };
    // On AVR the 8-iteration loop is faster than a 256-byte table lookup
    // from PROGMEM for the small buffers typical of a sensor frame.
    uint8_t crc = (uint8_t)seed.as.i64;
    uint8_t b = (uint8_t)data.as.i64;
    crc ^= b;
    for (int i = 0; i < 8; ++i) {
        if (crc & 0x80)
            crc = (uint8_t)((crc << 1) ^ 0x31);
        else
            crc = (uint8_t)(crc << 1);
    }
    return vyne_int((int64_t)crc);
}
