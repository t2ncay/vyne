// runtime/arduino/detail/mcu_eeprom.h
#pragma once
#include "mcu_types.h"
#include <avr/eeprom.h>

// Persistent storage across resets. 1 KB on the Uno R3, byte-addressable,
// ~100k write cycles per byte.
//
// Implemented directly on top of avr-libc's <avr/eeprom.h> rather than the
// bundled Arduino EEPROM library. That library lives at
// arduino/libraries/EEPROM/src/EEPROM.h and is NOT on the include path of
// the Vyne AVR build integration (which only adds arduino/cores/arduino/).
// Using avr-libc keeps this header buildable in every environment where
// avr-gcc can already compile the core — no library-install step required.
//
// Semantics match EEPROM.get()/put(): `update_byte` writes only when the
// new byte differs, extending cell life. AVR `double` is 4 bytes (same
// as float); `int64_t` is 8 bytes. Mixing the two at the same address
// corrupts the slot — same caveat as the Arduino library.

static inline VyneValue arduino_eeprom_read_int(VyneValue addr) {
    int a = (int)addr.as.i64;
    int64_t v = 0;
    uint8_t* p = (uint8_t*)&v;
    for (size_t i = 0; i < sizeof(v); ++i)
        p[i] = eeprom_read_byte((const uint8_t*)(a + i));
    return vyne_int(v);
}

static inline VyneValue
arduino_eeprom_write_int(VyneValue addr, VyneValue val) {
    int a = (int)addr.as.i64;
    int64_t v = val.as.i64;
    const uint8_t* p = (const uint8_t*)&v;
    for (size_t i = 0; i < sizeof(v); ++i)
        eeprom_update_byte((uint8_t*)(a + i), p[i]);
    return vyne_null();
}

static inline VyneValue arduino_eeprom_read_float(VyneValue addr) {
    int a = (int)addr.as.i64;
    double v = 0.0;
    uint8_t* p = (uint8_t*)&v;
    for (size_t i = 0; i < sizeof(v); ++i)
        p[i] = eeprom_read_byte((const uint8_t*)(a + i));
    return vyne_float(v);
}

static inline VyneValue
arduino_eeprom_write_float(VyneValue addr, VyneValue val) {
    int a = (int)addr.as.i64;
    double v = val.as.f64;
    const uint8_t* p = (const uint8_t*)&v;
    for (size_t i = 0; i < sizeof(v); ++i)
        eeprom_update_byte((uint8_t*)(a + i), p[i]);
    return vyne_null();
}
