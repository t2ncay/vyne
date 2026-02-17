#ifdef VYNE_JIT_ENABLED

#include "jit_runtime.h"
#include <cstdio>
#include <cmath>

extern "C" void vyne_rt_out_number(double value) {
    if (std::floor(value) == value && std::isfinite(value)) {
        std::printf("%.0f\n", value);
    } else {
        std::printf("%g\n", value);
    }
}

#endif
