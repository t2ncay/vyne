# vconv_backward_test.vy - finite-difference checks for conv2d backward.
#
#   ./vynec --compile examples/test/vconv_backward_test.vy
#   ./examples/test/vconv_backward_test

use external "vconv/vconv.vy";

failures :: Array<Int64> := [0];

fn check(name :: String, cond :: Bool) {
    if cond { out("PASS " + name); }
    else {
        out("FAIL " + name);
        failures[0] = failures[0] + 1;
    }
}

fn close(a :: Float64, b :: Float64, eps :: Float64) -> Bool {
    d :: Float64 := a - b;
    if d < 0.0 { d = 0.0 - d; }
    return d <= eps;
}

fn dot(a :: Array<Float64>, b :: Array<Float64>, n :: Int64) -> Float64 {
    s :: Float64 := 0.0;
    through i :: 0..n-1 -> loop { s = s + a[i] * b[i]; };
    return s;
}

out("=== vconv backward ===");

# 3x3 input, 2x2 kernel, pad=1 -> 4x4 output, 1 channel.
x :: Array<Float64> := [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0];
w :: Array<Float64> := [0.5, -0.25, 0.75, 1.5];
b :: Array<Float64> := [0.2];
G :: Array<Float64> := [1.0, -2.0, 3.0, -4.0, 2.0, 0.5, -1.5, 2.5, -3.0, 1.0, 4.0, -0.5, 0.25, -0.75, 1.25, 2.0];

t :: vconv.Tensor := vconv.tensor(x, 3, 3, 1);
y :: vconv.Tensor := vconv.conv2d(t, w, b, 1, 2, 1, 1);
g :: vconv.Tensor := vconv.tensor(G, 4, 4, 1);

base :: Float64 := dot(y.data, G, 16);

gi :: Array<Float64> := vconv.conv_backward_input(g, w, 3, 3, 1, 2, 1, 1);
gw :: Array<Float64> := vconv.conv_backward_weight(t, g, 2, 1, 1);
gb :: Array<Float64> := vconv.conv_backward_bias(g);

gsum :: Float64 := 0.0;
through i :: 0..15 -> loop { gsum = gsum + G[i]; };
check("bias-gradient", close(gb[0], gsum, 1e-12));

eps :: Float64 := 1e-5;

# Input gradient vs finite differences.
in_ok :: Bool := true;
through i :: 0..8 -> loop {
    xp :: Array<Float64> := [];
    through j :: 0..8 -> loop {
        v :: Float64 := x[j];
        if j == i { v = v + eps; }
        xp.push(v);
    };
    tp :: vconv.Tensor := vconv.tensor(xp, 3, 3, 1);
    yp :: vconv.Tensor := vconv.conv2d(tp, w, b, 1, 2, 1, 1);
    lp :: Float64 := dot(yp.data, G, 16);
    num :: Float64 := (lp - base) / eps;
    if !close(gi[i], num, 1e-4) { in_ok = false; }
};
check("input-gradient", in_ok);

# Weight gradient vs finite differences.
w_ok :: Bool := true;
through i :: 0..3 -> loop {
    wp :: Array<Float64> := [];
    through j :: 0..3 -> loop {
        v :: Float64 := w[j];
        if j == i { v = v + eps; }
        wp.push(v);
    };
    yp2 :: vconv.Tensor := vconv.conv2d(t, wp, b, 1, 2, 1, 1);
    lp2 :: Float64 := dot(yp2.data, G, 16);
    num2 :: Float64 := (lp2 - base) / eps;
    if !close(gw[i], num2, 1e-4) { w_ok = false; }
};
check("weight-gradient", w_ok);

# ReLU backward: negative pre-activations pass zero.
pre :: Array<Float64> := [-1.0, 2.0, -3.0, 4.0];
up :: Array<Float64> := [5.0, 6.0, 7.0, 8.0];
rb :: Array<Float64> := vconv.relu_backward(up, pre);
check("relu-0", close(rb[0], 0.0, 1e-15));
check("relu-1", close(rb[1], 6.0, 1e-15));
check("relu-2", close(rb[2], 0.0, 1e-15));
check("relu-3", close(rb[3], 8.0, 1e-15));

if failures[0] != 0 {
    out(string(failures[0]) + " check(s) failed");
    exit(1);
}
out("=== done ===");
