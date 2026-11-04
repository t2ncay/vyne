// runtime/arduino/detail/mcu_arena.h
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Configure based on the board. 1024 bytes is safe for an Uno/Nano.
#ifndef VYNE_MCU_ARENA_SIZE
#define VYNE_MCU_ARENA_SIZE 1024
#endif

static uint8_t g_mcu_arena[VYNE_MCU_ARENA_SIZE];
static size_t g_mcu_cur = 0;
static size_t g_mcu_peak = 0;

// Checkpoint stack for `region` blocks
static size_t g_mcu_cp_stack[16];
static int g_mcu_cp_top = 0;

static inline void* arena_alloc(size_t size) {
    size = (size + 7) & ~(size_t)7; // 8-byte align
    if (g_mcu_cur + size > VYNE_MCU_ARENA_SIZE) {
        // Hardware failure: out of SRAM
        // In a real deployment, we will implement a custom error handler.
        while (1)
            ;
    }
    void* ptr = &g_mcu_arena[g_mcu_cur];
    g_mcu_cur += size;
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
    g_mcu_cur = cp.offset;
}
