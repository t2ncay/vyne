# vinterp — interpolation for Vyne

`vinterp` provides 1-D interpolation over sorted sample curves and 2-D
interpolation over row-major grids. It follows the kernel/wrapper pattern: the
hot searches and evaluations live in `Kernels.vy`, and the wrappers validate,
allocate, and expose the friendly API.

```vyne
use external "vinterp/vinterp.vy";

xs :: Array<Float64> := [0.0, 1.0, 2.0, 3.0, 4.0];
ys :: Array<Float64> := [0.0, 1.0, 4.0, 9.0, 16.0];
c :: vinterp.Curve := vinterp.Curve(xs, ys, 5);

out(vinterp.linear(c, 1.5));      # 2.5
out(vinterp.nearest(c, 1.4));     # 1.0

s :: vinterp.Spline := vinterp.spline(c);
out(vinterp.spline_eval(s, 2.0)); # 4.0 (passes through every knot)
```

## Types

| Type | Meaning |
| --- | --- |
| `Curve` | sorted `xs[]`, `ys[]`, `n` |
| `Spline` | `Curve` plus second derivatives `m[]` |
| `Grid` | row-major `data[]`, `rows`, `cols` |

Construct them directly (`vinterp.Curve(xs, ys, n)`) for zero-copy, or use the
boxed `vinterp.curve(xs, ys)` / `vinterp.grid(data, rows, cols)` constructors,
which copy and validate.

## 1-D API (`OneD.vy`)

| Function | Result |
| --- | --- |
| `nearest(c, x)` | nearest sample |
| `linear(c, x)` | linear interpolation with linear extrapolation |
| `clamped_linear(c, x)` | linear interpolation clamped to the end knots |
| `lagrange(c, x)` / `polynomial(c, x)` | Lagrange interpolation through all points |
| `spline(c)` | natural cubic spline (zero second derivative at the ends) |
| `spline_eval(s, x)` | evaluate a spline; clamps outside the knot range |
| `resample(c, xs)` | linear resample at a list of query points |
| `resample_spline(s, xs)` | spline resample at a list of query points |

`Curve.xs` must be strictly ascending. `curve(xs, ys)` checks this and throws;
the direct constructor trusts the caller.

## 2-D API (`TwoD.vy`)

The `*_at` functions take fractional grid indices (`fx` in `[0, cols-1]`,
`fy` in `[0, rows-1]`). The physical-coordinate wrappers map an axis range
onto those indices.

| Function | Result |
| --- | --- |
| `nearest_at(g, fx, fy)` | nearest grid cell |
| `bilinear_at(g, fx, fy)` | bilinear interpolation |
| `bicubic_at(g, fx, fy)` | Catmull-Rom bicubic interpolation |
| `nearest2d(g, x, y, xmin, xmax, ymin, ymax)` | nearest with physical coordinates |
| `bilinear2d(g, x, y, xmin, xmax, ymin, ymax)` | bilinear with physical coordinates |
| `bicubic2d(g, x, y, xmin, xmax, ymin, ymax)` | bicubic with physical coordinates |
| `grid_value(g, r, c)` | checked cell access |

Bicubic uses a clamped 4×4 Catmull-Rom neighborhood, so it reproduces the grid
exactly at integer indices and stays defined at the borders.

## Notes

- Searches are binary; evaluation is O(log n) after the grid/curve is built.
- `linear` extrapolates the first/last segment; `clamped_linear` and
  `spline_eval` clamp to the end values/knots.
- `lagrange` is O(n²) and requires distinct x values. Use it for a small number
  of irregular samples; use a spline for large curves.
- The spline is a natural cubic spline (`m[0] = m[n-1] = 0`). It passes through
  every knot exactly.

## Tests and benchmark

```bash
./vynec --compile examples/test/vinterp_test.vy
./examples/test/vinterp_test

./vynec --compile examples/benchmark/vinterp_bench.vy
./examples/benchmark/vinterp_bench
```
