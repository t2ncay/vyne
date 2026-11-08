// runtime/arduino/detail/mcu_realtime.h
//
// @realtime<ms> deadline support for the AVR target. `micros()` is the
// monotonic source; it wraps after ~70 minutes, which is acceptable for
// control loops that check a deadline every tick. The compiler emits
//     uint64_t start = vyne_realtime_now_us();
//     ... body ...
//     vyne_realtime_check(name, line, deadline_ms, start);
// and the same check on early-return paths.
//
// The stats table and panics mirror the desktop runtime so the same
// `@realtime` source is portable; telemetry is reachable from Vyne as
// `arduino.realtime_*`.

#pragma once
#include "mcu_types.h"

static inline uint64_t vyne_realtime_now_us(void) {
    return (uint64_t)micros();
}

#define VYNE_REALTIME_MAX_REGIONS 8

typedef struct {
    const char* name;
    uint32_t worst_us;
    uint32_t overruns;
    uint32_t checks;
} VyneRealtimeStat;

static VyneRealtimeStat g_vyne_realtime[VYNE_REALTIME_MAX_REGIONS];
static uint8_t g_vyne_realtime_count = 0;

static inline VyneRealtimeStat* vyne_realtime_slot(const char* name) {
    for (uint8_t i = 0; i < g_vyne_realtime_count; ++i) {
        if (strcmp(g_vyne_realtime[i].name, name) == 0)
            return &g_vyne_realtime[i];
    }
    if (g_vyne_realtime_count < VYNE_REALTIME_MAX_REGIONS) {
        VyneRealtimeStat* s = &g_vyne_realtime[g_vyne_realtime_count++];
        s->name = name;
        s->worst_us = 0;
        s->overruns = 0;
        s->checks = 0;
        return s;
    }
    return &g_vyne_realtime[0];
}

static inline void vyne_panic_deadline(
    const char* name,
    int line,
    int64_t deadline_ms,
    uint64_t elapsed_us
) {
    ::Serial.print("Runtime error (VNE-109): @realtime region '");
    ::Serial.print(name);
    ::Serial.print("' exceeded its ");
    ::Serial.print((long)deadline_ms);
    ::Serial.print(" ms deadline: ");
    ::Serial.print((double)elapsed_us / 1000.0);
    ::Serial.print(" ms (line ");
    ::Serial.print(line);
    ::Serial.println(")");
    while (1) {
    }
}

static inline void vyne_realtime_check(
    const char* name,
    int line,
    int64_t deadline_ms,
    uint64_t start_us
) {
    uint64_t elapsed = vyne_realtime_now_us() - start_us;
    VyneRealtimeStat* s = vyne_realtime_slot(name);
    s->checks++;
    if (elapsed > s->worst_us)
        s->worst_us = (uint32_t)elapsed;
    if (elapsed > (uint64_t)(deadline_ms * 1000)) {
        s->overruns++;
        vyne_panic_deadline(name, line, deadline_ms, elapsed);
    }
}

static inline double vyne_realtime_worst_ms(void) {
    uint32_t worst = 0;
    for (uint8_t i = 0; i < g_vyne_realtime_count; ++i)
        if (g_vyne_realtime[i].worst_us > worst)
            worst = g_vyne_realtime[i].worst_us;
    return (double)worst / 1000.0;
}

static inline int64_t vyne_realtime_overruns(void) {
    int64_t total = 0;
    for (uint8_t i = 0; i < g_vyne_realtime_count; ++i)
        total += (int64_t)g_vyne_realtime[i].overruns;
    return total;
}

static inline double vyne_realtime_worst_ms_named(const char* name) {
    VyneRealtimeStat* s = vyne_realtime_slot(name);
    return (double)s->worst_us / 1000.0;
}

static inline int64_t vyne_realtime_overruns_named(const char* name) {
    VyneRealtimeStat* s = vyne_realtime_slot(name);
    return (int64_t)s->overruns;
}

static inline void vyne_realtime_reset(void) {
    for (uint8_t i = 0; i < g_vyne_realtime_count; ++i) {
        g_vyne_realtime[i].worst_us = 0;
        g_vyne_realtime[i].overruns = 0;
        g_vyne_realtime[i].checks = 0;
    }
}

// --- typed surface (arduino.realtime_*) ---

static inline VyneValue arduino_realtime_worst_ms(void) {
    return vyne_float(vyne_realtime_worst_ms());
}

static inline VyneValue arduino_realtime_overruns(void) {
    return vyne_int(vyne_realtime_overruns());
}

static inline VyneValue arduino_realtime_reset(void) {
    vyne_realtime_reset();
    return vyne_null();
}

static inline VyneValue arduino_realtime_worst_ms_named(VyneValue name) {
    const char* n =
        (name.type == V_STRING && name.as.str) ? name.as.str : "";
    return vyne_float(vyne_realtime_worst_ms_named(n));
}

static inline VyneValue arduino_realtime_overruns_named(VyneValue name) {
    const char* n =
        (name.type == V_STRING && name.as.str) ? name.as.str : "";
    return vyne_int(vyne_realtime_overruns_named(n));
}
