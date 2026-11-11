// runtime/esp32/detail/mcu_arena.h
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Arena size. The ESP32-WROOM-32 has 520 KB of SRAM. Reserving 128 KB
// for the Vyne arena leaves roughly 390 KB for the WiFi/BT stacks,
// FreeRTOS objects, and the Arduino core, which is the comfortable
// operating envelope for a typical application. 128 KB is also far more
// than any embedded Vyne program that respects region discipline needs:
// a region body allocates, then rewinds to its checkpoint, so peak use
// is driven by a single region's working set, not by program length.
#ifndef VYNE_MCU_ARENA_SIZE
#define VYNE_MCU_ARENA_SIZE 131072
#endif

static uint8_t g_mcu_arena[VYNE_MCU_ARENA_SIZE];
static size_t g_mcu_cur = 0;
static size_t g_mcu_peak = 0;

// The desktop runtime exposes allocation counters through `g_arena`.
// The generated code for `@bounded` regions reads
// `g_arena.total_allocated` on every target, so the MCU runtime keeps a
// matching running total. `total_allocated` is the live byte count, not
// the peak; `arena_used()` remains the canonical MCU introspection call.
typedef struct {
    size_t total_allocated;
} VyneMcuArenaStats;
static VyneMcuArenaStats g_arena = {0};

// Region-bound violation. There is no console on a bare MCU, so halt.
// A stopped sketch is the clearest signal that the declared budget was
// exceeded; `vynec verify` compares the compiler's prediction against
// the budget on the host side.
static inline void vyne_panic_region_bound(
    const char* name,
    int line,
    size_t bound,
    size_t used
) {
    (void)name;
    (void)line;
    (void)bound;
    (void)used;
    while (1) {
    }
}

// Checkpoint stack for `region` blocks. ESP32 programs routinely nest
// deeper than AVR (device/display/touch setup inside a body), so this
// holds 32 frames instead of the Uno's 16.
static size_t g_mcu_cp_stack[32];
static int g_mcu_cp_top = 0;

static inline void* arena_alloc(size_t size) {
    size = (size + 7) & ~(size_t)7; // 8-byte align
    if (g_mcu_cur + size > VYNE_MCU_ARENA_SIZE) {
        // Hardware failure: out of SRAM. Halt rather than return a
        // half-valid pointer; the sketch author sees a dead board and
        // the compile-time region budget points at the real culprit.
        while (1)
            ;
    }
    void* ptr = &g_mcu_arena[g_mcu_cur];
    g_mcu_cur += size;
    g_arena.total_allocated += size;
    if (g_mcu_cur > g_mcu_peak)
        g_mcu_peak = g_mcu_cur;
    return ptr;
}

static inline void* arena_alloc_aligned(size_t size, size_t align) {
    // Simplified for MCU: just calls arena_alloc
    return arena_alloc(size);
}

static inline void arena_free_all(void) {
    g_mcu_cur = 0;
    g_mcu_peak = 0;
    g_mcu_cp_top = 0;
    g_arena.total_allocated = 0;
}

// Byte count currently in use. Exposed via arduino.arena_used().
static inline size_t arena_used(void) {
    return g_mcu_cur;
}

typedef struct {
    size_t offset;
} ArenaCheckpoint;

static inline ArenaCheckpoint arena_checkpoint(void) {
    ArenaCheckpoint cp;
    cp.offset = g_mcu_cur;
    return cp;
}

static inline void arena_rewind(ArenaCheckpoint cp) {
    if (g_mcu_cur > cp.offset) {
        size_t freed = g_mcu_cur - cp.offset;
        if (g_arena.total_allocated >= freed)
            g_arena.total_allocated -= freed;
        else
            g_arena.total_allocated = 0;
    }
    g_mcu_cur = cp.offset;
}
