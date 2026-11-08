# vstat/Kernels.vy - the numerical hot loops.
#
# Every function here takes Array<Float64> / Array<Int64> parameters and
# returns a primitive. That is the native array ABI shape: the call site
# passes `.data`, and the body compiles to a bare C loop. These are leaves
# (they call no other function that takes an array), which keeps them
# eligible under the current native-call constraints.

ruleset { dynamic_casting };

module vstat;

fn :: vstat k_sum(xs :: Array<Float64>, n :: Int64) -> Float64 {
    s :: Float64 := 0.0;
    through i :: 0..n-1 -> loop { s = s + xs[i]; };
    return s;
}

fn :: vstat k_sum_sq(xs :: Array<Float64>, n :: Int64) -> Float64 {
    s :: Float64 := 0.0;
    through i :: 0..n-1 -> loop { s = s + xs[i] * xs[i]; };
    return s;
}

fn :: vstat k_abs_sum(xs :: Array<Float64>, n :: Int64) -> Float64 {
    s :: Float64 := 0.0;
    through i :: 0..n-1 -> loop {
        if xs[i] < 0.0 { s = s - xs[i]; } else { s = s + xs[i]; }
    };
    return s;
}

fn :: vstat k_min(xs :: Array<Float64>, n :: Int64) -> Float64 {
    m :: Float64 := xs[0];
    through i :: 1..n-1 -> loop {
        if xs[i] < m { m = xs[i]; }
    };
    return m;
}

fn :: vstat k_max(xs :: Array<Float64>, n :: Int64) -> Float64 {
    m :: Float64 := xs[0];
    through i :: 1..n-1 -> loop {
        if xs[i] > m { m = xs[i]; }
    };
    return m;
}

fn :: vstat k_dot(xs :: Array<Float64>, ys :: Array<Float64>,
                  n :: Int64) -> Float64 {
    s :: Float64 := 0.0;
    through i :: 0..n-1 -> loop { s = s + xs[i] * ys[i]; };
    return s;
}

# Sum of squared deviations from a known mean. Two-pass variance uses this
# after mean(); it is more stable than E[x^2] - mean^2.
fn :: vstat k_devsq_sum(xs :: Array<Float64>, mean :: Float64,
                        n :: Int64) -> Float64 {
    s :: Float64 := 0.0;
    through i :: 0..n-1 -> loop {
        d :: Float64 := xs[i] - mean;
        s = s + d * d;
    };
    return s;
}

# Sum of (x - mx)(y - my). Covariance / Pearson / regression all use it.
fn :: vstat k_cov_sum(xs :: Array<Float64>, ys :: Array<Float64>,
                      mx :: Float64, my :: Float64,
                      n :: Int64) -> Float64 {
    s :: Float64 := 0.0;
    through i :: 0..n-1 -> loop {
        s = s + (xs[i] - mx) * (ys[i] - my);
    };
    return s;
}

# In-place heapsort. O(n log n), no auxiliary array, no recursion.
# `end` is the exclusive upper bound of the heap during extraction.
fn :: vstat k_sort_inplace(xs :: Array<Float64>, n :: Int64) -> Int64 {
    if n <= 1 { return 0; }

    # Build a max heap.
    start :: Int64 := (n - 2) / 2;
    while start >= 0 {
        root :: Int64 := start;
        while true {
            child :: Int64 := 2 * root + 1;
            if child >= n { break; }
            if child + 1 < n {
                if xs[child] < xs[child + 1] { child = child + 1; }
            }
            if xs[root] < xs[child] {
                tmp :: Float64 := xs[root];
                xs[root] = xs[child];
                xs[child] = tmp;
                root = child;
            } else {
                break;
            }
        }
        start = start - 1;
    }

    # Extract the maximum to the end, shrinking the heap.
    end :: Int64 := n - 1;
    while end > 0 {
        tmp2 :: Float64 := xs[0];
        xs[0] = xs[end];
        xs[end] = tmp2;

        root2 :: Int64 := 0;
        while true {
            child2 :: Int64 := 2 * root2 + 1;
            if child2 >= end { break; }
            if child2 + 1 < end {
                if xs[child2] < xs[child2 + 1] { child2 = child2 + 1; }
            }
            if xs[root2] < xs[child2] {
                tmp3 :: Float64 := xs[root2];
                xs[root2] = xs[child2];
                xs[child2] = tmp3;
                root2 = child2;
            } else {
                break;
            }
        }
        end = end - 1;
    }
    return 0;
}

# Count values into `counts` for equal-width bins in [lo, lo + bins*width).
# Values outside the range are clamped to the end bins.
fn :: vstat k_count_bins(xs :: Array<Float64>, n :: Int64,
                         lo :: Float64, width :: Float64,
                         bins :: Int64, counts :: Array<Int64>) -> Int64 {
    through i :: 0..bins-1 -> loop { counts[i] = 0; };
    through i :: 0..n-1 -> loop {
        idx :: Int64 := int64((xs[i] - lo) / width);
        if idx < 0 { idx = 0; }
        if idx >= bins { idx = bins - 1; }
        counts[idx] = counts[idx] + 1;
    };
    return 0;
}

# Average ranks with ties handled by the standard (less + (equal+1)/2)
# rule. O(n^2); Spearman on very large inputs is the only caller.
fn :: vstat k_rank_average(xs :: Array<Float64>, n :: Int64,
                           ranks :: Array<Float64>) -> Int64 {
    through i :: 0..n-1 -> loop {
        less :: Int64 := 0;
        equal :: Int64 := 0;
        through j :: 0..n-1 -> loop {
            if xs[j] < xs[i] { less = less + 1; }
            else if xs[j] == xs[i] { equal = equal + 1; }
        };
        ranks[i] = float64(less) + (float64(equal) + 1.0) / 2.0;
    };
    return 0;
}

# Sum of squared differences. Caller divides by n for MSE. Image metrics
# pass HWC pixel buffers, so this is also the inner loop for PSNR.
fn :: vstat k_mse_sum(xs :: Array<Float64>, ys :: Array<Float64>,
                      n :: Int64) -> Float64 {
    s :: Float64 := 0.0;
    through i :: 0..n-1 -> loop {
        d :: Float64 := xs[i] - ys[i];
        s = s + d * d;
    };
    return s;
}

# Sum of absolute differences.
fn :: vstat k_mae_sum(xs :: Array<Float64>, ys :: Array<Float64>,
                      n :: Int64) -> Float64 {
    s :: Float64 := 0.0;
    through i :: 0..n-1 -> loop {
        d :: Float64 := xs[i] - ys[i];
        if d < 0.0 { d = 0.0 - d; }
        s = s + d;
    };
    return s;
}
