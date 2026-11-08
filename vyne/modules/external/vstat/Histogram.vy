# vstat/Histogram.vy - equal-width histograms, CDFs, and a Gaussian KDE.
#
# A Histogram carries `edges` (bins + 1 values) and `counts` (bins
# values). The counting loop is k_count_bins, so it stays unboxed.

use "Types.vy";
use "Kernels.vy";
use "Reductions.vy";

ruleset { dynamic_casting };

module vstat;
use native vmath;

fn :: vstat histogram_range(s :: vstat.Series, bins :: Int64,
                            lo :: Float64, hi :: Float64) -> vstat.Histogram {
    if bins <= 0 { throw "vstat.histogram: bins must be positive"; }
    if hi <= lo { throw "vstat.histogram: hi must be greater than lo"; }
    if s.n <= 0 { throw "vstat.histogram: empty series"; }

    width :: Float64 := (hi - lo) / float64(bins);
    edges :: Array<Float64> := [];
    through i :: 0..bins -> loop { edges.push(lo + float64(i) * width); };

    counts :: Array<Int64> := [];
    through i2 :: 0..bins-1 -> loop { counts.push(0); };
    vstat.k_count_bins(s.data, s.n, lo, width, bins, counts);
    return vstat.Histogram(edges, counts);
}

fn :: vstat histogram(s :: vstat.Series, bins :: Int64) -> vstat.Histogram {
    if s.n <= 0 { throw "vstat.histogram: empty series"; }
    lo :: Float64 := vstat.minimum(s);
    hi :: Float64 := vstat.maximum(s);
    if hi == lo { hi = lo + 1.0; }
    return vstat.histogram_range(s, bins, lo, hi);
}

fn :: vstat bin_edges(h :: vstat.Histogram) -> Array<Float64> {
    return h.edges;
}

fn :: vstat bin_counts(h :: vstat.Histogram) -> Array<Int64> {
    return h.counts;
}

fn :: vstat _hist_total(h :: vstat.Histogram) -> Int64 {
    total :: Int64 := 0;
    through i :: 0..h.counts.size()-1 -> loop {
        total = total + h.counts[i];
    };
    return total;
}

# Probability density per bin: count / (total * width).
fn :: vstat density(h :: vstat.Histogram) -> Array<Float64> {
    n :: Int64 := h.counts.size();
    if n <= 0 { return []; }
    total :: Int64 := vstat._hist_total(h);
    if total == 0 { throw "vstat.density: histogram has no samples"; }
    width :: Float64 := h.edges[1] - h.edges[0];
    result :: Array<Float64> := [];
    through i :: 0..n-1 -> loop {
        result.push(float64(h.counts[i]) / (float64(total) * width));
    };
    return result;
}

# Cumulative distribution at each bin's right edge.
fn :: vstat cdf(h :: vstat.Histogram) -> Array<Float64> {
    n :: Int64 := h.counts.size();
    if n <= 0 { return []; }
    total :: Int64 := vstat._hist_total(h);
    if total == 0 { throw "vstat.cdf: histogram has no samples"; }
    result :: Array<Float64> := [];
    running :: Int64 := 0;
    through i :: 0..n-1 -> loop {
        running = running + h.counts[i];
        result.push(float64(running) / float64(total));
    };
    return result;
}

# Fraction of samples <= x.
fn :: vstat empirical_cdf(s :: vstat.Series, x :: Float64) -> Float64 {
    if s.n <= 0 { throw "vstat.empirical_cdf: empty series"; }
    count :: Int64 := 0;
    through i :: 0..s.n-1 -> loop {
        if s.data[i] <= x { count = count + 1; }
    };
    return float64(count) / float64(s.n);
}

# Quantiles at an explicit list of probabilities.
fn :: vstat quantiles(s :: vstat.Series, ps :: vstat.Series) -> Array<Float64> {
    result :: Array<Float64> := [];
    through i :: 0..ps.n-1 -> loop {
        result.push(vstat.quantile(s, ps.data[i]));
    };
    return result;
}

# Gaussian kernel density estimate on a caller-supplied grid.
fn :: vstat kde(s :: vstat.Series, bandwidth :: Float64,
                grid :: vstat.Series) -> vstat.Series {
    if s.n <= 0 { throw "vstat.kde: empty series"; }
    if bandwidth <= 0.0 { throw "vstat.kde: bandwidth must be positive"; }
    if grid.n <= 0 { throw "vstat.kde: empty grid"; }

    inv_h :: Float64 := 1.0 / bandwidth;
    norm :: Float64 := 1.0 / (float64(s.n) * bandwidth *
                               vmath.sqrt(6.283185307179586));
    result :: Array<Float64> := [];
    through g :: 0..grid.n-1 -> loop {
        sum :: Float64 := 0.0;
        through i :: 0..s.n-1 -> loop {
            z :: Float64 := (s.data[i] - grid.data[g]) * inv_h;
            sum = sum + vmath.exp(-0.5 * z * z);
        };
        result.push(sum * norm);
    };
    return vstat.Series(result, grid.n);
}
