// runtime/esp32/detail/mcu_memory.h
#pragma once
#include "mcu_types.h"
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Memory introspection. Reaches into mcu_arena.h for its private globals
// (g_mcu_cur, g_mcu_peak) and into the ESP-IDF heap allocator for the
// system-wide numbers. These are diagnostics — calling them in a loop
// is a mistake.

static inline VyneValue arduino_arena_peak(void) {
    return vyne_int((int64_t)g_mcu_peak);
}

static inline VyneValue arduino_arena_free(void) {
    // Bytes still available in the 128 KB arena before arena_alloc halts.
    return vyne_int((int64_t)(VYNE_MCU_ARENA_SIZE - g_mcu_cur));
}

static inline VyneValue arduino_free_ram(void) {
    // Total free ESP32 heap, not just the gap between arena and stack.
    // The arena is a static .bss array, so it never appears in this
    // number; use arena_free() for the arena budget itself.
    return vyne_int((int64_t)heap_caps_get_free_size(MALLOC_CAP_8BIT));
}

static inline VyneValue arduino_stack_free(void) {
    // FreeRTOS high-water mark for the calling (loop) task, in bytes.
    // This is the closest analogue to AVR's stack/free-RAM probe but is
    // measured differently: it is the minimum free stack ever observed,
    // not the instantaneous gap to the heap.
    return vyne_int(
        (int64_t)uxTaskGetStackHighWaterMark(NULL) * (int64_t)sizeof(StackType_t)
    );
}

static inline VyneValue esp32_heap_largest_free(void) {
    // Largest contiguous 8-bit-capable block. Compare with free_ram():
    // a large gap signals fragmentation, and this is the number that
    // matters before a big allocation (WiFi buffers, TLS, image decode).
    return vyne_int((int64_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}
