// runtime/arduino/vyne_arduino.h
#pragma once
#include "detail/mcu_arena.h"
#include "detail/mcu_hardware.h"
#include "detail/mcu_types.h"
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
