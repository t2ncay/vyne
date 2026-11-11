# device_fn_test.vy - @device_fn helpers lower device intrinsics inside helpers.
#
#   ./vynec --compile examples/test/device_fn_test.vy
#   ./examples/test/device_fn_test

failures :: Array<Int64> := [0];

fn check(name :: String, cond :: Bool) {
    if cond { out("PASS " + name); }
    else { out("FAIL " + name); failures[0] = failures[0] + 1; }
}

fn close(a :: Float64, b :: Float64, eps :: Float64) -> Bool {
    d :: Float64 := a - b;
    if d < 0.0 { d = 0.0 - d; }
    return d <= eps;
}

@device_fn
fn fill_then_scale(d_out :: Int64, d_tmp :: Int64, n :: Int64) {
    device_fill(d_tmp, 2.0, n);
    device_scale(d_out, d_tmp, 3.0, n);
}

@device_fn
fn helper_chain(d_out :: Int64, d_tmp :: Int64, n :: Int64) {
    fill_then_scale(d_out, d_tmp, n);
}

out("=== device_fn ===");
outv :: Array<Float32> := [0.0f, 0.0f, 0.0f, 0.0f];

@device<cuda> region r {
    d_out :: Int64 := device_alloc(4);
    d_tmp :: Int64 := device_alloc(4);
    helper_chain(d_out, d_tmp, 4);
    device_download(d_out, outv, 4);
    device_sync();
};

through i :: 0..3 -> loop {
    check("device-fn-" + string(i), close(float64(outv[i]), 6.0, 1e-6));
};

if failures[0] != 0 {
    out("device_fn: " + string(failures[0]) + " check(s) failed");
    exit(1);
}
out("=== done ===");
