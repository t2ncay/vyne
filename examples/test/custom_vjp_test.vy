# custom_vjp_test.vy - the `<fn>_vjp` convention.
#
#   ./vynec --compile examples/test/custom_vjp_test.vy
#   ./examples/test/custom_vjp_test

g :: Array<Float64> := [0.0];

fn square_vjp(x :: Float64) {
    g[0] = 2.0 * x;
}

@differentiable
fn square(x :: Float64) -> Float64 {
    return x * x;
}

out("=== custom vjp ===");
@backward region step {
    loss :: Float64 := square(3.0);
    backward(loss);
};
out("grad=" + string(g[0]));
if g[0] != 6.0 {
    out("FAIL");
    exit(1);
}
out("PASS");
