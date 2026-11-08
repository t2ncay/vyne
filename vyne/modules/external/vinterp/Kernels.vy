# vinterp/Kernels.vy - interpolation hot loops.
#
# Each function inlines its interval search so it calls no other
# array-parameter function. That keeps the native array ABI gate happy.

ruleset { dynamic_casting };

module vinterp;

fn :: vinterp k_nearest(xs :: Array<Float64>, ys :: Array<Float64>,
                        n :: Int64, x :: Float64) -> Float64 {
    if n <= 0 { return 0.0; }
    if x <= xs[0] { return ys[0]; }
    if x >= xs[n-1] { return ys[n-1]; }
    lo :: Int64 := 0;
    hi :: Int64 := n - 1;
    while hi - lo > 1 {
        mid :: Int64 := (lo + hi) / 2;
        if xs[mid] <= x { lo = mid; } else { hi = mid; }
    }
    dl :: Float64 := x - xs[lo];
    dh :: Float64 := xs[hi] - x;
    if dl <= dh { return ys[lo]; }
    return ys[hi];
}

fn :: vinterp k_linear(xs :: Array<Float64>, ys :: Array<Float64>,
                       n :: Int64, x :: Float64) -> Float64 {
    if n <= 0 { return 0.0; }
    if n == 1 { return ys[0]; }
    if x <= xs[0] {
        # Extrapolate the first segment linearly.
        h0 :: Float64 := xs[1] - xs[0];
        return ys[0] + (x - xs[0]) * (ys[1] - ys[0]) / h0;
    }
    if x >= xs[n-1] {
        # Extrapolate the last segment linearly.
        h0 :: Float64 := xs[n-1] - xs[n-2];
        return ys[n-1] + (x - xs[n-1]) * (ys[n-1] - ys[n-2]) / h0;
    }
    lo :: Int64 := 0;
    hi :: Int64 := n - 1;
    while hi - lo > 1 {
        mid :: Int64 := (lo + hi) / 2;
        if xs[mid] <= x { lo = mid; } else { hi = mid; }
    }
    h :: Float64 := xs[lo+1] - xs[lo];
    t :: Float64 := (x - xs[lo]) / h;
    return ys[lo] * (1.0 - t) + ys[lo+1] * t;
}

fn :: vinterp k_lagrange(xs :: Array<Float64>, ys :: Array<Float64>,
                         n :: Int64, x :: Float64) -> Float64 {
    if n <= 0 { return 0.0; }
    result :: Float64 := 0.0;
    through i :: 0..n-1 -> loop {
        term :: Float64 := ys[i];
        through j :: 0..n-1 -> loop {
            if j != i {
                den :: Float64 := xs[i] - xs[j];
                if den == 0.0 {
                    throw "vinterp.lagrange: duplicate x values";
                }
                term = term * (x - xs[j]) / den;
            }
        };
        result = result + term;
    };
    return result;
}

fn :: vinterp k_spline_eval(xs :: Array<Float64>, ys :: Array<Float64>,
                            m :: Array<Float64>, n :: Int64,
                            x :: Float64) -> Float64 {
    if n <= 0 { return 0.0; }
    if n == 1 { return ys[0]; }
    if x <= xs[0] { return ys[0]; }
    if x >= xs[n-1] { return ys[n-1]; }

    lo :: Int64 := 0;
    hi :: Int64 := n - 1;
    while hi - lo > 1 {
        mid :: Int64 := (lo + hi) / 2;
        if xs[mid] <= x { lo = mid; } else { hi = mid; }
    }
    h :: Float64 := xs[lo+1] - xs[lo];
    a :: Float64 := (xs[lo+1] - x) / h;
    b :: Float64 := (x - xs[lo]) / h;
    return a * ys[lo] + b * ys[lo+1] +
           ((a * a * a - a) * m[lo] + (b * b * b - b) * m[lo+1]) * h * h / 6.0;
}

# ---------------------------------------------------------------------------
# 2D
# ---------------------------------------------------------------------------

