// runtime/esp32/vyne_esp32.h
//
// Single-translation-unit entry point for ESP32 firmware. Include this
// from the generated .ino; the compiler emits the VYNE_USE_<FEATURE>
// defines it needs immediately before this include.
#pragma once
#if defined(ARDUINO_ARCH_ESP32) && !defined(ESP32)
#error "ESP32 platform define missing: use compiler.cpp.extra_flags instead of replacing build.extra_flags"
#endif
#include "detail/mcu_arena.h"
#include "detail/mcu_bits.h"
#include "detail/mcu_hardware.h"
#include "detail/mcu_operators.h"
#if defined(VYNE_USE_I2C)
#include "detail/mcu_i2c.h"
#endif
#include "detail/mcu_interrupts.h"
#include "detail/mcu_math.h"
#include "detail/mcu_memory.h"
#include "detail/mcu_pins_ext.h"
#if defined(VYNE_USE_CONTRACTS)
#include "detail/mcu_contracts.h"
#endif
#if defined(VYNE_USE_REALTIME)
#include "detail/mcu_realtime.h"
#endif
#include "detail/mcu_serial.h"
#include "detail/mcu_timing.h"
#include "detail/mcu_types.h"
#if defined(VYNE_USE_SPI)
#include "detail/mcu_spi.h"
#endif
#if defined(VYNE_USE_NVS)
#include "detail/mcu_nvs.h"
#endif
#if defined(VYNE_USE_WIFI)
#include "detail/mcu_wifi.h"
#endif
#if defined(VYNE_USE_DISPLAY)
#include "detail/mcu_display.h"
#endif
#if defined(VYNE_USE_TOUCH)
#include "detail/mcu_touch.h"
#endif
#if defined(VYNE_USE_IMU)
#include "detail/mcu_imu.h"
#endif
#if defined(VYNE_USE_POWER)
#include "detail/mcu_power.h"
#endif
#include <Arduino.h>

// Declare the Vyne program's setup and loop
extern void vyne_setup();
extern void vyne_loop();

void setup() {
#ifndef VYNE_SERIAL_BAUD
#define VYNE_SERIAL_BAUD 115200
#endif
    ::Serial.begin(VYNE_SERIAL_BAUD);
    // Initialize the arena
    arena_free_all();
    vyne_setup();
}

void loop() {
    vyne_loop();
}
