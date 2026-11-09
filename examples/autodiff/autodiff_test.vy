# autodiff_test.vy — self-checking AD regression.
#
# Run: vynec examples/autodiff/autodiff_test.vy
# Exits 0 on success, 1 on the first mismatch.

use native vmath;

fn near(a :: Float64, b :: Float64, tol :: Float64) -> Bool {
    d :: Float64 := a - b;
    if d < 0.0 { d = -d; }
    return d <= tol;
}

@differentiable
fn quad(x :: Float64) -> Float64 {
    return x * x + 3.0 * x;
}

@differentiable
fn composite(x :: Float64) -> Float64 {
    a :: Float64 := x * x;
    b :: Float64 := a + 1.0;
    c :: Float64 := x + 2.0;
    d :: Float64 := b / c;
    return vmath.exp(d);
}

@differentiable
fn cube(x :: Float64) -> Float64 {
    return x ** 3.0;
}

@differentiable
fn two_param(x :: Float64, y :: Float64) -> Float64 {
    a :: Float64 := x * x;
    b :: Float64 := vmath.sin(y);
    return a + b;
}

# 1. grad(f, x) — unary reverse sweep.
g1 :: Float64 := grad(quad, 2.0);
if !near(g1, 7.0, 1e-9) {
    out("FAIL quad: " + string(g1));
    exit(1);
}

# 2. Chain through locals: exp((x^2+1)/(x+2)) at x=1.
g2 :: Float64 := grad(composite, 1.0);
if !near(g2, 0.8656595738020781, 1e-9) {
    out("FAIL composite: " + string(g2));
    exit(1);
}

# 3. Power rule.
g3 :: Float64 := grad(cube, 2.0);
if !near(g3, 12.0, 1e-9) {
    out("FAIL cube: " + string(g3));
    exit(1);
}

# 4. Region + backward + `.grad` on two parameters.
x :: Float64 := 3.0;
y :: Float64 := 0.0;

@backward
@bounded<64_000_000>
region step {
    l :: Float64 := two_param(x, y);
    backward(l);
    if !near(x.grad, 6.0, 1e-9) {
        out("FAIL backward dx: " + string(x.grad));
        exit(1);
    }
    if !near(y.grad, 1.0, 1e-9) {
        out("FAIL backward dy: " + string(y.grad));
        exit(1);
    }
};

out("autodiff_test: all checks passed");
