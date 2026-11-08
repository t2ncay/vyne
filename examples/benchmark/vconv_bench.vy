# vconv_bench.vy - convolution throughput.
#
# Run compiled:
#   ./vynec --compile examples/benchmark/vconv_bench.vy
#   ./examples/benchmark/vconv_bench

use external "vconv/vconv.vy";
use native vcore;

W :: Int64 := 64;
H :: Int64 := 64;
C :: Int64 := 3;
OC :: Int64 := 32;
K :: Int64 := 3;
STEPS :: Int64 := 20;

in_data :: Array<Float64> := [];
through i :: 0..(H * W * C)-1 -> loop {
    in_data.push(float64(i % 251) / 251.0);
};

weight :: Array<Float64> := [];
through i :: 0..(OC * C * K * K)-1 -> loop {
    weight.push((float64((i * 17) % 101) - 50.0) / 1000.0);
};

bias :: Array<Float64> := [];
through i :: 0..OC-1 -> loop { bias.push(0.01); };

input :: vconv.Tensor := vconv.tensor(in_data, H, W, C);

# Warm the kernel and the arena.
warm :: vconv.Tensor := vconv.conv2d(input, weight, bias, OC, K, 1, 1);

t0 :: Int64 := vcore.now_ns();
through s :: 0..STEPS-1 -> loop {
    y :: vconv.Tensor := vconv.conv2d(input, weight, bias, OC, K, 1, 1);
    if y.data.size() == 0 { out("bad output"); }
};
t1 :: Int64 := vcore.now_ns();

per :: Float64 := float64(t1 - t0) / 1000000.0 / float64(STEPS);
out("vconv benchmark");
out("shape      : " + string(H) + "x" + string(W) + "x" + string(C) +
    " -> " + string(OC) + " channels, " + string(K) + "x" + string(K));
out("steps      : " + string(STEPS));
out("per step   : " + string(per) + " ms");
