# vcomplex_bench.vy - split-array complex arithmetic throughput.
#
# Run compiled:
#   ./vynec --compile examples/benchmark/vcomplex_bench.vy
#   ./examples/benchmark/vcomplex_bench

use external "vcomplex/vcomplex.vy";
use native vcore;

N :: Int64 := 1000000;
out("vcomplex benchmark");
out("N = " + string(N));

re_a :: Array<Float64> := [];
im_a :: Array<Float64> := [];
re_b :: Array<Float64> := [];
im_b :: Array<Float64> := [];
seed :: Int64 := 12345;
through i :: 0..N-1 -> loop {
    seed = (seed * 1103515245 + 12345) % 2147483648;
    if seed < 0 { seed = 0 - seed; }
    re_a.push(float64(seed) / 2147483648.0);
    seed = (seed * 1103515245 + 12345) % 2147483648;
    if seed < 0 { seed = 0 - seed; }
    im_a.push(float64(seed) / 2147483648.0);
    seed = (seed * 1103515245 + 12345) % 2147483648;
    if seed < 0 { seed = 0 - seed; }
    re_b.push(float64(seed) / 2147483648.0);
    seed = (seed * 1103515245 + 12345) % 2147483648;
    if seed < 0 { seed = 0 - seed; }
    im_b.push(float64(seed) / 2147483648.0);
};

a :: vcomplex.ComplexArray := vcomplex.ComplexArray(re_a, im_a, N);
b :: vcomplex.ComplexArray := vcomplex.ComplexArray(re_b, im_b, N);

c :: vcomplex.ComplexArray := vcomplex.a_mul(a, b);
t0 :: Int64 := vcore.now_ns();
through k :: 0..9 -> loop { c = vcomplex.a_mul(a, b); };
t1 :: Int64 := vcore.now_ns();
out("a_mul x10   : " + string((t1 - t0) / 1000000) + " ms");

mag :: Array<Float64> := vcomplex.a_abs(a);
t2 :: Int64 := vcore.now_ns();
through k :: 0..9 -> loop { mag = vcomplex.a_abs(a); };
t3 :: Int64 := vcore.now_ns();
out("a_abs x10   : " + string((t3 - t2) / 1000000) + " ms");

ang :: Array<Float64> := vcomplex.a_arg(a);
t4 :: Int64 := vcore.now_ns();
through k :: 0..9 -> loop { ang = vcomplex.a_arg(a); };
t5 :: Int64 := vcore.now_ns();
out("a_arg x10   : " + string((t5 - t4) / 1000000) + " ms");

s :: vcomplex.Complex := vcomplex.a_sum(a);
t6 :: Int64 := vcore.now_ns();
through k :: 0..9 -> loop { s = vcomplex.a_sum(a); };
t7 :: Int64 := vcore.now_ns();
out("a_sum x10   : " + string((t7 - t6) / 1000000) + " ms");
