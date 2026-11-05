// runtime/arduino/detail/mcu_math.h
#pragma once
#include "mcu_types.h"
#include <math.h>

#ifndef VYNE_PI
#define VYNE_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------------
// Unboxed _f64 variants. When every argument at a call site is a
// provably-Float64 native expression, the emitter dispatches to these
// instead of the boxed form — no VyneValue construction, no union read.
// This is the same fast path vmath already exposes via NativeMapEntry's
// nativeF64 field.
// ---------------------------------------------------------------------
static inline double arduino_sin_f64(double x) {
    return sin(x);
}
static inline double arduino_cos_f64(double x) {
    return cos(x);
}
static inline double arduino_sqrt_f64(double x) {
    return sqrt(x);
}
static inline double arduino_tan_f64(double x) {
    return tan(x);
}
static inline double arduino_asin_f64(double x) {
    return asin(x);
}
static inline double arduino_acos_f64(double x) {
    return acos(x);
}
static inline double arduino_atan_f64(double x) {
    return atan(x);
}
static inline double arduino_atan2_f64(double y, double x) {
    return atan2(y, x);
}
static inline double arduino_log_f64(double x) {
    return log(x);
}
static inline double arduino_log10_f64(double x) {
    return log10(x);
}
static inline double arduino_exp_f64(double x) {
    return exp(x);
}
static inline double arduino_pow_f64(double x, double y) {
    return pow(x, y);
}
static inline double arduino_fmod_f64(double x, double y) {
    return fmod(x, y);
}

static inline double arduino_constrain_f64(double x, double lo, double hi) {
    if (x < lo)
        return lo;
    if (x > hi)
        return hi;
    return x;
}

static inline double arduino_fmap_f64(
    double x,
    double in_lo,
    double in_hi,
    double out_lo,
    double out_hi
) {
    // Arduino map() equivalent for floats. Division by zero is
    // deliberately not guarded: a degenerate input range is a caller
    // bug, and NaN propagates to the use site rather than masking it.
    return out_lo + (x - in_lo) * (out_hi - out_lo) / (in_hi - in_lo);
}

static inline double arduino_lerp_f64(double a, double b, double t) {
    return a + (b - a) * t;
}

static inline double arduino_smoothstep_f64(double a, double b, double x) {
    // Standard Hermite ramp. Clamped to [0, 1] so out-of-range x
    // produces a plateau, matching GLSL smoothstep semantics.
    double t = (x - a) / (b - a);
    if (t < 0.0)
        t = 0.0;
    if (t > 1.0)
        t = 1.0;
    return t * t * (3.0 - 2.0 * t);
}

static inline double arduino_radians_f64(double deg) {
    return deg * (VYNE_PI / 180.0);
}
static inline double arduino_degrees_f64(double rad) {
    return rad * (180.0 / VYNE_PI);
}

// ---------------------------------------------------------------------
// Boxed forms. Required as the fallback whenever an argument is not
// statically Float64 (Int64 literal, dynamic result, mixed site).
// ---------------------------------------------------------------------
static inline VyneValue arduino_tan(VyneValue x) {
    return vyne_float(tan(x.as.f64));
}
static inline VyneValue arduino_asin(VyneValue x) {
    return vyne_float(asin(x.as.f64));
}
static inline VyneValue arduino_acos(VyneValue x) {
    return vyne_float(acos(x.as.f64));
}
static inline VyneValue arduino_atan(VyneValue x) {
    return vyne_float(atan(x.as.f64));
}
static inline VyneValue arduino_atan2(VyneValue y, VyneValue x) {
    return vyne_float(atan2(y.as.f64, x.as.f64));
}
static inline VyneValue arduino_log(VyneValue x) {
    return vyne_float(log(x.as.f64));
}
static inline VyneValue arduino_log10(VyneValue x) {
    return vyne_float(log10(x.as.f64));
}
static inline VyneValue arduino_exp(VyneValue x) {
    return vyne_float(exp(x.as.f64));
}
static inline VyneValue arduino_pow(VyneValue x, VyneValue y) {
    return vyne_float(pow(x.as.f64, y.as.f64));
}
static inline VyneValue arduino_fmod(VyneValue x, VyneValue y) {
    return vyne_float(fmod(x.as.f64, y.as.f64));
}

static inline VyneValue
arduino_constrain(VyneValue x, VyneValue lo, VyneValue hi) {
    return vyne_float(arduino_constrain_f64(x.as.f64, lo.as.f64, hi.as.f64));
}
static inline VyneValue arduino_fmap(
    VyneValue x,
    VyneValue in_lo,
    VyneValue in_hi,
    VyneValue out_lo,
    VyneValue out_hi
) {
    return vyne_float(arduino_fmap_f64(
        x.as.f64,
        in_lo.as.f64,
        in_hi.as.f64,
        out_lo.as.f64,
        out_hi.as.f64
    ));
}
static inline VyneValue arduino_lerp(VyneValue a, VyneValue b, VyneValue t) {
    return vyne_float(arduino_lerp_f64(a.as.f64, b.as.f64, t.as.f64));
}
static inline VyneValue
arduino_smoothstep(VyneValue a, VyneValue b, VyneValue x) {
    return vyne_float(arduino_smoothstep_f64(a.as.f64, b.as.f64, x.as.f64));
}
static inline VyneValue arduino_radians(VyneValue deg) {
    return vyne_float(arduino_radians_f64(deg.as.f64));
}
static inline VyneValue arduino_degrees(VyneValue rad) {
    return vyne_float(arduino_degrees_f64(rad.as.f64));
}

// ---------------------------------------------------------------------
// PRNG. `random()` in the Arduino core is overloaded ((max) and
// (min, max)); C has no overloading, so the variadic form is exposed
// via NativeMapEntry::usesArgv. `random_seed` reseeds the LCG.
// ---------------------------------------------------------------------
static inline VyneValue arduino_random_variadic(int argc, VyneValue* argv) {
    if (argc == 1) {
        return vyne_int((int64_t)::random((long)argv[0].as.i64));
    }
    if (argc == 2) {
        return vyne_int(
            (int64_t)::random((long)argv[0].as.i64, (long)argv[1].as.i64)
        );
    }
    return vyne_int(0);
}

static inline VyneValue arduino_random_seed(VyneValue seed) {
    ::randomSeed((unsigned long)seed.as.i64);
    return vyne_null();
}
