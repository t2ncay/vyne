/* vyne/runtime/modules/vmath.h
 * -------------------------------------------------------------------
 * Vyne runtime math module — transpiler target.
 * Mirror of the interpreter's VMathNative namespace, ported to
 * static-inline C functions that operate on VyneValue directly.
 *
 * Naming convention: vmath_<name>
 *   - Methods  → vmath_sqrt(v), vmath_pow(b, e), vmath_clamp(v,lo,hi) …
 *   - Constants → vmath_pi(), vmath_e(), vmath_inf() …  (zero-arg getters)
 *
 * Emitter getNativeMapping should resolve:
 *   isCall=true  → function name only, e.g. "vmath_sqrt"
 *                  (caller appends the arg list)
 *   isCall=false → full expression,    e.g. "vmath_pi()"
 * ------------------------------------------------------------------- */

#ifndef VYNE_VMATH_RT_H
#define VYNE_VMATH_RT_H

#include "../vyne_runtime.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>

/* ===================================================================
 * Internal helper — coerce VyneValue to double regardless of storage
 * =================================================================== */
static inline double _vmath_f(VyneValue v) {
    return (v.type == V_INT64) ? (double)v.as.i64 : v.as.f64;
}

/* ===================================================================
 * Constants  (zero-argument getters; used by member-access path)
 * =================================================================== */
static inline VyneValue vmath_pi()      { return vyne_float(3.141592653589793);  }
static inline VyneValue vmath_e()       { return vyne_float(2.718281828459045);  }
static inline VyneValue vmath_tau()     { return vyne_float(6.283185307179586);  }
static inline VyneValue vmath_phi()     { return vyne_float(1.618033988749895);  }
static inline VyneValue vmath_sqrt2()   { return vyne_float(1.4142135623730951); }
static inline VyneValue vmath_inf()     { return vyne_float((double)INFINITY);   }
static inline VyneValue vmath_nan()     { return vyne_float((double)NAN);        }

/* ===================================================================
 * 1-argument functions
 * =================================================================== */
static inline VyneValue vmath_sqrt(VyneValue v)    { return vyne_float(sqrt(_vmath_f(v)));   }
static inline VyneValue vmath_sin(VyneValue v)     { return vyne_float(sin(_vmath_f(v)));    }
static inline VyneValue vmath_cos(VyneValue v)     { return vyne_float(cos(_vmath_f(v)));    }
static inline VyneValue vmath_tan(VyneValue v)     { return vyne_float(tan(_vmath_f(v)));    }
static inline VyneValue vmath_asin(VyneValue v)    { return vyne_float(asin(_vmath_f(v)));   }
static inline VyneValue vmath_acos(VyneValue v)    { return vyne_float(acos(_vmath_f(v)));   }
static inline VyneValue vmath_atan(VyneValue v)    { return vyne_float(atan(_vmath_f(v)));   }
static inline VyneValue vmath_sinh(VyneValue v)    { return vyne_float(sinh(_vmath_f(v)));   }
static inline VyneValue vmath_cosh(VyneValue v)    { return vyne_float(cosh(_vmath_f(v)));   }
static inline VyneValue vmath_tanh(VyneValue v)    { return vyne_float(tanh(_vmath_f(v)));   }
static inline VyneValue vmath_log(VyneValue v)     { return vyne_float(log(_vmath_f(v)));    }
static inline VyneValue vmath_log10(VyneValue v)   { return vyne_float(log10(_vmath_f(v)));  }
static inline VyneValue vmath_exp(VyneValue v)     { return vyne_float(exp(_vmath_f(v)));    }
static inline VyneValue vmath_floor(VyneValue v)   { return vyne_float(floor(_vmath_f(v)));  }
static inline VyneValue vmath_ceil(VyneValue v)    { return vyne_float(ceil(_vmath_f(v)));   }
static inline VyneValue vmath_round(VyneValue v)   { return vyne_float(round(_vmath_f(v)));  }
static inline VyneValue vmath_erf(VyneValue v)     { return vyne_float(erf(_vmath_f(v)));    }
static inline VyneValue vmath_erfc(VyneValue v)    { return vyne_float(erfc(_vmath_f(v)));   }
static inline VyneValue vmath_tgamma(VyneValue v)  { return vyne_float(tgamma(_vmath_f(v))); }
static inline VyneValue vmath_lgamma(VyneValue v)  { return vyne_float(lgamma(_vmath_f(v))); }

static inline VyneValue vmath_abs(VyneValue v) {
    double d = _vmath_f(v);
    return vyne_float(d < 0.0 ? -d : d);
}
static inline VyneValue vmath_degrees(VyneValue v) {
    return vyne_float(_vmath_f(v) * (180.0 / 3.141592653589793));
}
static inline VyneValue vmath_radians(VyneValue v) {
    return vyne_float(_vmath_f(v) * (3.141592653589793 / 180.0));
}
static inline VyneValue vmath_sigmoid(VyneValue v) {
    double x = _vmath_f(v);
    return vyne_float(1.0 / (1.0 + exp(-x)));
}
static inline VyneValue vmath_relu(VyneValue v) {
    double x = _vmath_f(v);
    return vyne_float(x > 0.0 ? x : 0.0);
}

/* ===================================================================
 * 2-argument functions
 * =================================================================== */
