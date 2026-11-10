# Automatic differentiation

Vyne ships the **surface** for automatic differentiation (AD) and a working
**source-to-source reverse-mode** implementation for straight-line `Float64`
functions and `Array<Float64>` reductions. The design composes function and
region annotations with a small set of intrinsics:

| Form | Surface | Lowers to |
| :--- | :--- | :--- |
| Function | `@differentiable fn f(x :: Float64) -> Float64 { ... }` | primal `fn_f` plus adjoint `fn_f_grad` |
| Region | `@backward region step { ... backward(l); ... }` | a tape scope inside the region; `.grad` reads |
| Region | `@implicit region solve { z := newton_solve(F, x, z0); backward(z); }` | Newton primal plus an Implicit Function Theorem adjoint |
| Expression | `grad(f, x)` | a seeded call to `fn_f_grad` |

`@backward` is a **predicate**, not a memory policy. It composes with the
existing region policies:

```vyne
@backward
@bounded<64_000_000>
@recompute<saved_peak: 8_000_000>
@mixed<compute: Float16, accumulate: Float32>
region train_step {
    l :: Float64 := loss(w, b);
    backward(l);
    # w.grad, b.grad are now populated
};
```

`@bounded` is enforced exactly as before. `@recompute` and `@mixed` are parsed
and recorded today; their lowering is a later phase, and the emitted C carries a
comment saying so rather than silently claiming the guarantee.

## Example: expression form

```vyne
use native vmath;

@differentiable
fn f(x :: Float64) -> Float64 {
    a :: Float64 := x * x;
    b :: Float64 := 3.0 * x;
    c :: Float64 := vmath.sin(x);
    return a + b + c;
}

g :: Float64 := grad(f, 2.0);   # 2*2 + 3 + cos(2) ~= 6.583853
```

## Example: region form

```vyne
@differentiable
fn loss(w :: Float64, b :: Float64) -> Float64 {
    z :: Float64 := w * 1.0 + b;
    p :: Float64 := 1.0 / (1.0 + vmath.exp(-z));
    return (p - 0.0) * (p - 0.0);
}

@backward
@bounded<64_000_000>
region train_step {
    l :: Float64 := loss(w, b);
    backward(l);
    out("dL/dw = " + string(w.grad));
    out("dL/db = " + string(b.grad));
};
```

## Example: implicit region

`@implicit` differentiates an equation instead of a solver trace. The body
calls `newton_solve(F, x..., z0)` to find a root `z` of `F(x..., z) = 0`, and
`backward(z)` applies the Implicit Function Theorem at the converged root:

```
dz/dx_i = -(dF/dx_i) / (dF/dz)
```

```vyne
use native vmath;

# F(x, z) = z^3 + x - 1 = 0.
@differentiable
fn cubic_residual(x :: Float64, z :: Float64) -> Float64 {
    return z * z * z + x - 1.0;
}

x :: Float64 := 0.0;

@implicit
@bounded<64_000_000>
region solve_cubic {
    z :: Float64 := newton_solve(cubic_residual, x, 0.5);

    backward(z);                    # dz/dx = -1/3 at z = 1
    out("dz/dx = " + string(x.grad));
};
```

The residual is a scalar `@differentiable` function whose last parameter is
the state. `@implicit` implies the `@backward` tape scope and composes with the
memory policies. `backward(root, seed)` chains an explicit upstream
sensitivity. Newton uses a `1e-12` tolerance, fails after the iteration ceiling
(64 by default, or `@implicit<N>`), and rejects a singular `dF/dz` with
`VNE-128`. Custom `<fn>_vjp` adjoints cannot supply `dF/dz` and are rejected.
Vector residuals and the AVR target are deferred; `newton_solve` and
`implicit_solve` resolve as ordinary functions outside an `@implicit` region.

## Example: array gradients

`grad(f, w)` returns an `Array<Float64>` of the same length when `w` is an
`Array<Float64>` parameter. The adjoint lowers each reduction VJP to a loop
over the native array storage.

```vyne
use external "vstat/vstat.vy";

target :: Array<Float64> := [1.0, 2.0, 3.0];

@differentiable
fn loss(w :: Array<Float64>) -> Float64 {
    n :: Int64 := w.size();
    return vstat.k_mse_sum(w, target, n) / float64(n);
}

w :: Array<Float64> := [1.0, 1.0, 1.0];
gw :: Array<Float64> := grad(loss, w);
# gw = [0.0, -0.666667, -1.33333]
```

`vstat.mse` is the `vstat.Series` API; `vstat.k_mse_sum` is the raw
`Array<Float64>` kernel that `mse` wraps, and it is the primitive this slice
differentiates.

Recognized array reductions and their VJPs (`d` is the incoming scalar
gradient, `n` is the array length):

| Primitive | Scalar result | VJP |
| :--- | :--- | :--- |
| `mse(a,b)` / `k_mse_sum(a,b,n)` | mean / sum of squared error | `d_a += 2(a-b)/n * d`, `d_b -= ...` (the `k_` form omits `1/n`) |
| `mae(a,b)` / `k_mae_sum(a,b,n)` | mean / sum absolute error | `d_a += sign(a-b)*d/n`, `d_b -= ...` |
| `dot(a,b)` / `k_dot(a,b,n)` | dot product | `d_a += b*d`, `d_b += a*d` |
| `sum(a,n)` / `mean(a,n)` | sum / mean | `d_a += d` / `d_a += d/n` |
| `norm_sq(a,n)` / `sum_sq(a,n)` | squared norm / sum of squares | `d_a += 2a*d` |
| `abs_sum(a,n)` | sum of absolute values | `d_a += sign(a)*d` |

