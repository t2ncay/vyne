# vstat/Streaming.vy - one-pass (Welford) mean/variance/covariance.
#
# Streaming state is immutable: each add returns a new Welford /
# WelfordCov. That keeps the functions pure and lets the compiler treat
# them as ordinary native struct calls.

use "Types.vy";

ruleset { dynamic_casting };

module vstat;
use native vmath;

# ---------------------------------------------------------------------------
# Univariate
# ---------------------------------------------------------------------------

fn :: vstat welford_new() -> vstat.Welford {
    return vstat.Welford(0, 0.0, 0.0);
}

fn :: vstat welford_add(s :: vstat.Welford, x :: Float64) -> vstat.Welford {
    n :: Int64 := s.n + 1;
    delta :: Float64 := x - s.mean;
    mean :: Float64 := s.mean + delta / float64(n);
    delta2 :: Float64 := x - mean;
    m2 :: Float64 := s.m2 + delta * delta2;
    return vstat.Welford(n, mean, m2);
}

fn :: vstat welford_variance(s :: vstat.Welford, sample :: Bool) -> Float64 {
    if s.n <= 0 { throw "vstat.welford_variance: no samples"; }
    if sample && s.n < 2 {
        throw "vstat.welford_variance: sample variance needs 2 samples";
    }
    denom :: Float64 := float64(s.n);
    if sample { denom = float64(s.n - 1); }
    return s.m2 / denom;
}

fn :: vstat welford_stddev(s :: vstat.Welford, sample :: Bool) -> Float64 {
    return vmath.sqrt(vstat.welford_variance(s, sample));
}

# Merge two Welford states (parallel / chunked reduction).
fn :: vstat welford_merge(a :: vstat.Welford,
                          b :: vstat.Welford) -> vstat.Welford {
    if a.n == 0 { return b; }
    if b.n == 0 { return a; }
    n :: Int64 := a.n + b.n;
    delta :: Float64 := b.mean - a.mean;
    mean :: Float64 := a.mean + delta * float64(b.n) / float64(n);
    m2 :: Float64 := a.m2 + b.m2 +
                     delta * delta * float64(a.n) * float64(b.n) / float64(n);
    return vstat.Welford(n, mean, m2);
}

fn :: vstat welford_from_series(s :: vstat.Series) -> vstat.Welford {
    state :: vstat.Welford := vstat.welford_new();
    through i :: 0..s.n-1 -> loop {
        state = vstat.welford_add(state, s.data[i]);
    };
    return state;
}

# ---------------------------------------------------------------------------
# Bivariate
# ---------------------------------------------------------------------------

fn :: vstat wcov_new() -> vstat.WelfordCov {
    return vstat.WelfordCov(0, 0.0, 0.0, 0.0, 0.0, 0.0);
}

fn :: vstat wcov_add(s :: vstat.WelfordCov, x :: Float64,
                     y :: Float64) -> vstat.WelfordCov {
    n :: Int64 := s.n + 1;
    dx :: Float64 := x - s.mean_x;
    dy :: Float64 := y - s.mean_y;
    mean_x :: Float64 := s.mean_x + dx / float64(n);
    mean_y :: Float64 := s.mean_y + dy / float64(n);
    m2_x :: Float64 := s.m2_x + dx * (x - mean_x);
    m2_y :: Float64 := s.m2_y + dy * (y - mean_y);
    c :: Float64 := s.c + dx * (y - mean_y);
    return vstat.WelfordCov(n, mean_x, mean_y, m2_x, m2_y, c);
}

fn :: vstat wcov_covariance(s :: vstat.WelfordCov,
                            sample :: Bool) -> Float64 {
    if s.n <= 0 { throw "vstat.wcov_covariance: no samples"; }
    if sample && s.n < 2 {
        throw "vstat.wcov_covariance: sample covariance needs 2 samples";
    }
    denom :: Float64 := float64(s.n);
    if sample { denom = float64(s.n - 1); }
    return s.c / denom;
}

fn :: vstat wcov_variance_x(s :: vstat.WelfordCov,
                            sample :: Bool) -> Float64 {
    if s.n <= 0 { throw "vstat.wcov_variance_x: no samples"; }
    if sample && s.n < 2 {
        throw "vstat.wcov_variance_x: sample variance needs 2 samples";
    }
    denom :: Float64 := float64(s.n);
    if sample { denom = float64(s.n - 1); }
    return s.m2_x / denom;
}

fn :: vstat wcov_variance_y(s :: vstat.WelfordCov,
                            sample :: Bool) -> Float64 {
    if s.n <= 0 { throw "vstat.wcov_variance_y: no samples"; }
    if sample && s.n < 2 {
        throw "vstat.wcov_variance_y: sample variance needs 2 samples";
    }
    denom :: Float64 := float64(s.n);
    if sample { denom = float64(s.n - 1); }
    return s.m2_y / denom;
}

fn :: vstat wcov_pearson(s :: vstat.WelfordCov) -> Float64 {
    if s.n <= 0 { throw "vstat.wcov_pearson: no samples"; }
    if s.m2_x == 0.0 || s.m2_y == 0.0 {
        throw "vstat.wcov_pearson: a variable has zero variance";
    }
    return s.c / vmath.sqrt(s.m2_x * s.m2_y);
}

fn :: vstat wcov_from_series(x :: vstat.Series,
                             y :: vstat.Series) -> vstat.WelfordCov {
    if x.n != y.n {
        throw "vstat.wcov_from_series: series differ in length";
    }
    state :: vstat.WelfordCov := vstat.wcov_new();
    through i :: 0..x.n-1 -> loop {
        state = vstat.wcov_add(state, x.data[i], y.data[i]);
    };
    return state;
}
