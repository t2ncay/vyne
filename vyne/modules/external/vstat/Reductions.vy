# vstat/Reductions.vy - descriptive statistics over a Series.
#
# Every function takes a Series (typed data + length) and returns a
# primitive, a new Series, or a Summary. The k_* kernels do the loops;
# these wrappers validate inputs, allocate outputs, and handle the empty
# / degenerate cases.

use "Types.vy";
use "Kernels.vy";

ruleset { dynamic_casting };

module vstat;
use native vmath;

# ---------------------------------------------------------------------------
# Construction / conversion
# ---------------------------------------------------------------------------

# Copy a boxed Array into a Series. Use `vstat.Series(data, n)` directly
# when you already have an Array<Float64> and its length to avoid the copy.
fn :: vstat from_array(xs :: Array) -> vstat.Series {
    n :: Int64 := xs.size();
    data :: Array<Float64> := [];
    through i :: 0..n-1 -> loop { data.push(xs[i]); };
    return vstat.Series(data, n);
}

fn :: vstat to_array(s :: vstat.Series) -> Array<Float64> {
    data :: Array<Float64> := [];
    through i :: 0..s.n-1 -> loop { data.push(s.data[i]); };
    return data;
}

fn :: vstat sorted_copy(s :: vstat.Series) -> vstat.Series {
    data :: Array<Float64> := [];
    through i :: 0..s.n-1 -> loop { data.push(s.data[i]); };
    vstat.k_sort_inplace(data, s.n);
    return vstat.Series(data, s.n);
}

# Average ranks (ties share a rank). Used by Spearman correlation.
fn :: vstat rank(s :: vstat.Series) -> vstat.Series {
    ranks :: Array<Float64> := [];
    through i :: 0..s.n-1 -> loop { ranks.push(0.0); };
    vstat.k_rank_average(s.data, s.n, ranks);
    return vstat.Series(ranks, s.n);
}

# ---------------------------------------------------------------------------
# Basic reductions
# ---------------------------------------------------------------------------

fn :: vstat sum(s :: vstat.Series) -> Float64 {
    return vstat.k_sum(s.data, s.n);
}

fn :: vstat mean(s :: vstat.Series) -> Float64 {
    if s.n <= 0 { throw "vstat.mean: empty series"; }
    return vstat.k_sum(s.data, s.n) / float64(s.n);
}

fn :: vstat minimum(s :: vstat.Series) -> Float64 {
    if s.n <= 0 { throw "vstat.minimum: empty series"; }
    return vstat.k_min(s.data, s.n);
}

fn :: vstat maximum(s :: vstat.Series) -> Float64 {
    if s.n <= 0 { throw "vstat.maximum: empty series"; }
    return vstat.k_max(s.data, s.n);
}

fn :: vstat range(s :: vstat.Series) -> Float64 {
    return vstat.maximum(s) - vstat.minimum(s);
}

fn :: vstat variance(s :: vstat.Series, sample :: Bool) -> Float64 {
    if s.n <= 0 { throw "vstat.variance: empty series"; }
    if sample && s.n < 2 {
        throw "vstat.variance: sample variance needs at least 2 values";
    }
    m :: Float64 := vstat.mean(s);
    ss :: Float64 := vstat.k_devsq_sum(s.data, m, s.n);
    denom :: Float64 := float64(s.n);
    if sample { denom = float64(s.n - 1); }
    return ss / denom;
}

fn :: vstat stddev(s :: vstat.Series, sample :: Bool) -> Float64 {
    return vmath.sqrt(vstat.variance(s, sample));
}

fn :: vstat median(s :: vstat.Series) -> Float64 {
    if s.n <= 0 { throw "vstat.median: empty series"; }
    ordered :: vstat.Series := vstat.sorted_copy(s);
    mid :: Int64 := s.n / 2;
    if s.n % 2 == 1 { return ordered.data[mid]; }
    return (ordered.data[mid - 1] + ordered.data[mid]) / 2.0;
}

# Linearly interpolated quantile; q in [0, 1].
fn :: vstat quantile(s :: vstat.Series, q :: Float64) -> Float64 {
    if s.n <= 0 { throw "vstat.quantile: empty series"; }
    if q <= 0.0 { return vstat.minimum(s); }
    if q >= 1.0 { return vstat.maximum(s); }
    ordered :: vstat.Series := vstat.sorted_copy(s);
    if s.n == 1 { return ordered.data[0]; }
    pos :: Float64 := q * float64(s.n - 1);
    lo :: Int64 := int64(pos);
    if lo >= s.n - 1 { return ordered.data[s.n - 1]; }
    frac :: Float64 := pos - float64(lo);
    return ordered.data[lo] * (1.0 - frac) + ordered.data[lo + 1] * frac;
}

