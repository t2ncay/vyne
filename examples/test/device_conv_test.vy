# device_conv_test.vy - device_conv2d against the CPU vconv reference.
#
#   ./vynec --compile examples/test/device_conv_test.vy
#   ./examples/test/device_conv_test
#
# The CUDA backend is used when available; otherwise @device<cuda> falls
# back to the CPU backend and the same comparison still runs.

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

out("=== device conv2d ===");

# 3x3x1 input, 3x3x1 kernel, pad 1 -> 3x3x1.
x :: Array<Float64> := [1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0];
w :: Array<Float64> := [0.5, 0.0, -0.5, 0.25, 0.0, -0.25, 0.1, 0.0, -0.1];
b :: Array<Float64> := [0.2];

xt :: vconv.Tensor := vconv.tensor(x, 3, 3, 1);
yt :: vconv.Tensor := vconv.conv2d(xt, w, b, 1, 3, 1, 1);

xf :: Array<Float32> := [];
through i :: 0..8 -> loop { xf.push(float32(x[i])); };
wf :: Array<Float32> := [];
through i2 :: 0..8 -> loop { wf.push(float32(w[i2])); };
bf :: Array<Float32> := [0.2f];
of :: Array<Float32> := [];
through i3 :: 0..8 -> loop { of.push(0.0f); };

@device<cuda> region conv {
    d_x :: Int64 := device_alloc(9);
    d_w :: Int64 := device_alloc(9);
    d_b :: Int64 := device_alloc(1);
    d_o :: Int64 := device_alloc(9);
    device_upload(d_x, xf, 9);
    device_upload(d_w, wf, 9);
    device_upload(d_b, bf, 1);
    device_conv2d(d_o, d_x, d_w, d_b, 3, 3, 1, 3, 3, 1, 3, 1, 1, 1);
    device_download(d_o, of, 9);
    device_sync();
};

through i4 :: 0..8 -> loop {
    check("conv-" + string(i4), close(yt.data[i4], float64(of[i4]), 1e-6));
};

if failures[0] != 0 {
    out(string(failures[0]) + " check(s) failed");
    exit(1);
}
out("=== done ===");
