# vstat/Correlation.vy - covariance and association measures.
#
# All functions take Series. Mean-centred sums are delegated to
# k_cov_sum so the hot loop stays unboxed.

use "Types.vy";
use "Kernels.vy";
use "Reductions.vy";

ruleset { dynamic_casting };

module vstat;
use native vmath;

fn :: vstat covariance(a :: vstat.Series, b :: vstat.Series,
                       sample :: Bool) -> Float64 {
    if a.n != b.n {
        throw "vstat.covariance: series differ in length";
    }
    if a.n <= 0 { throw "vstat.covariance: empty series"; }
    if sample && a.n < 2 {
        throw "vstat.covariance: sample covariance needs at least 2 values";
    }
    ma :: Float64 := vstat.mean(a);
    mb :: Float64 := vstat.mean(b);
    s :: Float64 := vstat.k_cov_sum(a.data, b.data, ma, mb, a.n);
    denom :: Float64 := float64(a.n);
    if sample { denom = float64(a.n - 1); }
    return s / denom;
}

fn :: vstat pearson(a :: vstat.Series, b :: vstat.Series) -> Float64 {
    if a.n != b.n {
        throw "vstat.pearson: series differ in length";
    }
    if a.n <= 0 { throw "vstat.pearson: empty series"; }
    cov :: Float64 := vstat.covariance(a, b, false);
    sa :: Float64 := vstat.stddev(a, false);
    sb :: Float64 := vstat.stddev(b, false);
    if sa == 0.0 || sb == 0.0 {
        throw "vstat.pearson: a series has zero variance";
    }
    return cov / (sa * sb);
}

fn :: vstat spearman(a :: vstat.Series, b :: vstat.Series) -> Float64 {
    if a.n != b.n {
        throw "vstat.spearman: series differ in length";
    }
    return vstat.pearson(vstat.rank(a), vstat.rank(b));
}

# Correlation between x[0 .. n-lag-1] and x[lag .. n-1].
fn :: vstat autocorrelation(s :: vstat.Series, lag :: Int64) -> Float64 {
    if lag <= 0 || lag >= s.n {
        throw "vstat.autocorrelation: lag must be in 1 .. n-1";
    }
    left :: Array<Float64> := [];
    right :: Array<Float64> := [];
    m :: Int64 := s.n - lag;
    through i :: 0..m-1 -> loop {
        left.push(s.data[i]);
        right.push(s.data[i + lag]);
    };
    return vstat.pearson(vstat.Series(left, m), vstat.Series(right, m));
}

# Normalised cross-correlation over the overlap, scanned at one lag.
fn :: vstat cross_correlation(a :: vstat.Series, b :: vstat.Series,
                              lag :: Int64) -> Float64 {
    if a.n != b.n {
        throw "vstat.cross_correlation: series differ in length";
    }
    if lag <= 0 || lag >= a.n {
        throw "vstat.cross_correlation: lag must be in 1 .. n-1";
    }
    left :: Array<Float64> := [];
    right :: Array<Float64> := [];
    m :: Int64 := a.n - lag;
    through i :: 0..m-1 -> loop {
        left.push(a.data[i]);
        right.push(b.data[i + lag]);
    };
    return vstat.pearson(vstat.Series(left, m), vstat.Series(right, m));
}

# Coefficient of determination between observed y and predicted y_hat.
fn :: vstat r_squared(y :: vstat.Series, y_hat :: vstat.Series) -> Float64 {
    if y.n != y_hat.n {
        throw "vstat.r_squared: series differ in length";
    }
    if y.n <= 0 { throw "vstat.r_squared: empty series"; }
    my :: Float64 := vstat.mean(y);
    ss_res :: Float64 := 0.0;
    ss_tot :: Float64 := 0.0;
    through i :: 0..y.n-1 -> loop {
        r :: Float64 := y.data[i] - y_hat.data[i];
        t :: Float64 := y.data[i] - my;
        ss_res = ss_res + r * r;
        ss_tot = ss_tot + t * t;
    };
    if ss_tot == 0.0 { return 0.0; }
    return 1.0 - ss_res / ss_tot;
}
