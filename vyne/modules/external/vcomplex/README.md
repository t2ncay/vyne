# vcomplex — complex numbers for Vyne

`vcomplex` provides scalar complex arithmetic, elementary functions, polar
helpers, and element-wise operations over split `(re, im)` arrays. It follows the
same kernel/wrapper structure as the other external libraries.

```vyne
use external "vcomplex/vcomplex.vy";

i :: vcomplex.Complex := vcomplex.Complex(0.0, 1.0);
z :: vcomplex.Complex := vcomplex.mul(i, i);
out(vcomplex.to_string(z));        # -1+0i
out(vcomplex.abs(vcomplex.Complex(3.0, 4.0)));  # 5.0
```

## Types

| Type | Meaning |
| --- | --- |
| `Complex` | `{ re, im }` scalar |
| `ComplexArray` | `{ re[], im[], n }` split representation |
| `Polar` | `{ r, theta }` |

## Scalar operations (`Ops.vy`)

`make`, `add`, `sub`, `mul`, `div`, `scale`, `neg`, `conj`, `abs`, `abs_sq`,
`arg`, `equals(a, b, eps)`, `to_string`.

`div` throws on a zero denominator. `abs` is `hypot(re, im)`; `arg` is
`atan2(im, re)`.

## Elementary functions (`Functions.vy`)

| Function | Notes |
| --- | --- |
| `exp(z)` | `e^re (cos im + i sin im)` |
| `log(z)` | principal branch; `log(0)` throws |
| `sqrt(z)` | principal branch via polar half-angle |
| `pow(z, w)` | `exp(w log z)` |
| `sin(z)` | `sin re cosh im + i cos re sinh im` |
| `cos(z)` | `cos re cosh im − i sin re sinh im` |
| `tan(z)` | `sin(z) / cos(z)` |

## Polar helpers (`Polar.vy`)

`polar(r, theta)`, `from_polar(r, theta)`, `to_polar(z)`, `rotate(z, theta)`,
`scale_polar(p, factor)`.

## Split arrays (`Arrays.vy`)

| Function | Result |
| --- | --- |
| `zeros(n)` | `ComplexArray` of zeroes |
| `from_parts(re, im, n)` | copy boxed arrays; construct `ComplexArray(re, im, n)` directly for zero-copy |
| `copy(a)` | deep copy |
| `a_add(a, b)` / `a_sub` | element-wise sum/difference |
| `a_mul(a, b)` | element-wise product |
| `a_scale(a, s)` | scalar multiply |
| `a_conj(a)` | conjugate |
| `a_abs(a)` / `a_abs_sq(a)` / `a_arg(a)` | real-valued outputs |
| `a_sum(a)` / `a_dot(a, b)` | complex reductions |
| `a_from_polar(rs, thetas, n)` | polar to split complex |

The element-wise kernels are leaves over typed arrays; wrappers allocate the
output and make one native call.

## Notes

- Functions use the principal branch (`arg` in `(−π, π]`).
- `equals` is tolerance-based; exact `==` on floats is rarely what you want.
- `ComplexArray` uses two `Array<Float64>` buffers, which matches the FFT and
  signal-processing data flow in this repository.

## Tests and benchmark

```bash
./vynec --compile examples/test/vcomplex_test.vy
./examples/test/vcomplex_test

./vynec --compile examples/benchmark/vcomplex_bench.vy
./examples/benchmark/vcomplex_bench
```
