# device_conv_test.vy
use external "vconv/vconv.vy";
use native vcore;

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

# --- 1. CORRECTNESS (Tiny 3x3) ---
out("=== device conv2d correctness ===");
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

@device<cuda> region conv_small {
    d_x :: Int64 := device_alloc(9);
    d_w :: Int64 := device_alloc(9);
    d_b :: Int64 := device_alloc(1);
    d_o :: Int64 := device_alloc(9);
    device_upload(d_x, xf, 9); 
    device_upload(d_w, wf, 9);
    device_upload(d_b, bf, 1);
    device_conv2d(d_o, d_x, d_w, d_b, 3, 3, 1, 3, 3, 1, 3, 1, 1, 1);
    device_download(d_o, of, 9);
};
through i4 :: 0..8 -> loop { check("conv-" + string(i4), close(yt.data[i4], float64(of[i4]), 1e-6)); };

# --- 2. PERFORMANCE (Huge 128x128x16) ---
out("\n=== performance benchmark (128x128x16) ===");
H :: Int64 := 128;
W :: Int64 := 128;
C :: Int64 := 16;
OC :: Int64 := 16;
SIZE_IN :: Int64 := H * W * C;
SIZE_W :: Int64 := OC * C * 3 * 3;
SIZE_OUT :: Int64 := H * W * OC;

big_x :: Array<Float32> := [];
big_w :: Array<Float32> := [];
big_b :: Array<Float32> := [];
big_out_cpu :: Array<Float32> := [];
big_out_gpu :: Array<Float32> := [];
through z :: 0..SIZE_IN-1 -> loop { big_x.push(0.5f); };
through z2 :: 0..SIZE_W-1 -> loop { big_w.push(0.01f); };
through z3 :: 0..OC-1 -> loop { big_b.push(0.0f); };
through z4 :: 0..SIZE_OUT-1 -> loop { big_out_cpu.push(0.0f); big_out_gpu.push(0.0f); };

# --- CPU Timing ---
start_cpu := vcore.now_ns();
@device<cpu> region cpu_bench {
    d_x :: Int64 := device_alloc(SIZE_IN); 
    d_w :: Int64 := device_alloc(SIZE_W);
    d_b :: Int64 := device_alloc(OC); 
    d_o :: Int64 := device_alloc(SIZE_OUT);
    device_upload(d_x, big_x, SIZE_IN); 
    device_upload(d_w, big_w, SIZE_W);
    device_upload(d_b, big_b, OC);
    device_conv2d(d_o, d_x, d_w, d_b, H, W, C, H, W, OC, 3, 1, 1, 1);
    device_download(d_o, big_out_cpu, SIZE_OUT);
};
end_cpu := vcore.now_ns();
cpu_ms :: Float64 := float64(end_cpu - start_cpu) / 1000000.0;
out("CPU Time : " + string(cpu_ms) + " ms");

# --- GPU Timing ---
start_gpu := vcore.now_ns();
@device<cuda> region gpu_bench {
    d_x :: Int64 := device_alloc(SIZE_IN); 
    d_w :: Int64 := device_alloc(SIZE_W);
    d_b :: Int64 := device_alloc(OC); 
    d_o :: Int64 := device_alloc(SIZE_OUT);
    device_upload(d_x, big_x, SIZE_IN); 
    device_upload(d_w, big_w, SIZE_W);
    device_upload(d_b, big_b, OC);
    device_conv2d(d_o, d_x, d_w, d_b, H, W, C, H, W, OC, 3, 1, 1, 1);
    device_download(d_o, big_out_gpu, SIZE_OUT);
};
end_gpu := vcore.now_ns();
gpu_ms :: Float64 := float64(end_gpu - start_gpu) / 1000000.0;
out("GPU Time : " + string(gpu_ms) + " ms");

speedup :: Float64 := cpu_ms / gpu_ms;
out("Speedup  : " + string(speedup) + "x");

if failures[0] != 0 {
    out(string(failures[0]) + " check(s) failed");
    exit(1); 
}
out("=== done ===");
