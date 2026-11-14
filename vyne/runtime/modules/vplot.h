// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tuncay Gafarli
#pragma once
#include "../vyne_runtime.h"
#ifndef VPLOT_CLOCK_MS
#include <time.h>
static inline uint32_t vp_host_clock_ms(void) {
#ifdef _WIN32
    return (uint32_t)GetTickCount64();
#else
    struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);
    return (uint32_t)((uint64_t)now.tv_sec*1000+now.tv_nsec/1000000);
#endif
}
#define VPLOT_CLOCK_MS() vp_host_clock_ms()
#endif
#include "../vplot/vplot_bridge.h"