static inline VyneValue vmath_pow(VyneValue base, VyneValue ex) {
    return vyne_float(pow(_vmath_f(base), _vmath_f(ex)));
}
static inline VyneValue vmath_hypot(VyneValue a, VyneValue b) {
    return vyne_float(hypot(_vmath_f(a), _vmath_f(b)));
}
static inline VyneValue vmath_atan2(VyneValue y, VyneValue x) {
    return vyne_float(atan2(_vmath_f(y), _vmath_f(x)));
}
static inline VyneValue vmath_fmod(VyneValue a, VyneValue b) {
    double bv = _vmath_f(b);
    /* mirror interpreter: silently return 0 on div-by-zero rather than trap */
    return vyne_float(bv == 0.0 ? 0.0 : fmod(_vmath_f(a), bv));
}
static inline VyneValue vmath_min(VyneValue a, VyneValue b) {
    double av = _vmath_f(a), bv = _vmath_f(b);
    return vyne_float(av < bv ? av : bv);
}
static inline VyneValue vmath_max(VyneValue a, VyneValue b) {
    double av = _vmath_f(a), bv = _vmath_f(b);
    return vyne_float(av > bv ? av : bv);
}

/* ===================================================================
 * 3-argument functions
 * =================================================================== */
static inline VyneValue vmath_clamp(VyneValue val, VyneValue lo, VyneValue hi) {
    double v = _vmath_f(val);
    double l = _vmath_f(lo);
    double h = _vmath_f(hi);
    /* mirror interpreter: swap silently if lo > hi */
    if (l > h) { double tmp = l; l = h; h = tmp; }
    return vyne_float(v < l ? l : v > h ? h : v);
}

/* ===================================================================
 * PCG32 — small, fast, statistically sound.
 *
 * Replaces the Numerical Recipes LCG that had a period-4 problem in
 * its low two bits. The old generator returned the same 4-cycle for
 * any `range` that was a power of two, which silently collapsed
 * vmath.random(0, 3) onto A, U, G, C, A, U, G, C, ...
 *
 * The state is a single uint64_t. The output function applies the
 * standard PCG permutation so that low bits are as well-distributed
 * as high bits. Seeding mixes time() with the address of the state
 * variable, which is enough to decorrelate two processes started in
 * the same second.
 *
 * vmath_random and vmath_random_float deliberately share one state.
 * The old code had two separate static seeds, so interleaved calls
 * to the two functions produced correlated output.
 * =================================================================== */

static uint64_t _vmath_rng_state = 0;
static uint64_t _vmath_rng_inc   = 0;

static inline uint32_t _vmath_rng_next_u32(void) {
    if (_vmath_rng_state == 0) {
        _vmath_rng_state = (uint64_t)time(NULL)
                         ^ (uint64_t)(size_t)&_vmath_rng_state;
        _vmath_rng_inc   = 1442695040888963407ULL;   /* must be odd */
        /* Advance once so the first output isn't a pure function
         * of the wall clock. */
        _vmath_rng_state = _vmath_rng_state * 6364136223846793005ULL
                         + _vmath_rng_inc;
    }
    uint64_t old = _vmath_rng_state;
    _vmath_rng_state = old * 6364136223846793005ULL + _vmath_rng_inc;

    uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    uint32_t rot        = (uint32_t)(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

static inline VyneValue vmath_random(VyneValue mn, VyneValue mx) {
    int64_t lo = (mn.type == V_INT64) ? mn.as.i64 : (int64_t)mn.as.f64;
    int64_t hi = (mx.type == V_INT64) ? mx.as.i64 : (int64_t)mx.as.f64;
    int64_t range = hi - lo + 1;
    if (range <= 0) return vyne_int(lo);

    uint32_t r = _vmath_rng_next_u32();
    return vyne_int(lo + (int64_t)(r % (uint32_t)range));
}

static inline VyneValue vmath_random_float(VyneValue mn, VyneValue mx) {
    double lo = (mn.type == V_FLOAT64) ? mn.as.f64 : (double)mn.as.i64;
    double hi = (mx.type == V_FLOAT64) ? mx.as.f64 : (double)mx.as.i64;

    uint32_t r = _vmath_rng_next_u32();
    double unit = (double)r / 4294967296.0;    /* [0, 1) */
    return vyne_float(lo + unit * (hi - lo));
}

/* ===================================================================
 * M5: native (unboxed) variants for hot math functions.
 * Callers that already hold `double` skip the VyneValue round-trip.
 * =================================================================== */
static inline double vmath_sqrt_f64(double v)    { return sqrt(v); }
static inline double vmath_abs_f64(double v)     { return v < 0.0 ? -v : v; }
static inline double vmath_floor_f64(double v)   { return floor(v); }
static inline double vmath_ceil_f64(double v)    { return ceil(v); }
static inline double vmath_round_f64(double v)   { return round(v); }
static inline double vmath_exp_f64(double v)     { return exp(v); }
static inline double vmath_log_f64(double v)     { return log(v); }
static inline double vmath_sigmoid_f64(double v) { return 1.0 / (1.0 + exp(-v)); }
static inline double vmath_relu_f64(double v)    { return v > 0.0 ? v : 0.0; }
static inline double vmath_pow_f64(double b, double e)   { return pow(b, e); }
static inline double vmath_min_f64(double a, double b)   { return a < b ? a : b; }
static inline double vmath_max_f64(double a, double b)   { return a > b ? a : b; }
static inline double vmath_hypot_f64(double a, double b) { return hypot(a, b); }
static inline double vmath_fmod_f64(double a, double b)  { return b == 0.0 ? 0.0 : fmod(a, b); }
static inline double vmath_clamp_f64(double v, double lo, double hi) {
    if (lo > hi) { double t = lo; lo = hi; hi = t; }
    return v < lo ? lo : v > hi ? hi : v;
}

#endif /* VYNE_VMATH_RT_H */
