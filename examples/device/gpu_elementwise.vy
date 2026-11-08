# gpu_elementwise.vy - @device element-wise operations.
#
#   ./vynec --compile examples/device/gpu_elementwise.vy
#   ./examples/device/gpu_elementwise
#
# The first region requests CUDA; the runtime falls back to the CPU
# backend if the driver/device is unavailable. A second device region
# checks that the first region's device arena was rewound.

failures :: Array<Int64> := [0];

fn close(a :: Float32, b :: Float32) -> Bool {
    d :: Float32 := a - b;
    if d < 0.0f { d = 0.0f - d; }
    return d < 0.0001;
}

fn check(name :: String, cond :: Bool) {
    if cond { out("PASS " + name); }
    else {
        out("FAIL " + name);
        failures[0] = failures[0] + 1;
    }
}

out("=== @device elementwise ===");

@device<cuda> region gpu {
    out("available: " + string(device_available()));
    out("backend:   " + device_backend());

    n :: Int64 := 16;
    a :: Array<Float32> := [];
    b :: Array<Float32> := [];
    c :: Array<Float32> := [];

    through i :: 0..n-1 -> loop {
        a.push(float32(i));
        b.push(float32(2 * i));
        c.push(0.0f);
    };

    da :: Int64 := device_alloc(n);
    db :: Int64 := device_alloc(n);
    dc :: Int64 := device_alloc(n);

    device_upload(da, a, n);
    device_upload(db, b, n);
    check("device-bytes-live", device_bytes() > 0);

    # add
    device_add(dc, da, db, n);
    device_download(dc, c, n);
    ok :: Bool := true;
    through i :: 0..n-1 -> loop {
        if !close(c[i], a[i] + b[i]) { ok = false; }
    };
    check("device-add", ok);

    # mul
    device_mul(dc, da, db, n);
    device_download(dc, c, n);
    ok2 :: Bool := true;
    through i :: 0..n-1 -> loop {
        if !close(c[i], a[i] * b[i]) { ok2 = false; }
    };
    check("device-mul", ok2);

    # sub
    device_sub(dc, db, da, n);
    device_download(dc, c, n);
    ok3 :: Bool := true;
    through i :: 0..n-1 -> loop {
        if !close(c[i], b[i] - a[i]) { ok3 = false; }
    };
    check("device-sub", ok3);

    # scale
    device_scale(dc, da, 3.0, n);
    device_download(dc, c, n);
    ok4 :: Bool := true;
    through i :: 0..n-1 -> loop {
        if !close(c[i], a[i] * 3.0) { ok4 = false; }
    };
    check("device-scale", ok4);

    # fill
    device_fill(dc, 7.0, n);
    device_download(dc, c, n);
    ok5 :: Bool := true;
    through i :: 0..n-1 -> loop {
        if !close(c[i], 7.0) { ok5 = false; }
    };
    check("device-fill", ok5);

    # relu
    r :: Array<Float32> := [];
    through i :: 0..n-1 -> loop { r.push(float32(i) - 8.0); };
    dr :: Int64 := device_alloc(n);
    device_upload(dr, r, n);
    device_relu(dc, dr, n);
    device_download(dc, c, n);
    ok6 :: Bool := true;
    through i :: 0..n-1 -> loop {
        expected :: Float32 := 0.0f;
        if r[i] > 0.0f { expected = r[i]; }
        if !close(c[i], expected) { ok6 = false; }
    };
    check("device-relu", ok6);

    device_sync();
    check("device-peak", device_peak_bytes() > 0);
};

# A second device frame observes the first frame's rewind.
@device<cpu> region check_rewound {
    check("device-rewound", device_bytes() == 0);
};

if failures[0] > 0 {
    out("device elementwise: " + string(failures[0]) + " check(s) failed");
    exit(1);
}
out("device elementwise: all checks passed");
