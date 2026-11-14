// SPDX-License-Identifier: MIT
#include "../../../../vyne/runtime/vraylib/saturation.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    VyneRaySaturation state = {0};
    float bypass[] = {-0.8f, -0.1f, 0.1f, 0.8f, 123.0f};
    vyne_ray_saturation_process(&state, bypass, 2);
    assert(bypass[0] == -0.8f && bypass[3] == 0.8f && bypass[4] == 123.0f);
    assert(vyne_ray_saturation_set(&state, 0.8f, 1));
    float hard[] = {-0.5f, -0.02f, 0.02f, 0.5f, 123.0f};
    vyne_ray_saturation_process(&state, hard, 2);
    assert(hard[0] == -1.0f && hard[3] == 1.0f);
    assert(fabsf(hard[2] - 0.18f) < 0.00001f && hard[4] == 123.0f);
    assert(vyne_ray_saturation_set(&state, 0.8f, 0));
    float soft[] = {-0.5f, -0.02f, 0.02f, 0.5f};
    vyne_ray_saturation_process(&state, soft, 2);
    assert(soft[3] > 0.9f && soft[3] < 1.0f && fabsf(soft[0] + soft[3]) < 0.00001f);
    unsigned before = atomic_load(&state.settings);
    assert(!vyne_ray_saturation_set(&state, NAN, 0));
    assert(!vyne_ray_saturation_set(&state, 1.1f, 0));
    assert(!vyne_ray_saturation_set(&state, 0.5f, 2));
    assert(atomic_load(&state.settings) == before);
    float invalid[] = {NAN, INFINITY};
    vyne_ray_saturation_process(&state, invalid, 1);
    assert(invalid[0] == 0.0f && invalid[1] == 0.0f);
    assert(atomic_load(&state.blocks) == 4);
    puts("VRAYLIB_SATURATION_OK");
    return 0;
}