Array arguments must be plain variables (parameters or read-only globals). The
gradient is returned in a fresh `VyneArray_f64`; when the call sits inside a
region, that container is region-scoped like every other value.

## Custom adjoints (`<fn>_vjp`)

A function named `<fn>_vjp` registers itself as the reverse pass for `<fn>`.
This is the escape hatch for functions outside the scalar/array-reduction
subset - for example a conv-net loss that carries `Tensor` locals and calls
`vconv.conv2d`.

The custom adjoint receives the original call's arguments as boxed values and
owns its gradient storage. It runs when `backward(loss)` consumes the recorded
`@differentiable` call:

```vyne
g :: Array<Float64> := [0.0];

fn square_vjp(x :: Float64) {
    g[0] = 2.0 * x;
}

@differentiable
fn square(x :: Float64) -> Float64 {
    return x * x;
}

@backward region step {
    loss :: Float64 := square(3.0);
    backward(loss);
};
# g[0] == 6.0
```

Rules:

- The name must be exactly `<primal>_vjp` (module-qualified names are supported).
- The adjoint's parameters must match the primal's parameter list.
- There is no `d_out` parameter: a scalar `backward(loss)` seeds with `1.0`.
- The compiler does not validate the primal body when a custom adjoint exists;
  the custom function is responsible for the reverse computation.
- Gradient storage lives wherever the adjoint puts it (globals, a `Grads`
  object, caller-provided state). `w.grad` scalar slots are not extended to
  array/Tensor parameters yet.

The `srcnn` project uses this convention: `sr_loss` is `@differentiable` and
`sr_loss_vjp` fills a global `Grads` using the explicit `vconv` backward
kernels. The training loop then writes `backward(loss)` and reads
`last_grads()`; the explicit backprop loop is gone.
## What the compiler emits

For

```vyne
@differentiable
fn f(x :: Float64, y :: Float64) -> Float64 {
    a :: Float64 := x * x;
    b :: Float64 := vmath.sin(y);
    return a + b;
}
```

the C backend emits the primal unchanged and a companion:

```c
void fn_f_grad(double x, double y, double d_out, double* d_x, double* d_y) {
    double a = x * x;
    double b = sin(y);

    double d_x = 0.0, d_y = 0.0, d_a = 0.0, d_b = 0.0;
    d_a += 1.0 * d_out;
    d_b += 1.0 * d_out;
    d_y += cos(y) * d_b;
    d_x += (1.0 * x + x * 1.0) * d_a;

    *d_x = d_x;
    *d_y = d_y;
}
```

The transformation is mechanical:

1. Recompute the primal intermediates. There is no runtime tape interpreter and
   no per-operation dispatch.
2. Seed every variable referenced by the returned expression with
   `partial(expr, var) * d_out`.
3. Walk the assignments in reverse, accumulating
   `d_u += partial(rhs, u) * d_v`.
4. Publish the input gradients through the out-parameters.

The **tape is a set of ordinary C locals inside the caller's region block**. It
is reclaimed at the region's closing brace, no heap allocation is involved, and
`@bounded` accounts for it like any other region memory.

## Supported subset (Phase B)

- Parameters are `Float64` scalars or `Array<Float64>`. The return type is
  `Float64`.
- The body is a sequence of fresh scalar declarations (`:=`) followed by one
  `return <expr>;`.
- Scalar expressions use `+`, `-`, `*`, `/`, `**`, and the math calls
  `sin`, `cos`, `exp`, `log`, `sqrt`, `tanh`, `abs`, `relu`, and `pow(a, b)`.
  Both `vmath.sin(x)` (method form) and a same-named plain call are accepted.
- Array values participate through the reduction primitives listed above.
  Array locals and element-wise array arithmetic are deferred.
- Scalar bodies must be closed over their parameters: reading a scalar global
  inside a scalar `@differentiable` body is rejected because the global is not
  part of the adjoint signature. Array reductions may read read-only array
  globals (`target` above), which are constants with respect to the adjoint.

Everything else is rejected with a stable diagnostic instead of producing a
wrong gradient:

| Code | Meaning |
| :--- | :--- |
| `VNE-120` | function outside the supported differentiable subset |
| `VNE-121` | unsupported statement / mutation inside `@differentiable` |
| `VNE-122`–`VNE-125` | unsupported expression or `grad()` misuse |
| `VNE-126` | `backward()` could not resolve the recorded differentiable call |
| `VNE-127` | malformed `newton_solve()` call or residual outside the implicit scalar subset |
| `VNE-128` | runtime implicit solve failure (non-convergence or singular `dF/dz`) |

## Deferred

The following are deliberately not claimed yet:

- array locals, element-wise array intermediates, and matrix/matmul VJPs;
- `backward(...)` on array parameters (use `grad(f, x)`);
- branches, `through`/`while` loops, and loop-carried state;
- vector-valued implicit roots, dense Jacobian assembly, and linear solves;
- the AVR target for `@implicit`;
- `@recompute` checkpointing and `@mixed` precision policies;
- interpreter execution of `grad` / `backward` (use the compiled backend);
- cross-module adjoint discovery beyond the defining translation unit.

See `examples/autodiff/` for runnable coverage.