fn :: vinterp k_nearest2d(data :: Array<Float64>, rows :: Int64,
                          cols :: Int64, fx :: Float64,
                          fy :: Float64) -> Float64 {
    if rows <= 0 || cols <= 0 { return 0.0; }
    x :: Float64 := fx;
    y :: Float64 := fy;
    if x < 0.0 { x = 0.0; }
    if x > float64(cols-1) { x = float64(cols-1); }
    if y < 0.0 { y = 0.0; }
    if y > float64(rows-1) { y = float64(rows-1); }
    c :: Int64 := int64(x + 0.5);
    r :: Int64 := int64(y + 0.5);
    if c >= cols { c = cols - 1; }
    if r >= rows { r = rows - 1; }
    return data[r * cols + c];
}

fn :: vinterp k_bilinear(data :: Array<Float64>, rows :: Int64,
                         cols :: Int64, fx :: Float64,
                         fy :: Float64) -> Float64 {
    if rows <= 0 || cols <= 0 { return 0.0; }
    x :: Float64 := fx;
    y :: Float64 := fy;
    if x < 0.0 { x = 0.0; }
    if x > float64(cols-1) { x = float64(cols-1); }
    if y < 0.0 { y = 0.0; }
    if y > float64(rows-1) { y = float64(rows-1); }

    c0 :: Int64 := int64(x);
    r0 :: Int64 := int64(y);
    c1 :: Int64 := c0 + 1;
    if c1 >= cols { c1 = cols - 1; }
    r1 :: Int64 := r0 + 1;
    if r1 >= rows { r1 = rows - 1; }

    tx :: Float64 := x - float64(c0);
    ty :: Float64 := y - float64(r0);
    v00 :: Float64 := data[r0 * cols + c0];
    v01 :: Float64 := data[r0 * cols + c1];
    v10 :: Float64 := data[r1 * cols + c0];
    v11 :: Float64 := data[r1 * cols + c1];
    top :: Float64 := v00 + (v01 - v00) * tx;
    bottom :: Float64 := v10 + (v11 - v10) * tx;
    return top + (bottom - top) * ty;
}

# Catmull-Rom bicubic. Reproduces the grid at integer coordinates.
fn :: vinterp k_bicubic(data :: Array<Float64>, rows :: Int64,
                        cols :: Int64, fx :: Float64,
                        fy :: Float64) -> Float64 {
    if rows <= 0 || cols <= 0 { return 0.0; }
    x :: Float64 := fx;
    y :: Float64 := fy;
    if x < 0.0 { x = 0.0; }
    if x > float64(cols-1) { x = float64(cols-1); }
    if y < 0.0 { y = 0.0; }
    if y > float64(rows-1) { y = float64(rows-1); }

    c0 :: Int64 := int64(x);
    r0 :: Int64 := int64(y);
    tx :: Float64 := x - float64(c0);
    ty :: Float64 := y - float64(r0);

    wx0 :: Float64 := ((-0.5 * tx + 1.0) * tx - 0.5) * tx;
    wx1 :: Float64 := ((1.5 * tx - 2.5) * tx) * tx + 1.0;
    wx2 :: Float64 := ((-1.5 * tx + 2.0) * tx + 0.5) * tx;
    wx3 :: Float64 := ((0.5 * tx - 0.5) * tx) * tx;

    wy0 :: Float64 := ((-0.5 * ty + 1.0) * ty - 0.5) * ty;
    wy1 :: Float64 := ((1.5 * ty - 2.5) * ty) * ty + 1.0;
    wy2 :: Float64 := ((-1.5 * ty + 2.0) * ty + 0.5) * ty;
    wy3 :: Float64 := ((0.5 * ty - 0.5) * ty) * ty;

    result :: Float64 := 0.0;
    through rr :: 0..3 -> loop {
        ri :: Int64 := r0 - 1 + rr;
        if ri < 0 { ri = 0; }
        if ri >= rows { ri = rows - 1; }

        wy :: Float64 := wy3;
        if rr == 0 { wy = wy0; }
        else if rr == 1 { wy = wy1; }
        else if rr == 2 { wy = wy2; }

        row_sum :: Float64 := 0.0;
        through cc :: 0..3 -> loop {
            ci :: Int64 := c0 - 1 + cc;
            if ci < 0 { ci = 0; }
            if ci >= cols { ci = cols - 1; }

            wx :: Float64 := wx3;
            if cc == 0 { wx = wx0; }
            else if cc == 1 { wx = wx1; }
            else if cc == 2 { wx = wx2; }

            row_sum = row_sum + wx * data[ri * cols + ci];
        };
        result = result + wy * row_sum;
    };
    return result;
}
