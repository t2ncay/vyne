// runtime/arduino/detail/mcu_memory.h
#pragma once
#include "mcu_types.h"

// Memory introspection. Reaches into mcu_arena.h for its private globals
// (g_mcu_cur, g_mcu_peak) and into avr-libc for the heap/stack boundary.
// These are diagnostics — calling them in a loop is a mistake.

static inline VyneValue arduino_arena_peak(void) {
    return vyne_int((int64_t)g_mcu_peak);
}

static inline VyneValue arduino_arena_free(void) {
    // Bytes still available in the 1 KB arena before arena_alloc halts.
    return vyne_int((int64_t)(VYNE_MCU_ARENA_SIZE - g_mcu_cur));
}

// Shared free-RAM computation, factored once. `__brkval` is 0 until the
// first heap allocation; use `__heap_start` in that case.
static inline int _vyne_free_ram_bytes(void) {
    extern int __heap_start;
    extern int* __brkval;
    int here;
    int heap_end = (__brkval == 0) ? (int)&__heap_start : (int)__brkval;
    return (int)&here - heap_end;
}

static inline VyneValue arduino_free_ram(void) {
    return vyne_int((int64_t)_vyne_free_ram_bytes());
}

static inline VyneValue arduino_stack_free(void) {
    // Identical to free_ram on AVR: without stack-painting instrumentation
    // there is no separate "stack watermark" to report. Exposing both
    // names lets sketches use whichever reads more naturally.
    return vyne_int((int64_t)_vyne_free_ram_bytes());
}
