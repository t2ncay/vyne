# Automatic differentiation examples

All files run on the compiled backend (the default for `vynec`):

```bash
vynec examples/autodiff/scalar_grad.vy
vynec examples/autodiff/backward_region.vy
vynec examples/autodiff/autodiff_test.vy
vynec examples/autodiff/implicit_test.vy
```

| File | What it shows |
| :--- | :--- |
| `scalar_grad.vy` | Form 3 - `grad(f, x)` for a unary `@differentiable` function |
| `backward_region.vy` | Forms 1 + 2 - `@differentiable` + `@backward @bounded` with `.grad` |
| `array_grad.vy` | `grad(f, w)` for `w :: Array<Float64>`, lowered to reduction VJP loops |
| `implicit_test.vy` | `@implicit` + `newton_solve` + Implicit Function Theorem backward, including a seeded two-parameter adjoint |
| `autodiff_test.vy` | Self-checking regression (scalar, region, array, and implicit); exits non-zero on mismatch |

Expected output of `autodiff_test.vy`:

```
autodiff_test: all checks passed
```

Expected output of `implicit_test.vy`:

```
implicit_test: all checks passed
```

`implicit_test.vy` differentiates a root `z` of `F(x..., z) = 0` with
`dz/dx_i = -(dF/dx_i)/(dF/dz)` at the converged root. It never stores or
replays Newton iterations, so the reverse pass costs one residual adjoint per
`backward(root)` call. The residual must be a scalar `@differentiable`
function whose last parameter is the state; `newton_solve` is intercepted only
inside an `@implicit` region.

`array_grad.vy` uses `vstat.k_mse_sum`, the raw-`Array<Float64>` kernel that
`vstat.mse` (the `Series` API) wraps. See
`docs/tutorials/reference/autodiff.md` for the surface, the generated-C shape,
and the array reduction VJP table. Array gradients currently cover reductions
(`mse`, `mae`, `dot`, `sum`, `mean`, `norm_sq`, `abs_sum`); array locals, loops,
branches, `@recompute` lowering, and `@mixed` lowering are deferred and rejected
with `VNE-120`..`VNE-128` rather than silently miscompiled.
