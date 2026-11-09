# array_grad.vy — Array<Float64> gradients.
#
# `grad(f, w)` now returns an `Array<Float64>` when `w` is an
# `Array<Float64>` parameter. This slice lowers reduction VJPs to loops
# over the native array storage; there is no runtime tape.
#
# Note: `vstat.mse` is the `vstat.Series` API. `vstat.k_mse_sum` is the
# raw-`Array<Float64>` kernel that `mse` wraps, and it is the primitive
# this slice differentiates. For w = [1,1,1] and target = [1,2,3]:
#
#   loss = ((0)^2 + (-1)^2 + (-2)^2) / 3 = 5/3
#   dL/dw = [0, -2/3, -4/3]

use external "vstat/vstat.vy";

target :: Array<Float64> := [1.0, 2.0, 3.0];

@differentiable
fn loss(w :: Array<Float64>) -> Float64 {
    n :: Int64 := w.size();
    return vstat.k_mse_sum(w, target, n) / float64(n);
}

w :: Array<Float64> := [1.0, 1.0, 1.0];
gw :: Array<Float64> := grad(loss, w);

out("loss = " + string(loss(w)));
out(
    "grad = [" + string(gw[0]) + ", " + string(gw[1]) + ", " +
    string(gw[2]) + "]"
);
out("expect = [0.0, -0.666667, -1.33333]");
