---
id: autodiff
title: Automatic Differentiation
group: Language Reference
icon: fa-infinity
order: 10
description: Compiler-generated adjoints with @differentiable, @backward, grad, and custom VJPs.
---

# Automatic Differentiation

Vyne ships a source-to-source reverse-mode AD surface with a working implementation for straight-line `Float64` functions and `Array<Float64>` reductions. There is no runtime tape interpreter and no per-operation dispatch: the compiler emits a primal function and a companion adjoint.

| Form | Surface | Lowers to |
| :--- | :--- | :--- |
| Function | `@differentiable fn f(x :: Float64) -> Float64 { ... }` | primal `fn_f` plus `fn_f_grad` |
| Region | `@backward region step { ... backward(l); ... }` | a tape scope inside the region; `.grad` reads |
| Expression | `grad(f, x)` | a seeded call to `fn_f_grad` |

`@backward` is a predicate, not a memory policy. It composes with `@bounded`, `@recompute`, and `@mixed`:

```vyne
@backward
@bounded<64_000_000>
region train_step {
    l :: Float64 := loss(w, b);
    backward(l);
    out("dL/dw = " + string(w.grad));
};
```

## Expression form

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

`grad` is compiled-backend functionality. The AST interpreter does not evaluate `grad` / `backward`.

## Region form

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

The tape is a set of ordinary C locals inside the region block. It dies at the region rewind, and `@bounded` accounts for it like any other region memory.

## Array gradients

`grad(f, w)` returns a fresh `Array<Float64>` when `w` is an `Array<Float64>` parameter. The adjoint lowers each reduction VJP to a loop over the native array storage.

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

Recognized reductions and their VJPs (`d` is the incoming scalar gradient, `n` the array length):

| Primitive | Result | VJP |
| :--- | :--- | :--- |
| `mse(a,b)` / `k_mse_sum(a,b,n)` | mean / sum of squared error | `d_a += 2(a-b)/n * d`, `d_b -= ...` |
| `mae(a,b)` / `k_mae_sum(a,b,n)` | mean / sum absolute error | `d_a += sign(a-b)*d/n`, `d_b -= ...` |
| `dot(a,b)` / `k_dot(a,b,n)` | dot product | `d_a += b*d`, `d_b += a*d` |
| `sum(a,n)` / `mean(a,n)` | sum / mean | `d_a += d` / `d_a += d/n` |
| `norm_sq(a,n)` / `sum_sq(a,n)` | squared norm / sum of squares | `d_a += 2a*d` |
| `abs_sum(a,n)` | sum of absolute values | `d_a += sign(a)*d` |

Array arguments must be plain variables (parameters or read-only globals). Array locals and element-wise array intermediates are deferred. `backward(...)` on array parameters is deferred; use `grad(f, x)`.

## Custom adjoints

A function named `<fn>_vjp` registers itself as the reverse pass for `<fn>`. This is the escape hatch for functions outside the scalar/reduction subset, such as a convolution loss that carries tensor locals.

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

- The name must be exactly `<primal>_vjp`; module-qualified names are supported.
- The adjoint's parameters must match the primal's parameter list.
- There is no `d_out` parameter: `backward(loss)` seeds with `1.0`.
- The compiler does not validate the primal body when a custom adjoint exists.
- Gradient storage lives wherever the adjoint puts it (globals, a `Grads` object, caller state). `w.grad` scalar slots are not extended to array/tensor parameters.

The [SRCNN showcase](page:srcnn) uses this convention: `sr_loss` is `@differentiable`, and `sr_loss_vjp` fills a global `Grads` using the explicit `vconv` backward kernels.

## What the compiler emits

For:

```vyne
@differentiable
fn f(x :: Float64, y :: Float64) -> Float64 {
    a :: Float64 := x * x;
    b :: Float64 := vmath.sin(y);
    return a + b;
}
```

the backend emits the primal unchanged and a companion of the form:

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

The transformation recomputes primal intermediates, seeds the returned expression, walks assignments in reverse, and publishes input gradients through out-parameters.

## Supported subset

- Parameters are `Float64` scalars or `Array<Float64>`. The return type is `Float64`.
- The body is a sequence of fresh scalar declarations (`:=`) followed by one `return <expr>;`.
- Scalar expressions use `+ - * / **` and `sin cos exp log sqrt tanh abs relu pow`.
- Array values participate through the reduction primitives above.
- Scalar bodies must close over their parameters. Reading a scalar global inside a scalar `@differentiable` body is rejected because it is not part of the adjoint signature. Array reductions may read read-only array globals.

Everything else is rejected with a stable diagnostic instead of producing a wrong gradient:

| Code | Meaning |
| :--- | :--- |
| `VNE-120` | function outside the supported differentiable subset |
| `VNE-121` | unsupported statement or mutation inside `@differentiable` |
| `VNE-122`–`VNE-125` | unsupported expression or `grad()` misuse |
| `VNE-126` | `backward()` could not resolve the recorded differentiable call |

## Deferred

- Array locals, element-wise array intermediates, matrix/matmul VJPs;
- `backward(...)` on array parameters (use `grad(f, x)`);
- branches, `through` / `while` loops, loop-carried state;
- `@recompute` checkpointing and `@mixed` precision lowering;
- interpreter execution of `grad` / `backward`;
- cross-module adjoint discovery beyond the defining translation unit.

## See also

- [Memory & Regions](page:regions)
- [Functions](page:fn)
- [SRCNN Showcase](page:srcnn)
- [voptim](page:voptim)
