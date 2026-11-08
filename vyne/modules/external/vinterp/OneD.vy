# vinterp/OneD.vy - 1-D interpolation over a sorted Curve.

use "Types.vy";
use "Kernels.vy";

ruleset { dynamic_casting };

module vinterp;

fn :: vinterp nearest(c :: vinterp.Curve, x :: Float64) -> Float64 {
    if c.n <= 0 { throw "vinterp.nearest: empty curve"; }
    return vinterp.k_nearest(c.xs, c.ys, c.n, x);
}

# Linear interpolation, with linear extrapolation outside the knots.
fn :: vinterp linear(c :: vinterp.Curve, x :: Float64) -> Float64 {
    if c.n <= 0 { throw "vinterp.linear: empty curve"; }
    return vinterp.k_linear(c.xs, c.ys, c.n, x);
}

# Linear interpolation clamped to the first/last knot outside the range.
fn :: vinterp clamped_linear(c :: vinterp.Curve, x :: Float64) -> Float64 {
    if c.n <= 0 { throw "vinterp.clamped_linear: empty curve"; }
    xc :: Float64 := x;
    if xc < c.xs[0] { xc = c.xs[0]; }
    if xc > c.xs[c.n-1] { xc = c.xs[c.n-1]; }
    return vinterp.k_linear(c.xs, c.ys, c.n, xc);
}

# Lagrange interpolation through every point. O(n^2); use for a small
# number of irregular samples.
fn :: vinterp lagrange(c :: vinterp.Curve, x :: Float64) -> Float64 {
    if c.n <= 0 { throw "vinterp.lagrange: empty curve"; }
    return vinterp.k_lagrange(c.xs, c.ys, c.n, x);
}

fn :: vinterp polynomial(c :: vinterp.Curve, x :: Float64) -> Float64 {
    return vinterp.lagrange(c, x);
}

# Natural cubic spline (second derivative zero at both ends).
fn :: vinterp spline(c :: vinterp.Curve) -> vinterp.Spline {
    n :: Int64 := c.n;
    m :: Array<Float64> := [];
    u :: Array<Float64> := [];
    through i :: 0..n-1 -> loop {
        m.push(0.0);
        u.push(0.0);
    };

    if n >= 3 {
        through i :: 1..n-2 -> loop {
            h0 :: Float64 := c.xs[i] - c.xs[i-1];
            h1 :: Float64 := c.xs[i+1] - c.xs[i];
            sig :: Float64 := h0 / (h0 + h1);
            p :: Float64 := sig * m[i-1] + 2.0;
            m[i] = (sig - 1.0) / p;
            d :: Float64 :=
                (c.ys[i+1] - c.ys[i]) / h1 - (c.ys[i] - c.ys[i-1]) / h0;
            u[i] = (6.0 * d / (h0 + h1) - sig * u[i-1]) / p;
        };
        k :: Int64 := n - 2;
        while k >= 1 {
            m[k] = m[k] * m[k+1] + u[k];
            k = k - 1;
        }
    }
    return vinterp.Spline(c.xs, c.ys, m, n);
}

fn :: vinterp spline_eval(s :: vinterp.Spline, x :: Float64) -> Float64 {
    if s.n <= 0 { throw "vinterp.spline_eval: empty spline"; }
    return vinterp.k_spline_eval(s.xs, s.ys, s.m, s.n, x);
}

# Linear resample at a list of query points.
fn :: vinterp resample(c :: vinterp.Curve, xs :: Array) -> Array<Float64> {
    n :: Int64 := xs.size();
    result :: Array<Float64> := [];
    through i :: 0..n-1 -> loop {
        result.push(vinterp.linear(c, xs[i]));
    };
    return result;
}

fn :: vinterp resample_spline(s :: vinterp.Spline,
                              xs :: Array) -> Array<Float64> {
    n :: Int64 := xs.size();
    result :: Array<Float64> := [];
    through i :: 0..n-1 -> loop {
        result.push(vinterp.spline_eval(s, xs[i]));
    };
    return result;
}
