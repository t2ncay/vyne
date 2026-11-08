# gpu_matmul.vy - @device 128x128 Float32 matmul against a CPU reference.
#
#   ./vynec --compile examples/device/gpu_matmul.vy
#   ./examples/device/gpu_matmul

failures :: Array<Int64> := [0];

fn check(name :: String, cond :: Bool) {
    if cond { out("PASS " + name); }
    else {
        out("FAIL " + name);
        failures[0] = failures[0] + 1;
    }
}

out("=== @device matmul ===");

@device<cuda> region gpu {
    out("backend: " + device_backend());

    n :: Int64 := 128;
    total :: Int64 := n * n;

    A :: Array<Float32> := [];
    B :: Array<Float32> := [];
    C :: Array<Float32> := [];
    ref :: Array<Float32> := [];

    through i :: 0..total-1 -> loop {
        A.push(float32((i * 7) % 13) / 13.0);
        B.push(float32((i * 11) % 17) / 17.0);
        C.push(0.0f);
        ref.push(0.0f);
    };

    # CPU reference.
    through i :: 0..n-1 -> loop {
        through j :: 0..n-1 -> loop {
            s :: Float32 := 0.0f;
            through k :: 0..n-1 -> loop {
                s = s + A[i * n + k] * B[k * n + j];
            };
            ref[i * n + j] = s;
        };
    };

    da :: Int64 := device_alloc(total);
    db :: Int64 := device_alloc(total);
    dc :: Int64 := device_alloc(total);

    device_upload(da, A, total);
    device_upload(db, B, total);
    device_matmul(dc, da, db, n, n, n);
    device_sync();
    device_download(dc, C, total);

    max_err :: Float32 := 0.0f;
    through i :: 0..total-1 -> loop {
        d :: Float32 := C[i] - ref[i];
        if d < 0.0f { d = 0.0f - d; }
        if d > max_err { max_err = d; }
    };
    out("max error: " + string(max_err));
    check("matmul-128", max_err < 0.0001);
    check("device-peak", device_peak_bytes() > 0);
};

@device<cpu> region check_rewound {
    check("device-rewound", device_bytes() == 0);
};

if failures[0] > 0 {
    out("device matmul: " + string(failures[0]) + " check(s) failed");
    exit(1);
}
out("device matmul: all checks passed");
