# device_matmul_bench.vy - GPU vs CPU 512x512 Float32 matmul.
#
# The CPU row here is a naive Vyne triple loop, not BLAS. For the fair
# comparison, see docs/device.md: on an i7-14700, OpenBLAS sgemm is
# ~626 us at 512x512 and ~2615 us at 1024x1024. The default Float32 GPU
# is ~1.9x / ~1.4x faster end-to-end (and ~3.8x / ~1.9x kernel-only).
# Float64 remains opt-in via device_alloc_f64.
#
#   ./vynec --compile examples/benchmark/device_matmul_bench.vy
#   ./examples/benchmark/device_matmul_bench

use native vcore;

out("=== @device matmul benchmark ===");

@device<cuda> region gpu {
    out("backend: " + device_backend());

    n :: Int64 := 512;
    total :: Int64 := n * n;
    iters :: Int64 := 20;

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

    da :: Int64 := device_alloc(total);
    db :: Int64 := device_alloc(total);
    dc :: Int64 := device_alloc(total);
    device_upload(da, A, total);
    device_upload(db, B, total);

    # Warm up the module/kernel path.
    device_matmul(dc, da, db, n, n, n);
    device_sync();
    device_download(dc, C, total);

    kt0 :: Int64 := vcore.now_ns();
    through r :: 0..iters-1 -> loop {
        device_matmul(dc, da, db, n, n, n);
        device_sync();
    };
    kt1 :: Int64 := vcore.now_ns();

    dt0 :: Int64 := vcore.now_ns();
    through r :: 0..iters-1 -> loop {
        device_download(dc, C, total);
    };
    dt1 :: Int64 := vcore.now_ns();

    ct0 :: Int64 := vcore.now_ns();
    through i :: 0..n-1 -> loop {
        through j :: 0..n-1 -> loop {
            s :: Float32 := 0.0f;
            through k :: 0..n-1 -> loop {
                s = s + A[i * n + k] * B[k * n + j];
            };
            ref[i * n + j] = s;
        };
    };
    ct1 :: Int64 := vcore.now_ns();

    max_err :: Float32 := 0.0f;
    through i :: 0..total-1 -> loop {
        d :: Float32 := C[i] - ref[i];
        if d < 0.0f { d = 0.0f - d; }
        if d > max_err { max_err = d; }
    };

    kernel_us :: Int64 := (kt1 - kt0) / (iters * 1000);
    download_us :: Int64 := (dt1 - dt0) / (iters * 1000);
    cpu_ns :: Int64 := ct1 - ct0;
    cpu_us :: Int64 := cpu_ns / 1000;

    flops :: Float64 := 2.0 * float64(n) * float64(n) * float64(n);
    gpu_gflops :: Float64 := flops / (float64(kernel_us) * 1000.0);
    cpu_gflops :: Float64 := flops / float64(cpu_ns);

    out("gpu kernel      : " + string(kernel_us) + " us (" +
        string(gpu_gflops) + " GFLOP/s)");
    out("gpu download    : " + string(download_us) + " us");
    out("gpu end-to-end  : " + string(kernel_us + download_us) + " us");
    out("naive cpu loop  : " + string(cpu_us) + " us (" +
        string(cpu_gflops) + " GFLOP/s)");
    out("max error       : " + string(max_err));
    out("device peak     : " + string(device_peak_bytes()) + " bytes");
};
