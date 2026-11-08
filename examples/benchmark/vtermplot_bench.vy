# vtermplot_bench.vy - terminal rendering throughput.
#
# Run compiled:
#   ./vynec --compile examples/benchmark/vtermplot_bench.vy
#   ./examples/benchmark/vtermplot_bench

use external "vtermplot/vtermplot.vy";
use native vcore;
use native vmath;

out("vtermplot benchmark");

N :: Int64 := 1000;
xs :: Array<Float64> := [];
ys :: Array<Float64> := [];
through i :: 0..N-1 -> loop {
    xs.push(float64(i));
    ys.push(vmath.sin(float64(i) * 0.02));
};

# Warm up allocations before timing.
line_warm :: String := vtermplot.plot_line(xs, ys, 100, 30, "sin");
t0 :: Int64 := vcore.now_ns();
line_out :: String := line_warm;
through k :: 0..19 -> loop {
    line_out = vtermplot.plot_line(xs, ys, 100, 30, "sin");
};
t1 :: Int64 := vcore.now_ns();
out("line 100x30 x20 : " + string((t1 - t0) / 1000000) + " ms (len=" + string(sizeof(line_out)) + ")");

rows :: Int64 := 200;
cols :: Int64 := 80;
heat :: Array<Float64> := [];
through i :: 0..rows*cols-1 -> loop { heat.push(float64(i % 101)); };
heat_warm :: String := vtermplot.plot_heatmap(heat, rows, cols, 80, 30, "heat");
t2 :: Int64 := vcore.now_ns();
heat_out :: String := heat_warm;
through k :: 0..19 -> loop {
    heat_out = vtermplot.plot_heatmap(heat, rows, cols, 80, 30, "heat");
};
t3 :: Int64 := vcore.now_ns();
out("heatmap 80x30 x20: " + string((t3 - t2) / 1000000) + " ms (len=" + string(sizeof(heat_out)) + ")");

big :: Array<Float64> := [];
through i :: 0..9999 -> loop { big.push(vmath.sin(float64(i) * 0.001)); };
spark_warm :: String := vtermplot.sparkline(big);
t4 :: Int64 := vcore.now_ns();
spark_out :: String := spark_warm;
through k :: 0..99 -> loop { spark_out = vtermplot.sparkline(big); };
t5 :: Int64 := vcore.now_ns();
out("sparkline 10k x100: " + string((t5 - t4) / 1000000) + " ms (len=" + string(sizeof(spark_out)) + ")");
