# libraries_integration_test.vy - vcsv + vstat + vinterp + vcomplex + vtermplot.
#
# Builds a CSV in memory, parses it, computes statistics, builds a spline,
# does complex array arithmetic, and renders the data and interpolated
# midpoints to the terminal.
#
#   ./vynec --compile examples/test/libraries_integration_test.vy
#   ./examples/test/libraries_integration_test

use external "vcsv/vcsv.vy";
use external "vstat/vstat.vy";
use external "vinterp/vinterp.vy";
use external "vcomplex/vcomplex.vy";
use external "vtermplot/vtermplot.vy";
use native vcore;
use native vmath;

failures :: Array<Int64> := [0];

fn close(a :: Float64, b :: Float64) -> Bool {
    d :: Float64 := a - b;
    if d < 0.0 { d = 0.0 - d; }
    return d < 0.000001;
}

fn check(name :: String, cond :: Bool) {
    if cond { out("PASS " + name); }
    else {
        out("FAIL " + name);
        failures[0] = failures[0] + 1;
    }
}

out("=== library integration ===");

N :: Int64 := 25;

# ---------------------------------------------------------------------------
# vcsv: build and parse a CSV of (x, y=x^2)
# ---------------------------------------------------------------------------
sb :: Int64 := vcore.sb_create();
vcore.sb_append(sb, "x,y\n");
through i :: 0..N-1 -> loop {
    vcore.sb_append(sb, i);
    vcore.sb_append(sb, ",");
    vcore.sb_append(sb, i * i);
    vcore.sb_append(sb, "\n");
};
csv :: String := vcore.sb_build(sb);

t :: vcsv.Table := vcsv.parse(csv, true);
check("csv-rows", t.n_rows == N);
check("csv-cols", t.n_cols == 2);
check("csv-header-x", vcsv.header(t, 0) == "x");
check("csv-header-y", vcsv.header(t, 1) == "y");

xs :: Array<Float64> := vcsv.column_f64(t, 0);
ys :: Array<Float64> := vcsv.column_f64(t, 1);
check("csv-x-len", xs.size() == N);
check("csv-y-first", close(ys[0], 0.0));
check("csv-y-last", close(ys[N-1], float64((N-1) * (N-1))));

# ---------------------------------------------------------------------------
# vstat: descriptive statistics over y
# ---------------------------------------------------------------------------
s :: vstat.Series := vstat.Series(ys, ys.size());
expected_mean :: Float64 := 0.0;
through i :: 0..N-1 -> loop {
    expected_mean = expected_mean + float64(i * i);
};
expected_mean = expected_mean / float64(N);

check("stat-mean", close(vstat.mean(s), expected_mean));
check("stat-min", close(vstat.minimum(s), 0.0));
check("stat-max", close(vstat.maximum(s), float64((N-1) * (N-1))));
check("stat-summary-n", vstat.summary(s).n == N);

# ---------------------------------------------------------------------------
# vinterp: linear and natural-spline interpolation
# ---------------------------------------------------------------------------
c :: vinterp.Curve := vinterp.Curve(xs, ys, N);
check("interp-linear-mid", close(vinterp.linear(c, 3.5), 12.5));
check("interp-nearest", close(vinterp.nearest(c, 3.4), 9.0));

sp :: vinterp.Spline := vinterp.spline(c);
check("interp-spline-knot-4", close(vinterp.spline_eval(sp, 4.0), 16.0));
check("interp-spline-knot-10", close(vinterp.spline_eval(sp, 10.0), 100.0));

qxs :: Array<Float64> := [];
qys :: Array<Float64> := [];
through i :: 0..N-2 -> loop {
    qx :: Float64 := float64(i) + 0.5;
    qxs.push(qx);
    qys.push(vinterp.linear(c, qx));
};
check("interp-query-count", qxs.size() == N - 1);

# ---------------------------------------------------------------------------
# vcomplex: z = x + i*y over the same table
# ---------------------------------------------------------------------------
re :: Array<Float64> := [];
im :: Array<Float64> := [];
through i :: 0..N-1 -> loop {
    re.push(float64(i));
    im.push(float64(i * i));
};
ca :: vcomplex.ComplexArray := vcomplex.ComplexArray(re, im, N);

mag :: Array<Float64> := vcomplex.a_abs(ca);
check("complex-abs-3", close(mag[3], vmath.sqrt(9.0 + 81.0)));
sumc :: vcomplex.Complex := vcomplex.a_sum(ca);
check("complex-sum-re", close(sumc.re, float64(N * (N - 1)) / 2.0));

dotc :: vcomplex.Complex := vcomplex.a_dot(ca, ca);
dot_re_expected :: Float64 := 0.0;
dot_im_expected :: Float64 := 0.0;
through i :: 0..N-1 -> loop {
    x :: Float64 := float64(i);
    y :: Float64 := float64(i * i);
    dot_re_expected = dot_re_expected + (x * x - y * y);
    dot_im_expected = dot_im_expected + 2.0 * x * y;
};
check("complex-dot-re", close(dotc.re, dot_re_expected));
check("complex-dot-im", close(dotc.im, dot_im_expected));

# ---------------------------------------------------------------------------
# vtermplot: render the data and interpolated midpoints
# ---------------------------------------------------------------------------
p :: vtermplot.Plot := vtermplot.new_plot(
    64, 18, "Integration: y = x^2", "x", "y"
);
vtermplot.line(p, xs, ys, 2);
vtermplot.scatter(p, qxs, qys, 1);
vtermplot.show(p);

check("plot-has-data", p.has_data);
check("plot-xmin", close(p.xmin, 0.0));
check("plot-xmax", close(p.xmax, float64(N - 1)));
check("plot-ymin", close(p.ymin, 0.0));
check("plot-ymax", close(p.ymax, float64((N-1) * (N-1))));

# ---------------------------------------------------------------------------
# vcsv: round-trip
# ---------------------------------------------------------------------------
round :: String := vcsv.serialize(t);
t2 :: vcsv.Table := vcsv.parse(round, true);
check("csv-round-rows", t2.n_rows == N);
check("csv-round-cell", vcsv.cell(t2, 4, 1) == vcsv.cell(t, 4, 1));

if failures[0] > 0 {
    out("library integration: " + string(failures[0]) + " check(s) failed");
    exit(1);
}
out("library integration: all checks passed");
