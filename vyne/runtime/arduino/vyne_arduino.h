// runtime/arduino/vyne_arduino.h
#pragma once
#include "detail/mcu_arena.h"
#include "detail/mcu_bits.h"
#include "detail/mcu_eeprom.h"
#include "detail/mcu_hardware.h"
#if defined(VYNE_USE_I2C)
#include "detail/mcu_i2c.h"
#endif
#include "detail/mcu_interrupts.h"
#include "detail/mcu_math.h"
#include "detail/mcu_memory.h"
#include "detail/mcu_pins_ext.h"
#include "detail/mcu_serial.h"
#include "detail/mcu_timing.h"
#include "detail/mcu_types.h"
#if defined(VYNE_USE_SPI)
#include "detail/mcu_spi.h"
#endif
#include <Arduino.h>

// Declare the Vyne program's setup and loop
extern void vyne_setup();
extern void vyne_loop();

void setup() {
    ::Serial.begin(115200);
    // Initialize the arena
    arena_free_all();
    vyne_setup();
}

void loop() {
    vyne_loop();
}
