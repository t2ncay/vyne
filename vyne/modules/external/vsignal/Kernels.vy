# vsignal/Kernels.vy - the numerical hot loops.
#
# Every function here takes Array<Float64> parameters and returns a
# primitive, which is the native array ABI shape: the call site passes
# `.data` and the body lowers to a bare C loop. These are leaves (they
# call no other array-taking function), so they keep the native fast
# path on desktop.

ruleset { dynamic_casting };

module vsignal;

# Trailing moving average. The first samples average over a shorter
# window (edge clamping), so the output has the same length as the input.
fn :: vsignal k_move_avg(xs :: Array<Float64>, n :: Int64, w :: Int64,
                         dst :: Array<Float64>) -> Int64 {
    if n <= 0 { return 0; }
    ww :: Int64 := w;
    if ww <= 0 { ww = 1; }
    through i :: 0..n-1 -> loop {
        lo :: Int64 := i - ww + 1;
        if lo < 0 { lo = 0; }
        s :: Float64 := 0.0;
        through j :: lo..i -> loop { s = s + xs[j]; };
        dst[i] = s / float64(i - lo + 1);
    };
    return 0;
}

# First-order exponential smoothing. alpha = 1 follows the input
# exactly; alpha = 0 freezes at the first sample.
fn :: vsignal k_exp(xs :: Array<Float64>, n :: Int64, alpha :: Float64,
                    dst :: Array<Float64>) -> Int64 {
    if n <= 0 { return 0; }
    a :: Float64 := alpha;
    if a < 0.0 { a = 0.0; }
    if a > 1.0 { a = 1.0; }
    y :: Float64 := 0.0;
    through i :: 0..n-1 -> loop {
        if i == 0 { y = xs[0]; }
        else { y = a * xs[i] + (1.0 - a) * y; }
        dst[i] = y;
    };
    return 0;
}

# Causal FIR. y[i] = sum(coeffs[k] * x[i-k]), missing past samples = 0.
fn :: vsignal k_fir(xs :: Array<Float64>, n :: Int64,
                    coeffs :: Array<Float64>, m :: Int64,
                    dst :: Array<Float64>) -> Int64 {
    if n <= 0 || m <= 0 { return 0; }
    through i :: 0..n-1 -> loop {
        s :: Float64 := 0.0;
        through k :: 0..m-1 -> loop {
            j :: Int64 := i - k;
            if j >= 0 { s = s + coeffs[k] * xs[j]; }
        };
        dst[i] = s;
    };
    return 0;
}

# Direct Form II Transposed biquad. Coefficients are already normalized
# by a0 (the RBJ designers in Iir.vy do that).
fn :: vsignal k_biquad(xs :: Array<Float64>, n :: Int64,
                       b0 :: Float64, b1 :: Float64, b2 :: Float64,
                       a1 :: Float64, a2 :: Float64,
                       dst :: Array<Float64>) -> Int64 {
    if n <= 0 { return 0; }
    z1 :: Float64 := 0.0;
    z2 :: Float64 := 0.0;
    through i :: 0..n-1 -> loop {
        x :: Float64 := xs[i];
        y :: Float64 := b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        dst[i] = y;
    };
    return 0;
}

# Causal median filter with edge clamping. The rank selection is inlined
# (no helper call) to stay a native leaf; O(n * w^2) for small windows.
fn :: vsignal k_median(xs :: Array<Float64>, n :: Int64, w :: Int64,
                       dst :: Array<Float64>) -> Int64 {
    if n <= 0 { return 0; }
    ww :: Int64 := w;
    if ww <= 0 { ww = 1; }
    through i :: 0..n-1 -> loop {
        lo :: Int64 := i - ww + 1;
        if lo < 0 { lo = 0; }
        hi :: Int64 := i;
        cnt :: Int64 := hi - lo + 1;
        r1 :: Int64 := (cnt - 1) / 2;
        r2 :: Int64 := cnt / 2;
        m1 :: Float64 := 0.0;
        m2 :: Float64 := 0.0;
        seen1 :: Int64 := 0;
        seen2 :: Int64 := 0;

        through a :: lo..hi -> loop {
            if seen1 == 0 || seen2 == 0 {
                cand :: Float64 := xs[a];
                less :: Int64 := 0;
                equal :: Int64 := 0;
                through b :: lo..hi -> loop {
                    if xs[b] < cand { less = less + 1; }
                    else if xs[b] == cand { equal = equal + 1; }
                };
                if seen1 == 0 && r1 >= less && r1 < less + equal {
                    m1 = cand;
                    seen1 = 1;
                }
                if seen2 == 0 && r2 >= less && r2 < less + equal {
                    m2 = cand;
                    seen2 = 1;
                }
            }
        };

        dst[i] = (m1 + m2) / 2.0;
    };
    return 0;
}

# Linear resampling to out_n points over the input's index range.
fn :: vsignal k_resample_linear(xs :: Array<Float64>, n :: Int64,
                                out_n :: Int64,
                                dst :: Array<Float64>) -> Int64 {
    if n <= 0 || out_n <= 0 { return 0; }
    if out_n == 1 { dst[0] = xs[0]; return 0; }
    if n == 1 {
        through i :: 0..out_n-1 -> loop { dst[i] = xs[0]; };
        return 0;
    }
    scale :: Float64 := float64(n - 1) / float64(out_n - 1);
    through i :: 0..out_n-1 -> loop {
        pos :: Float64 := float64(i) * scale;
        k :: Int64 := int64(pos);
        if k >= n - 1 {
            dst[i] = xs[n-1];
        } else {
            t :: Float64 := pos - float64(k);
            dst[i] = xs[k] * (1.0 - t) + xs[k+1] * t;
        }
    };
    return 0;
}
