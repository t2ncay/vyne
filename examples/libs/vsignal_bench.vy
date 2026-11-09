# vsignal_bench.vy - filter throughput on a 1 M-sample buffer.
#
#   ./vynec --compile examples/libs/vsignal_bench.vy
#   ./examples/libs/vsignal_bench

use external "vsignal/vsignal.vy";
use native vcore;

out("vsignal benchmark");

N :: Int64 := 1000000;
xs :: Array<Float64> := [];
through i :: 0..N-1 -> loop {
    xs.push(float64(i % 1024) / 1024.0);
};
s :: vsignal.Series := vsignal.Series(xs, N);

# Warm up allocations so the timings measure the kernels, not growth.
warm :: Array<Float64> := vsignal.moving_average(s, 8);
check_warm :: Int64 := warm.size();

t0 :: Int64 := vcore.now_ns();
ma :: Array<Float64> := vsignal.moving_average(s, 8);
t1 :: Int64 := vcore.now_ns();
out("moving_average w=8  : " + string((t1 - t0) / 1000000) + " ms (n=" + string(ma.size()) + ")");

t2 :: Int64 := vcore.now_ns();
ex :: Array<Float64> := vsignal.exponential(s, 0.1);
t3 :: Int64 := vcore.now_ns();
out("exponential a=0.1    : " + string((t3 - t2) / 1000000) + " ms (n=" + string(ex.size()) + ")");

coeffs :: Array<Float64> := [];
through i :: 0..15 -> loop { coeffs.push(1.0 / 16.0); };

t4 :: Int64 := vcore.now_ns();
fir :: Array<Float64> := vsignal.fir(s, coeffs);
t5 :: Int64 := vcore.now_ns();
out("fir m=16             : " + string((t5 - t4) / 1000000) + " ms (n=" + string(fir.size()) + ")");

lp :: vsignal.Biquad := vsignal.lowpass(100.0, 1000.0, 0.70710678);

t6 :: Int64 := vcore.now_ns();
bq :: Array<Float64> := vsignal.biquad_apply(s, lp);
t7 :: Int64 := vcore.now_ns();
out("biquad lowpass       : " + string((t7 - t6) / 1000000) + " ms (n=" + string(bq.size()) + ")");

# Streaming path: 100k samples through a boxed state step.
stream :: vsignal.Biquad := vsignal.biquad_new(0.5, 0.5, 0.0, 0.0, 0.0);
acc :: Float64 := 0.0;
t8 :: Int64 := vcore.now_ns();
through i :: 0..99999 -> loop {
    acc = acc + vsignal.biquad_step(stream, xs[i]);
};
t9 :: Int64 := vcore.now_ns();
out("biquad_step 100k     : " + string((t9 - t8) / 1000000) + " ms (sum=" + string(acc) + ")");

out("done.");