fn :: vstat percentile(s :: vstat.Series, p :: Float64) -> Float64 {
    return vstat.quantile(s, p / 100.0);
}

fn :: vstat iqr(s :: vstat.Series) -> Float64 {
    return vstat.quantile(s, 0.75) - vstat.quantile(s, 0.25);
}

# Median absolute deviation from the median.
fn :: vstat mad(s :: vstat.Series) -> Float64 {
    med :: Float64 := vstat.median(s);
    dev :: Array<Float64> := [];
    through i :: 0..s.n-1 -> loop {
        d :: Float64 := s.data[i] - med;
        if d < 0.0 { d = 0.0 - d; }
        dev.push(d);
    };
    return vstat.median(vstat.Series(dev, s.n));
}

fn :: vstat mode(s :: vstat.Series) -> Float64 {
    if s.n <= 0 { throw "vstat.mode: empty series"; }
    ordered :: vstat.Series := vstat.sorted_copy(s);
    best :: Float64 := ordered.data[0];
    best_count :: Int64 := 1;
    run_value :: Float64 := ordered.data[0];
    run_count :: Int64 := 1;
    through i :: 1..s.n-1 -> loop {
        if ordered.data[i] == run_value {
            run_count = run_count + 1;
        } else {
            if run_count > best_count {
                best_count = run_count;
                best = run_value;
            }
            run_value = ordered.data[i];
            run_count = 1;
        }
    };
    if run_count > best_count {
        best = run_value;
    }
    return best;
}

fn :: vstat weighted_mean(s :: vstat.Series, w :: vstat.Series) -> Float64 {
    if s.n != w.n {
        throw "vstat.weighted_mean: series and weights differ in length";
    }
    sw :: Float64 := vstat.k_sum(w.data, w.n);
    if sw == 0.0 { throw "vstat.weighted_mean: weights sum to zero"; }
    return vstat.k_dot(s.data, w.data, s.n) / sw;
}

# ---------------------------------------------------------------------------
# Derived series
# ---------------------------------------------------------------------------

fn :: vstat zscore(s :: vstat.Series) -> vstat.Series {
    m :: Float64 := vstat.mean(s);
    sd :: Float64 := vstat.stddev(s, false);
    result :: Array<Float64> := [];
    if sd == 0.0 {
        through i :: 0..s.n-1 -> loop { result.push(0.0); };
        return vstat.Series(result, s.n);
    }
    through i :: 0..s.n-1 -> loop { result.push((s.data[i] - m) / sd); };
    return vstat.Series(result, s.n);
}

fn :: vstat minmax_normalize(s :: vstat.Series) -> vstat.Series {
    lo :: Float64 := vstat.minimum(s);
    hi :: Float64 := vstat.maximum(s);
    span :: Float64 := hi - lo;
    result :: Array<Float64> := [];
    through i :: 0..s.n-1 -> loop {
        if span == 0.0 { result.push(0.0); }
        else { result.push((s.data[i] - lo) / span); }
    };
    return vstat.Series(result, s.n);
}

fn :: vstat standardize(s :: vstat.Series) -> vstat.Series {
    return vstat.zscore(s);
}

fn :: vstat abs_series(s :: vstat.Series) -> vstat.Series {
    result :: Array<Float64> := [];
    through i :: 0..s.n-1 -> loop {
        if s.data[i] < 0.0 { result.push(0.0 - s.data[i]); }
        else { result.push(s.data[i]); }
    };
    return vstat.Series(result, s.n);
}

fn :: vstat cumulative_sum(s :: vstat.Series) -> vstat.Series {
    result :: Array<Float64> := [];
    running :: Float64 := 0.0;
    through i :: 0..s.n-1 -> loop {
        running = running + s.data[i];
        result.push(running);
    };
    return vstat.Series(result, s.n);
}

fn :: vstat difference(s :: vstat.Series) -> vstat.Series {
    if s.n <= 1 { return vstat.Series([], 0); }
    result :: Array<Float64> := [];
    through i :: 1..s.n-1 -> loop { result.push(s.data[i] - s.data[i - 1]); };
    return vstat.Series(result, s.n - 1);
}

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

fn :: vstat summary(s :: vstat.Series) -> vstat.Summary {
    if s.n <= 0 { throw "vstat.summary: empty series"; }
    m :: Float64 := vstat.mean(s);
    v :: Float64 := vstat.variance(s, false);
    return vstat.Summary(
        s.n,
        m,
        v,
        vmath.sqrt(v),
        vstat.minimum(s),
        vstat.maximum(s),
        vstat.median(s)
    );
}
