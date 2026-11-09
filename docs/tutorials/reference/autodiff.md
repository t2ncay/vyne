# Automatic differentiation

Vyne ships the **surface** for automatic differentiation (AD) and a working
**source-to-source reverse-mode** implementation for straight-line `Float64`
functions. The design composes two annotations and one intrinsic:

| Form | Surface | Lowers to |
| :--- | :--- | :--- |
| Function | `@differentiable fn f(x :: Float64) -> Float64 { ... }` | primal `fn_f` plus adjoint `fn_f_grad` |
| Region | `@backward region step { ... backward(l); ... }` | a tape scope inside the region; `.grad` reads |
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

- Parameters and return type are `Float64` scalars.
- The body is a sequence of fresh `Float64` declarations (`:=`) followed by one
  `return <expr>;`.
- Expressions use `+`, `-`, `*`, `/`, `**`, and the math calls
  `sin`, `cos`, `exp`, `log`, `sqrt`, `tanh`, `abs`, `relu`, and `pow(a, b)`.
  Both `vmath.sin(x)` (method form) and a same-named plain call are accepted.
- Bodies must be closed over their parameters: reading a global inside a
  `@differentiable` body is rejected, because the global is not part of the
  adjoint signature.

Everything else is rejected with a stable diagnostic instead of producing a
wrong gradient:

| Code | Meaning |
| :--- | :--- |
| `VNE-120` | function outside the supported differentiable subset |
| `VNE-121` | unsupported statement / mutation inside `@differentiable` |
| `VNE-122`–`VNE-125` | unsupported expression or `grad()` misuse |
| `VNE-126` | `backward()` could not resolve the recorded differentiable call |

## Deferred

The following are deliberately not claimed yet:

- arrays, tensors, and vector-Jacobian products;
- branches, `through`/`while` loops, and loop-carried state;
- `@recompute` checkpointing and `@mixed` precision policies;
- interpreter execution of `grad` / `backward` (use the compiled backend);
- cross-module adjoint discovery beyond the defining translation unit.

See `examples/autodiff/` for runnable coverage.
