# vstat_bench.vy - throughput of the vstat reductions.
#
# Run compiled:
#   ./vynec --compile examples/benchmark/vstat_bench.vy
#   ./examples/benchmark/vstat_bench

use external "vstat/vstat.vy";
use native vcore;

N :: Int64 := 200000;

out("vstat benchmark");
out("N = " + string(N));

data :: Array<Float64> := [];
seed :: Int64 := 12345;
through i :: 0..N-1 -> loop {
    seed = (seed * 1103515245 + 12345) % 2147483648;
    if seed < 0 { seed = 0 - seed; }
    data.push(float64(seed) / 2147483648.0);
};

s :: vstat.Series := vstat.Series(data, N);

t0 :: Int64 := vcore.now_ns();
m :: Float64 := vstat.mean(s);
v :: Float64 := vstat.variance(s, false);
t1 :: Int64 := vcore.now_ns();
out("mean+variance : " + string((t1 - t0) / 1000000) + " ms  (mean=" + string(m) + ")");

t2 :: Int64 := vcore.now_ns();
med :: Float64 := vstat.median(s);
t3 :: Int64 := vcore.now_ns();
out("median (sort) : " + string((t3 - t2) / 1000000) + " ms  (median=" + string(med) + ")");

t4 :: Int64 := vcore.now_ns();
h :: vstat.Histogram := vstat.histogram(s, 64);
t5 :: Int64 := vcore.now_ns();
out("histogram    : " + string((t5 - t4) / 1000000) + " ms  (bins=" + string(h.counts.size()) + ")");

t6 :: Int64 := vcore.now_ns();
q1 :: Float64 := vstat.quantile(s, 0.25);
q3 :: Float64 := vstat.quantile(s, 0.75);
t7 :: Int64 := vcore.now_ns();
out("quantiles    : " + string((t7 - t6) / 1000000) + " ms  (IQR=" + string(q3 - q1) + ")");

t8 :: Int64 := vcore.now_ns();
zs :: vstat.Series := vstat.zscore(s);
t9 :: Int64 := vcore.now_ns();
out("zscore       : " + string((t9 - t8) / 1000000) + " ms  (n=" + string(zs.n) + ")");
