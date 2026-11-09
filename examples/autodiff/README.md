# Automatic differentiation examples

All three files run on the compiled backend (the default for `vynec`):

```bash
vynec examples/autodiff/scalar_grad.vy
vynec examples/autodiff/backward_region.vy
vynec examples/autodiff/autodiff_test.vy
```

| File | What it shows |
| :--- | :--- |
| `scalar_grad.vy` | Form 3 — `grad(f, x)` for a unary `@differentiable` function |
| `backward_region.vy` | Forms 1 + 2 — `@differentiable` + `@backward @bounded` with `.grad` |
| `autodiff_test.vy` | Self-checking regression; exits non-zero on mismatch |

Expected output of `autodiff_test.vy`:

```
autodiff_test: all checks passed
```

See `docs/tutorials/reference/autodiff.md` for the surface and the generated-C
shape. The supported subset is straight-line `Float64` functions; arrays, loops,
branches, `@recompute` lowering, and `@mixed` lowering are deferred and rejected
with `VNE-120`..`VNE-126` rather than silently miscompiled.
