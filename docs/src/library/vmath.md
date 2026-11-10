---
id: vmath
title: VMath
group: Standard Library
icon: fa-calculator
order: 3
description: Scalar math, rounding, activations, clamping, PCG32 random, and constants.
---

# VMath

`vmath` is the scalar math module. Import it with `use native vmath;`. Trigonometry uses radians.

## Constants

| Property | Value |
| :--- | :--- |
| `vmath.pi` | 3.141592653589793 |
| `vmath.e` | 2.718281828459045 |
| `vmath.tau` | 6.283185307179586 |
| `vmath.phi` | 1.618033988749895 |
| `vmath.sqrt2` | 1.4142135623730951 |
| `vmath.inf` | positive infinity |
| `vmath.nan` | NaN |

## One-argument functions

`sqrt`, `abs`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `sinh`, `cosh`, `tanh`, `log`, `log10`, `exp`, `floor`, `ceil`, `round`, `erf`, `erfc`, `tgamma`, `lgamma`, `sigmoid`, `relu`, `degrees`, `radians`.

```vyne
use native vmath;

out(vmath.sqrt(9.0));       # 3
out(vmath.sin(vmath.pi / 2.0));
out(vmath.exp(0.0));        # 1
out(vmath.sigmoid(0.0));    # 0.5
out(vmath.relu(-3.0));      # 0
out(vmath.degrees(vmath.pi));  # 180
```

## Two- and three-argument functions

| Function | Result |
| :--- | :--- |
| `vmath.pow(base, exp)` | `base ** exp` |
| `vmath.hypot(a, b)` | `sqrt(a² + b²)` |
| `vmath.atan2(y, x)` | quadrant-aware arctangent |
| `vmath.fmod(a, b)` | floating remainder; `b == 0` yields `0` |
| `vmath.min(a, b)` / `vmath.max(a, b)` | numeric min/max |
| `vmath.clamp(v, lo, hi)` | clamp; silently swaps `lo`/`hi` if reversed |

## Random

`vmath` exposes one process-global PCG32 stream shared by `random` and `random_float`.

| Function | Result |
| :--- | :--- |
| `vmath.random(lo, hi)` | `Int64` in `[lo, hi]` |
| `vmath.random_float(lo, hi)` | `Float64` in `[lo, hi)` |
| `vmath.seed(n)` | Reset the stream state |

```vyne
vmath.seed(42);
dice := vmath.random(1, 6);
x := vmath.random_float(-1.0, 1.0);
```

The state is process-global. Two independent streams require explicit state that has not landed yet. For reproducible simulations, call `vmath.seed` once at startup. On AVR, `arduino.random` / `arduino.random_seed` are separate bindings; the TinyML demo implements its own LCG in Vyne source for cross-boot reproducibility.

## Unboxed variants

Every hot scalar function has a `_f64` variant used by the emitter when every argument is provably `Float64`. The source spelling does not change: `vmath.sqrt(x)` lowers to `vmath_sqrt` or `vmath_sqrt_f64` depending on the inferred type. Library kernels are written to keep the unboxed path.

## AVR surface

The same names are available through `arduino.*` on the Uno target, with `_f64` variants for the provable-native path. AVR `double` is 32-bit, so expect single-precision behavior.

## See also

- [Values & Types](page:types)
- [vrand](page:vrand)
- [Arduino & AVR](page:arduino)
