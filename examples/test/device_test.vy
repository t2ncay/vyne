# device_test.vy - @device region lifecycle, element-wise ops, and matmul.
#
# Falls back to the CPU backend when no CUDA device is available, so the
# test is portable; the printed backend shows which path ran.
#
#   ./vynec --compile examples/test/device_test.vy
#   ./examples/test/device_test

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

out("=== @device ===");

@device<cuda> region gpu {
    out("backend: " + device_backend());
    check("backend-known",
          device_backend() == "cuda" || device_backend() == "cpu");

    # --- element-wise ---
    n :: Int64 := 8;
    a :: Array<Float32> := [];
    b :: Array<Float32> := [];
    c :: Array<Float32> := [];
    through i :: 0..n-1 -> loop {
        a.push(float32(i + 1));
        b.push(float32(2 * i + 3));
        c.push(0.0f);
    };

    da :: Int64 := device_alloc(n);
    db :: Int64 := device_alloc(n);
    dc :: Int64 := device_alloc(n);
    device_upload(da, a, n);
    device_upload(db, b, n);

    device_add(dc, da, db, n);
    device_download(dc, c, n);
    ok :: Bool := true;
    through i :: 0..n-1 -> loop {
        if !close(c[i], a[i] + b[i]) { ok = false; }
    };
    check("add", ok);

    device_mul(dc, da, db, n);
    device_download(dc, c, n);
    ok2 :: Bool := true;
    through i :: 0..n-1 -> loop {
        if !close(c[i], a[i] * b[i]) { ok2 = false; }
    };
    check("mul", ok2);

    device_scale(dc, da, 2.5, n);
    device_download(dc, c, n);
    ok3 :: Bool := true;
    through i :: 0..n-1 -> loop {
        if !close(c[i], a[i] * 2.5) { ok3 = false; }
    };
    check("scale", ok3);

    check("bytes-live", device_bytes() > 0);

    # --- matmul 32x32 ---
    m :: Int64 := 32;
    total :: Int64 := m * m;
    A :: Array<Float32> := [];
    B :: Array<Float32> := [];
    C :: Array<Float32> := [];
    ref :: Array<Float32> := [];
    through i :: 0..total-1 -> loop {
        A.push(float32((i * 5) % 11) / 11.0);
        B.push(float32((i * 3) % 7) / 7.0);
        C.push(0.0f);
        ref.push(0.0f);
    };

    through i :: 0..m-1 -> loop {
        through j :: 0..m-1 -> loop {
            s :: Float32 := 0.0f;
            through k :: 0..m-1 -> loop {
                s = s + A[i * m + k] * B[k * m + j];
            };
            ref[i * m + j] = s;
        };
    };

    ma :: Int64 := device_alloc(total);
    mb :: Int64 := device_alloc(total);
    mc :: Int64 := device_alloc(total);
    device_upload(ma, A, total);
    device_upload(mb, B, total);
    device_matmul(mc, ma, mb, m, m, m);
    device_sync();
    device_download(mc, C, total);

    max_err :: Float32 := 0.0f;
    through i :: 0..total-1 -> loop {
        d :: Float32 := C[i] - ref[i];
        if d < 0.0f { d = 0.0f - d; }
        if d > max_err { max_err = d; }
    };
    check("matmul-32", max_err < 0.0001);
    # --- Float64 opt-in (device_alloc_f64) ---
    nf :: Int64 := 4;
    af :: Array<Float64> := [];
    bf :: Array<Float64> := [];
    cf :: Array<Float64> := [];
    through i :: 0..nf-1 -> loop {
        af.push(float64(i + 1));
        bf.push(float64(i + 2));
        cf.push(0.0);
    };
    dfa :: Int64 := device_alloc_f64(nf);
    dfb :: Int64 := device_alloc_f64(nf);
    dfc :: Int64 := device_alloc_f64(nf);
    device_upload(dfa, af, nf);
    device_upload(dfb, bf, nf);
    device_add(dfc, dfa, dfb, nf);
    device_download(dfc, cf, nf);
    okf :: Bool := true;
    through i :: 0..nf-1 -> loop {
        if cf[i] != af[i] + bf[i] { okf = false; }
    };
    check("f64-add", okf);

    check("peak", device_peak_bytes() > 0);

    out("max matmul error: " + string(max_err));
};

@device<cpu> region cpu_path {
    check("backend-cpu", device_backend() == "cpu");

    n2 :: Int64 := 4;
    a2 :: Array<Float32> := [];
    b2 :: Array<Float32> := [];
    c2 :: Array<Float32> := [];
    through i :: 0..n2-1 -> loop {
        a2.push(float32(i + 1));
        b2.push(float32(i + 2));
        c2.push(0.0f);
    };
    da2 :: Int64 := device_alloc(n2);
    db2 :: Int64 := device_alloc(n2);
    dc2 :: Int64 := device_alloc(n2);
    device_upload(da2, a2, n2);
    device_upload(db2, b2, n2);
    device_add(dc2, da2, db2, n2);
    device_download(dc2, c2, n2);

    okc :: Bool := true;
    through i :: 0..n2-1 -> loop {
        if !close(c2[i], a2[i] + b2[i]) { okc = false; }
    };
    check("cpu-add", okc);
};

@device<cpu> region check_rewound {
    check("rewound", device_bytes() == 0);
};

if failures[0] > 0 {
    out("device: " + string(failures[0]) + " check(s) failed");
    exit(1);
}
out("device: all checks passed");
