# vinterp_bench.vy - interpolation query throughput.
#
# Run compiled:
#   ./vynec --compile examples/benchmark/vinterp_bench.vy
#   ./examples/benchmark/vinterp_bench

use external "vinterp/vinterp.vy";
use native vcore;
use native vmath;

out("vinterp benchmark");

N :: Int64 := 1000;
xs :: Array<Float64> := [];
ys :: Array<Float64> := [];
through i :: 0..N-1 -> loop {
    t :: Float64 := float64(i) * 0.01;
    xs.push(t);
    ys.push(vmath.sin(t));
};
c :: vinterp.Curve := vinterp.Curve(xs, ys, N);

M :: Int64 := 1000000;
acc :: Float64 := 0.0;
t0 :: Int64 := vcore.now_ns();
through i :: 0..M-1 -> loop {
    q :: Float64 := float64(i) * 0.00000999;
    acc = acc + vinterp.linear(c, q);
};
t1 :: Int64 := vcore.now_ns();
out("linear 1M queries : " + string((t1 - t0) / 1000000) + " ms (checksum=" + string(acc) + ")");

t2 :: Int64 := vcore.now_ns();
sp :: vinterp.Spline := vinterp.spline(c);
t3 :: Int64 := vcore.now_ns();
out("spline build      : " + string((t3 - t2) / 1000000) + " ms");

acc2 :: Float64 := 0.0;
t4 :: Int64 := vcore.now_ns();
through i :: 0..M-1 -> loop {
    q2 :: Float64 := float64(i) * 0.00000999;
    acc2 = acc2 + vinterp.spline_eval(sp, q2);
};
t5 :: Int64 := vcore.now_ns();
out("spline 1M queries : " + string((t5 - t4) / 1000000) + " ms (checksum=" + string(acc2) + ")");

rows :: Int64 := 256;
cols :: Int64 := 256;
gd :: Array<Float64> := [];
through r :: 0..rows-1 -> loop {
    through cc :: 0..cols-1 -> loop {
        gd.push(vmath.sin(float64(r) * 0.02) * vmath.cos(float64(cc) * 0.03));
    };
};
g :: vinterp.Grid := vinterp.Grid(gd, rows, cols);
acc3 :: Float64 := 0.0;
t6 :: Int64 := vcore.now_ns();
through i :: 0..M-1 -> loop {
    fx :: Float64 := float64(i % 25500) * 0.01;
    fy :: Float64 := float64(i % 25000) * 0.01;
    acc3 = acc3 + vinterp.bilinear_at(g, fx, fy);
};
t7 :: Int64 := vcore.now_ns();
out("bilinear 1M queries: " + string((t7 - t6) / 1000000) + " ms (checksum=" + string(acc3) + ")");
