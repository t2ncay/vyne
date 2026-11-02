// runtime/arduino/detail/mcu_hardware.h
#pragma once
#include "mcu_types.h"

// These names match the `native_maps.h` additions (Step 5)
static inline VyneValue arduino_pin_mode(VyneValue pin, VyneValue mode) {
    pinMode(pin.as.i64, mode.as.i64);
    return vyne_null();
}

static inline VyneValue arduino_digital_write(VyneValue pin, VyneValue val) {
    digitalWrite(pin.as.i64, val.as.i64);
    return vyne_null();
}

static inline VyneValue arduino_digital_read(VyneValue pin) {
    return vyne_int(digitalRead(pin.as.i64));
}

static inline VyneValue arduino_analog_read(VyneValue pin) {
    return vyne_int(analogRead(pin.as.i64));
}

static inline VyneValue arduino_analog_write(VyneValue pin, VyneValue val) {
    analogWrite(pin.as.i64, val.as.i64);
    return vyne_null();
}

static inline VyneValue arduino_delay(VyneValue ms) {
    delay(ms.as.i64);
    return vyne_null();
}

static inline VyneValue arduino_millis(void) {
    return vyne_int(millis());
}
