// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
//
// This file is part of the Vyne runtime library, distributed under the
// MIT License. See LICENSE-MIT at the repository root for the full text.

#pragma once
// runtime/detail/realtime.h — @realtime<ms> deadline support.
//
// The compiler brackets a @realtime region with
//     uint64_t start = vyne_realtime_now_us();
//     ...
//     vyne_realtime_check(name, line, deadline_ms, start);
//
// `check` records the elapsed time in a per-region stats table and
// panics with VNE-109 when the deadline is exceeded. Early returns
// emit the same check through ReturnNode, so every exit path is
// measured. This is a runtime watchdog, not a static WCET proof.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <time.h>
#endif

// Monotonic microseconds. QPC on Windows, CLOCK_MONOTONIC elsewhere.
static inline uint64_t vyne_realtime_now_us(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq;
    static int freq_ready = 0;
    if (!freq_ready) {
        QueryPerformanceFrequency(&freq);
        freq_ready = 1;
    }
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (uint64_t)((counter.QuadPart * 1000000LL) / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)(ts.tv_nsec / 1000);
#endif
}

#define VYNE_REALTIME_MAX_REGIONS 16

typedef struct {
    const char* name;
    uint64_t worst_us;
    uint64_t overruns;
    uint64_t checks;
} VyneRealtimeStat;

static VyneRealtimeStat g_vyne_realtime[VYNE_REALTIME_MAX_REGIONS];
static int g_vyne_realtime_count = 0;

static inline VyneRealtimeStat* vyne_realtime_slot(const char* name) {
    for (int i = 0; i < g_vyne_realtime_count; ++i) {
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
    // Stats are best-effort; overflow must not fail the region.
    return &g_vyne_realtime[0];
}

static inline void vyne_panic_deadline(
    const char* name,
    int line,
    int64_t deadline_ms,
    uint64_t elapsed_us
) {
    fprintf(
        stderr,
        "Runtime error (VNE-109): @realtime region '%s' exceeded its "
        "%lld ms deadline: %.3f ms (line %d)\n"
        "  Reduce the work per tick, raise the deadline, or split the "
        "region.\n",
        name,
        (long long)deadline_ms,
        (double)elapsed_us / 1000.0,
        line
    );
    exit(1);
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
        s->worst_us = elapsed;
    if (elapsed > (uint64_t)(deadline_ms * 1000)) {
        s->overruns++;
        vyne_panic_deadline(name, line, deadline_ms, elapsed);
    }
}

static inline double vyne_realtime_worst_ms(void) {
    uint64_t worst = 0;
    for (int i = 0; i < g_vyne_realtime_count; ++i)
        if (g_vyne_realtime[i].worst_us > worst)
            worst = g_vyne_realtime[i].worst_us;
    return (double)worst / 1000.0;
}

static inline int64_t vyne_realtime_overruns(void) {
    int64_t total = 0;
    for (int i = 0; i < g_vyne_realtime_count; ++i)
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
    for (int i = 0; i < g_vyne_realtime_count; ++i) {
        g_vyne_realtime[i].worst_us = 0;
        g_vyne_realtime[i].overruns = 0;
        g_vyne_realtime[i].checks = 0;
    }
}
