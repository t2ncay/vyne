# vinterp_test.vy - 1-D and 2-D interpolation checks.

use external "vinterp/vinterp.vy";

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

out("=== vinterp ===");

xs :: Array<Float64> := [0.0, 1.0, 2.0, 3.0, 4.0];
ys :: Array<Float64> := [0.0, 1.0, 4.0, 9.0, 16.0];
c :: vinterp.Curve := vinterp.Curve(xs, ys, 5);

check("nearest-low", close(vinterp.nearest(c, 1.4), 1.0));
check("nearest-high", close(vinterp.nearest(c, 1.6), 4.0));
check("linear-mid", close(vinterp.linear(c, 1.5), 2.5));
check("linear-left-extrap", close(vinterp.linear(c, -1.0), -1.0));
check("linear-right-extrap", close(vinterp.linear(c, 5.0), 23.0));
check("clamped-left", close(vinterp.clamped_linear(c, -1.0), 0.0));
check("clamped-right", close(vinterp.clamped_linear(c, 5.0), 16.0));
check("lagrange", close(vinterp.lagrange(c, 1.5), 2.25));
check("polynomial", close(vinterp.polynomial(c, 2.5), 6.25));

sp :: vinterp.Spline := vinterp.spline(c);
check("spline-knot-1", close(vinterp.spline_eval(sp, 1.0), 1.0));
check("spline-knot-2", close(vinterp.spline_eval(sp, 2.0), 4.0));
check("spline-knot-3", close(vinterp.spline_eval(sp, 3.0), 9.0));
mid :: Float64 := vinterp.spline_eval(sp, 1.5);
check("spline-mid-range", mid > 1.0 && mid < 4.0);

qs :: Array<Float64> := [0.5, 1.5, 2.5];
lin_q := vinterp.resample(c, qs);
check("resample-len", lin_q.size() == 3);
check("resample-0", close(lin_q[0], 0.5));
check("resample-1", close(lin_q[1], 2.5));
check("resample-2", close(lin_q[2], 6.5));

knots :: Array<Float64> := [1.0, 2.0, 3.0];
spl_q := vinterp.resample_spline(sp, knots);
check("resample_spline-0", close(spl_q[0], 1.0));
check("resample_spline-1", close(spl_q[1], 4.0));
check("resample_spline-2", close(spl_q[2], 9.0));

# ---------------------------------------------------------------------------
# 2-D grid
# ---------------------------------------------------------------------------

gd :: Array<Float64> := [0.0, 1.0, 2.0, 10.0, 11.0, 12.0, 20.0, 21.0, 22.0];
g :: vinterp.Grid := vinterp.Grid(gd, 3, 3);

check("nearest2d-11", close(vinterp.nearest_at(g, 1.4, 1.4), 11.0));
check("nearest2d-22", close(vinterp.nearest_at(g, 1.6, 1.6), 22.0));
check("bilinear2d", close(vinterp.bilinear_at(g, 1.5, 1.5), 16.5));
check("bicubic2d-00", close(vinterp.bicubic_at(g, 0.0, 0.0), 0.0));
check("bicubic2d-11", close(vinterp.bicubic_at(g, 1.0, 1.0), 11.0));
check("bicubic2d-22", close(vinterp.bicubic_at(g, 2.0, 2.0), 22.0));

check("phys-nearest", close(vinterp.nearest2d(g, 1.1, 1.1, 0.0, 2.0, 0.0, 2.0), 11.0));
check("phys-bilinear", close(vinterp.bilinear2d(g, 1.1, 1.1, 0.0, 2.0, 0.0, 2.0), 12.1));
check("phys-bicubic", close(vinterp.bicubic2d(g, 1.0, 1.0, 0.0, 2.0, 0.0, 2.0), 11.0));
check("grid-value", close(vinterp.grid_value(g, 1, 1), 11.0));

if failures[0] > 0 {
    out("vinterp: " + string(failures[0]) + " check(s) failed");
    exit(1);
}
out("vinterp: all checks passed");
