// SPDX-License-Identifier: MIT
// Fixed stereo saturation kernel. No allocation, locks, logging or Vyne calls.
#pragma once
#include <math.h>
#include <stddef.h>
#include <stdatomic.h>

_Static_assert(ATOMIC_INT_LOCK_FREE == 2, "vraylib saturation needs lock-free unsigned atomics");
typedef struct { atomic_uint settings; atomic_uint blocks; } VyneRaySaturation;

static inline int vyne_ray_saturation_set(VyneRaySaturation *state, float drive, int mode) {
    if (!isfinite(drive) || drive < 0.0f || drive > 1.0f || (mode != 0 && mode != 1)) return 0;
    unsigned packed = (unsigned)lroundf(drive * 65535.0f) | ((unsigned)mode << 16);
    atomic_store_explicit(&state->settings, packed, memory_order_relaxed);
    return 1;
}
static inline void vyne_ray_saturation_process(VyneRaySaturation *state, float *samples, size_t frames) {
    unsigned packed = atomic_load_explicit(&state->settings, memory_order_relaxed);
    atomic_fetch_add_explicit(&state->blocks, 1, memory_order_relaxed);
    float drive = (float)(packed & 65535u) / 65535.0f;
    if (drive == 0.0f) return;
    float gain = 1.0f + drive * 10.0f;
    float normalization = tanhf(gain);
    for (size_t i = 0; i < frames * 2; ++i) {
        float value = samples[i];
        if (!isfinite(value)) value = 0.0f;
        value *= gain;
        samples[i] = (packed >> 16) ? fminf(1.0f, fmaxf(-1.0f, value)) : tanhf(value) / normalization;
    }
}
