# vinterp/Constructors.vy - boxed convenience constructors and validation.

use "Types.vy";

ruleset { dynamic_casting };

module vinterp;

# Copy boxed arrays into a Curve and require strictly ascending x.
# Construct `vinterp.Curve(xs, ys, n)` directly for zero-copy when the
# typed arrays are already sorted.
fn :: vinterp curve(xs :: Array, ys :: Array) -> vinterp.Curve {
    n :: Int64 := xs.size();
    if ys.size() != n {
        throw "vinterp.curve: xs and ys differ in length";
    }
    xd :: Array<Float64> := [];
    yd :: Array<Float64> := [];
    through i :: 0..n-1 -> loop {
        xd.push(xs[i]);
        yd.push(ys[i]);
    };
    through i :: 1..n-1 -> loop {
        if xd[i] <= xd[i-1] {
            throw "vinterp.curve: x values must be strictly ascending";
        }
    };
    return vinterp.Curve(xd, yd, n);
}

# Copy a boxed row-major matrix into a Grid.
fn :: vinterp grid(data :: Array, rows :: Int64,
                   cols :: Int64) -> vinterp.Grid {
    if rows <= 0 || cols <= 0 {
        throw "vinterp.grid: rows and cols must be positive";
    }
    n :: Int64 := rows * cols;
    if data.size() != n {
        throw "vinterp.grid: data length does not match rows * cols";
    }
    typed :: Array<Float64> := [];
    through i :: 0..n-1 -> loop { typed.push(data[i]); };
    return vinterp.Grid(typed, rows, cols);
}

fn :: vinterp grid_value(g :: vinterp.Grid, r :: Int64,
                         c :: Int64) -> Float64 {
    if r < 0 || r >= g.rows || c < 0 || c >= g.cols {
        throw "vinterp.grid_value: index out of bounds";
    }
    return g.data[r * g.cols + c];
}
