# vtermplot_test.vy - behavioural checks for the terminal plotting library.
#
# Run compiled:
#   ./vynec --compile examples/test/vtermplot_test.vy
#   ./examples/test/vtermplot_test

use external "vtermplot/vtermplot.vy";

failures :: Array<Int64> := [0];

fn check(name :: String, cond :: Bool) {
    if cond {
        out("PASS " + name);
    } else {
        out("FAIL " + name);
        failures[0] = failures[0] + 1;
    }
}

out("=== vtermplot ===");

xs :: Array<Float64> := [0.0, 1.0, 2.0, 3.0];
ys :: Array<Float64> := [0.0, 1.0, 4.0, 9.0];

s :: vtermplot.SeriesData := vtermplot.series(xs, ys, "quad", 2);
check("series-n", s.n == 4);
check("series-marker", s.marker == 2);

p :: vtermplot.Plot := vtermplot.new_plot(40, 12, "quadratic", "x", "y");
check("plot-unicode", p.unicode);
check("plot-grid-size", p.grid.size() == 40 * 12);
check("plot-no-data", !p.has_data);

vtermplot.line(p, xs, ys, 2);
check("line-xmin", p.xmin == 0.0);
check("line-xmax", p.xmax == 3.0);
check("line-ymin", p.ymin == 0.0);
check("line-ymax", p.ymax == 9.0);

r :: String := vtermplot.render(p);
check("render-title", r.find("quadratic") >= 0);
check("render-axis", r.find("└") >= 0);

vtermplot.set_unicode(p, false);
ra :: String := vtermplot.render(p);
check("ascii-axis", ra.find("+") >= 0);
vtermplot.set_unicode(p, true);

p2 :: vtermplot.Plot := vtermplot.new_plot(30, 10, "scatter", "", "");
vtermplot.scatter(p2, xs, ys, 1);
check("scatter-render", sizeof(vtermplot.render(p2)) > 0);

p3 :: vtermplot.Plot := vtermplot.new_plot(30, 10, "line-series", "", "");
vtermplot.line_series(p3, s);
check("line-series-render", sizeof(vtermplot.render(p3)) > 0);

bars :: Array<Float64> := [1.0, 3.0, 2.0, 5.0];
p4 :: vtermplot.Plot := vtermplot.new_plot(30, 10, "bars", "", "");
vtermplot.bars(p4, bars, 3);
check("bars-render", sizeof(vtermplot.render(p4)) > 0);

hxs :: Array<Float64> := [1.0, 2.0, 2.0, 3.0, 3.0, 3.0, 4.0, 4.0, 5.0];
p5 :: vtermplot.Plot := vtermplot.new_plot(30, 10, "hist", "", "");
vtermplot.hist(p5, hxs, 4, 3);
check("hist-render", sizeof(vtermplot.render(p5)) > 0);

data :: Array<Float64> := [];
through i :: 0..99 -> loop { data.push(float64(i)); };
p6 :: vtermplot.Plot := vtermplot.new_plot(20, 8, "heat", "", "");
vtermplot.heatmap(p6, data, 10, 10);
rh :: String := vtermplot.render(p6);
check("heat-render", sizeof(rh) > 0);
check("heat-char", rh.find("█") >= 0);

sp :: String := vtermplot.sparkline(xs);
check("sparkline-value", sp == "▁▃▅█");
check("sparkline-char", sp.find("█") >= 0);

spa :: String := vtermplot.sparkline_ascii(xs);
check("sparkline-ascii-len", sizeof(spa) == 4);
check("sparkline-ascii-char", spa.find("@") >= 0);

pl :: String := vtermplot.plot_line(xs, ys, 30, 10, "convenience");
check("plot_line", pl.find("convenience") >= 0);
ph :: String := vtermplot.plot_hist(hxs, 4, 30, 10, "ph");
check("plot_hist", sizeof(ph) > 0);
pm :: String := vtermplot.plot_heatmap(data, 10, 10, 20, 8, "phm");
check("plot_heatmap", sizeof(pm) > 0);
pb :: String := vtermplot.plot_bars(bars, 30, 10, "pb");
check("plot_bars", sizeof(pb) > 0);

if failures[0] > 0 {
    out("vtermplot: " + string(failures[0]) + " check(s) failed");
    exit(1);
}
out("vtermplot: all checks passed");
