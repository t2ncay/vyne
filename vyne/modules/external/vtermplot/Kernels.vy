# vtermplot/Kernels.vy - coordinate mapping and rasterisation.
#
# These are the hot loops. Each draw function inlines its own bounds
# computation so it never calls another array-parameter function, which
# keeps the native array ABI gate happy. The only calls are to the
# scalar/Plot helpers below.

ruleset { dynamic_casting };

module vtermplot;

fn :: vtermplot k_map_col(plot :: vtermplot.Plot, x :: Float64) -> Int64 {
    if plot.width <= 1 { return 0; }
    span :: Float64 := plot.xmax - plot.xmin;
    if span == 0.0 { return plot.width / 2; }
    c :: Int64 := int64((x - plot.xmin) / span * float64(plot.width - 1));
    if c < 0 { c = 0; }
    if c >= plot.width { c = plot.width - 1; }
    return c;
}

fn :: vtermplot k_map_row(plot :: vtermplot.Plot, y :: Float64) -> Int64 {
    if plot.height <= 1 { return 0; }
    span :: Float64 := plot.ymax - plot.ymin;
    if span == 0.0 { return plot.height / 2; }
    r :: Int64 := int64((y - plot.ymin) / span * float64(plot.height - 1));
    if r < 0 { r = 0; }
    if r >= plot.height { r = plot.height - 1; }
    return plot.height - 1 - r;
}

fn :: vtermplot k_set(plot :: vtermplot.Plot, col :: Int64, row :: Int64,
                      code :: Int64) -> Int64 {
    if col < 0 || col >= plot.width || row < 0 || row >= plot.height {
        return 0;
    }
    plot.grid[row * plot.width + col] = code;
    return 0;
}

fn :: vtermplot k_line(plot :: vtermplot.Plot, c0 :: Int64, r0 :: Int64,
                       c1 :: Int64, r1 :: Int64, code :: Int64) -> Int64 {
    dc :: Int64 := c1 - c0;
    if dc < 0 { dc = 0 - dc; }
    sc :: Int64 := 1;
    if c0 > c1 { sc = -1; }

    dr :: Int64 := r1 - r0;
    if dr < 0 { dr = 0 - dr; }
    sr :: Int64 := 1;
    if r0 > r1 { sr = -1; }

    err :: Int64 := dc - dr;
    cc :: Int64 := c0;
    rr :: Int64 := r0;
    while true {
        vtermplot.k_set(plot, cc, rr, code);
        if cc == c1 && rr == r1 { break; }
        e2 :: Int64 := 2 * err;
        if e2 > 0 - dr {
            err = err - dr;
            cc = cc + sc;
        }
        if e2 < dc {
            err = err + dc;
            rr = rr + sr;
        }
    }
    return 0;
}

# ---------------------------------------------------------------------------
# Scatter
# ---------------------------------------------------------------------------

fn :: vtermplot k_draw_scatter(plot :: vtermplot.Plot,
                               xs :: Array<Float64>, ys :: Array<Float64>,
                               n :: Int64, code :: Int64) -> Int64 {
    if n <= 1 { return 0; }

    xmin :: Float64 := xs[0];
    xmax :: Float64 := xs[0];
    ymin :: Float64 := ys[0];
    ymax :: Float64 := ys[0];
    through i :: 1..n-1 -> loop {
        if xs[i] < xmin { xmin = xs[i]; }
        if xs[i] > xmax { xmax = xs[i]; }
        if ys[i] < ymin { ymin = ys[i]; }
        if ys[i] > ymax { ymax = ys[i]; }
    };

    if !plot.has_data {
        plot.xmin = xmin; plot.xmax = xmax;
        plot.ymin = ymin; plot.ymax = ymax;
        plot.has_data = true;
    } else {
        if xmin < plot.xmin { plot.xmin = xmin; }
        if xmax > plot.xmax { plot.xmax = xmax; }
        if ymin < plot.ymin { plot.ymin = ymin; }
        if ymax > plot.ymax { plot.ymax = ymax; }
    }

    through i :: 0..n-1 -> loop {
        col :: Int64 := vtermplot.k_map_col(plot, xs[i]);
        row :: Int64 := vtermplot.k_map_row(plot, ys[i]);
        vtermplot.k_set(plot, col, row, code);
    };
    return 0;
}

# ---------------------------------------------------------------------------
# Line
# ---------------------------------------------------------------------------

fn :: vtermplot k_draw_line(plot :: vtermplot.Plot,
                            xs :: Array<Float64>, ys :: Array<Float64>,
                            n :: Int64, code :: Int64) -> Int64 {
    if n <= 1 { return 0; }

    xmin :: Float64 := xs[0];
    xmax :: Float64 := xs[0];
    ymin :: Float64 := ys[0];
    ymax :: Float64 := ys[0];
    through i :: 1..n-1 -> loop {
        if xs[i] < xmin { xmin = xs[i]; }
        if xs[i] > xmax { xmax = xs[i]; }
        if ys[i] < ymin { ymin = ys[i]; }
        if ys[i] > ymax { ymax = ys[i]; }
    };

    if !plot.has_data {
        plot.xmin = xmin; plot.xmax = xmax;
        plot.ymin = ymin; plot.ymax = ymax;
        plot.has_data = true;
    } else {
        if xmin < plot.xmin { plot.xmin = xmin; }
        if xmax > plot.xmax { plot.xmax = xmax; }
        if ymin < plot.ymin { plot.ymin = ymin; }
        if ymax > plot.ymax { plot.ymax = ymax; }
    }

    c0 :: Int64 := vtermplot.k_map_col(plot, xs[0]);
    r0 :: Int64 := vtermplot.k_map_row(plot, ys[0]);
    through i :: 1..n-1 -> loop {
        c1 :: Int64 := vtermplot.k_map_col(plot, xs[i]);
        r1 :: Int64 := vtermplot.k_map_row(plot, ys[i]);
        vtermplot.k_line(plot, c0, r0, c1, r1, code);
        c0 = c1;
        r0 = r1;
    };
    return 0;
}

