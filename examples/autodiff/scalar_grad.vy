# scalar_grad.vy — Form 3: expression-level `grad(f, x)`.
#
# A `@differentiable` function gets a compiler-generated adjoint
# (`fn_<name>_grad`). `grad(f, x)` is sugar for a seeded reverse sweep and
# desugars to the same machinery `@backward` uses.
#
# Requires the compiled backend: `vynec scalar_grad.vy` (default) or
# `vynec --compile scalar_grad.vy`.

use native vmath;

@differentiable
fn f(x :: Float64) -> Float64 {
    a :: Float64 := x * x;
    b :: Float64 := 3.0 * x;
    c :: Float64 := vmath.sin(x);
    return a + b + c;
}

# f'(x) = 2x + 3 + cos(x)
# f'(2) = 4 + 3 + cos(2) ~= 6.583853
g :: Float64 := grad(f, 2.0);

out("f(2)      = " + string(f(2.0)));
out("f'(2)     = " + string(g));
out("expected  = 6.583853");
