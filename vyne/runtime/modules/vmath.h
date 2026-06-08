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
 * random(min, max) — LCG seeded once from time(), int64 output
 * =================================================================== */
static inline VyneValue vmath_random(VyneValue mn, VyneValue mx) {
    static unsigned int _seed = 0;
    if (_seed == 0) _seed = (unsigned int)(size_t)time(NULL);
    _seed = _seed * 1664525u + 1013904223u;

    int64_t lo = (mn.type == V_INT64) ? mn.as.i64 : (int64_t)mn.as.f64;
    int64_t hi = (mx.type == V_INT64) ? mx.as.i64 : (int64_t)mx.as.f64;
    int64_t range = hi - lo + 1;
    return vyne_int(range > 0 ? lo + (int64_t)(_seed % (unsigned int)range) : lo);
}

#endif /* VYNE_VMATH_RT_H */
