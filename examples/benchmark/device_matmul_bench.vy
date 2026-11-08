# device_matmul_bench.vy - GPU vs CPU 512x512 Float64 matmul.
#
#   ./vynec --compile examples/benchmark/device_matmul_bench.vy
#   ./examples/benchmark/device_matmul_bench

use native vcore;

out("=== @device matmul benchmark ===");

@device<cuda> region gpu {
    out("backend: " + device_backend());

    n :: Int64 := 512;
    total :: Int64 := n * n;

    A :: Array<Float64> := [];
    B :: Array<Float64> := [];
    C :: Array<Float64> := [];
    ref :: Array<Float64> := [];

    through i :: 0..total-1 -> loop {
        A.push(float64((i * 7) % 13) / 13.0);
        B.push(float64((i * 11) % 17) / 17.0);
        C.push(0.0);
        ref.push(0.0);
    };

    da :: Int64 := device_alloc(total);
    db :: Int64 := device_alloc(total);
    dc :: Int64 := device_alloc(total);
    device_upload(da, A, total);
    device_upload(db, B, total);

    # Warm up the module/kernel path.
    device_matmul(dc, da, db, n, n, n);
    device_sync();

    t0 :: Int64 := vcore.now_ns();
    through r :: 0..2 -> loop {
        device_matmul(dc, da, db, n, n, n);
        device_sync();
        device_download(dc, C, total);
    };
    t1 :: Int64 := vcore.now_ns();

    t2 :: Int64 := vcore.now_ns();
    through i :: 0..n-1 -> loop {
        through j :: 0..n-1 -> loop {
            s :: Float64 := 0.0;
            through k :: 0..n-1 -> loop {
                s = s + A[i * n + k] * B[k * n + j];
            };
            ref[i * n + j] = s;
        };
    };
    t3 :: Int64 := vcore.now_ns();

    max_err :: Float64 := 0.0;
    through i :: 0..total-1 -> loop {
        d :: Float64 := C[i] - ref[i];
        if d < 0.0 { d = 0.0 - d; }
        if d > max_err { max_err = d; }
    };

    gpu_ms :: Int64 := (t1 - t0) / 3000000;
    cpu_ms :: Int64 := (t3 - t2) / 1000000;
    out("gpu (3 runs, incl. download): " + string(gpu_ms) + " ms/run");
    out("cpu reference               : " + string(cpu_ms) + " ms/run");
    out("max error                   : " + string(max_err));
    out("device peak                 : " + string(device_peak_bytes()) + " bytes");
};