# ---------------------------------------------------------------------------
# Bars
# ---------------------------------------------------------------------------

fn :: vtermplot k_draw_bars(plot :: vtermplot.Plot,
                            ys :: Array<Float64>,
                            n :: Int64, code :: Int64) -> Int64 {
    if n <= 0 { return 0; }

    ymin :: Float64 := ys[0];
    ymax :: Float64 := ys[0];
    through i :: 1..n-1 -> loop {
        if ys[i] < ymin { ymin = ys[i]; }
        if ys[i] > ymax { ymax = ys[i]; }
    };
    if ymin > 0.0 { ymin = 0.0; }
    if ymax < 0.0 { ymax = 0.0; }

    if !plot.has_data {
        plot.xmin = 0.0;
        plot.xmax = float64(n - 1);
        plot.ymin = ymin;
        plot.ymax = ymax;
        plot.has_data = true;
    } else {
        if 0.0 < plot.xmin { plot.xmin = 0.0; }
        if float64(n - 1) > plot.xmax { plot.xmax = float64(n - 1); }
        if ymin < plot.ymin { plot.ymin = ymin; }
        if ymax > plot.ymax { plot.ymax = ymax; }
    }

    base :: Int64 := vtermplot.k_map_row(plot, 0.0);
    through i :: 0..n-1 -> loop {
        col :: Int64 := vtermplot.k_map_col(plot, float64(i));
        row :: Int64 := vtermplot.k_map_row(plot, ys[i]);
        vtermplot.k_line(plot, col, base, col, row, code);
    };
    return 0;
}

# Equal-width histogram rendered as bars. The binning is local so
# vtermplot does not depend on vstat.
fn :: vtermplot k_draw_hist(plot :: vtermplot.Plot,
                            xs :: Array<Float64>, n :: Int64,
                            bins :: Int64, code :: Int64) -> Int64 {
    if n <= 0 || bins <= 0 { return 0; }

    lo :: Float64 := xs[0];
    hi :: Float64 := xs[0];
    through i :: 1..n-1 -> loop {
        if xs[i] < lo { lo = xs[i]; }
        if xs[i] > hi { hi = xs[i]; }
    };
    if hi == lo { hi = lo + 1.0; }
    width :: Float64 := (hi - lo) / float64(bins);

    counts :: Array<Float64> := [];
    through i :: 0..bins-1 -> loop { counts.push(0.0); };

    through i :: 0..n-1 -> loop {
        idx :: Int64 := int64((xs[i] - lo) / width);
        if idx < 0 { idx = 0; }
        if idx >= bins { idx = bins - 1; }
        counts[idx] = counts[idx] + 1.0;
    };

    maxc :: Float64 := counts[0];
    through i :: 1..bins-1 -> loop {
        if counts[i] > maxc { maxc = counts[i]; }
    };
    if maxc < 1.0 { maxc = 1.0; }

    if !plot.has_data {
        plot.xmin = 0.0;
        plot.xmax = float64(bins - 1);
        plot.ymin = 0.0;
        plot.ymax = maxc;
        plot.has_data = true;
    } else {
        if 0.0 < plot.xmin { plot.xmin = 0.0; }
        if float64(bins - 1) > plot.xmax { plot.xmax = float64(bins - 1); }
        if 0.0 < plot.ymin { plot.ymin = 0.0; }
        if maxc > plot.ymax { plot.ymax = maxc; }
    }

    base :: Int64 := vtermplot.k_map_row(plot, 0.0);
    through i :: 0..bins-1 -> loop {
        col :: Int64 := vtermplot.k_map_col(plot, float64(i));
        row :: Int64 := vtermplot.k_map_row(plot, counts[i]);
        vtermplot.k_line(plot, col, base, col, row, code);
    };
    return 0;
}

# ---------------------------------------------------------------------------
# Heatmap
# ---------------------------------------------------------------------------

fn :: vtermplot k_heat(plot :: vtermplot.Plot,
                       data :: Array<Float64>, rows :: Int64,
                       cols :: Int64) -> Int64 {
    if rows <= 0 || cols <= 0 { return 0; }

    lo :: Float64 := data[0];
    hi :: Float64 := data[0];
    through i :: 1..rows*cols-1 -> loop {
        if data[i] < lo { lo = data[i]; }
        if data[i] > hi { hi = data[i]; }
    };
    span :: Float64 := hi - lo;
    if span == 0.0 { span = 1.0; }

    through r :: 0..plot.height-1 -> loop {
        sr :: Int64 := r * rows / plot.height;
        if sr >= rows { sr = rows - 1; }
        through c :: 0..plot.width-1 -> loop {
            sc :: Int64 := c * cols / plot.width;
            if sc >= cols { sc = cols - 1; }
            v :: Float64 := data[sr * cols + sc];
            level :: Int64 := 5 + int64((v - lo) / span * 9.0);
            if level < 5 { level = 5; }
            if level > 13 { level = 13; }
            plot.grid[r * plot.width + c] = level;
        };
    };

    plot.xmin = 0.0;
    plot.xmax = float64(cols - 1);
    plot.ymin = 0.0;
    plot.ymax = float64(rows - 1);
    plot.has_data = true;
    return 0;
}
