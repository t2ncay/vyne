# vinterp/TwoD.vy - 2-D interpolation on a row-major Grid.
#
# The `*_at` functions take fractional grid indices: fx in [0, cols-1],
# fy in [0, rows-1]. The physical-coordinate wrappers map an axis range
# onto those indices.

use "Types.vy";
use "Kernels.vy";

ruleset { dynamic_casting };

module vinterp;

fn :: vinterp nearest_at(g :: vinterp.Grid, fx :: Float64,
                         fy :: Float64) -> Float64 {
    return vinterp.k_nearest2d(g.data, g.rows, g.cols, fx, fy);
}

fn :: vinterp bilinear_at(g :: vinterp.Grid, fx :: Float64,
                          fy :: Float64) -> Float64 {
    return vinterp.k_bilinear(g.data, g.rows, g.cols, fx, fy);
}

fn :: vinterp bicubic_at(g :: vinterp.Grid, fx :: Float64,
                         fy :: Float64) -> Float64 {
    return vinterp.k_bicubic(g.data, g.rows, g.cols, fx, fy);
}

fn :: vinterp _map_fx(g :: vinterp.Grid, x :: Float64,
                      xmin :: Float64, xmax :: Float64) -> Float64 {
    if xmax == xmin { return 0.0; }
    return (x - xmin) / (xmax - xmin) * float64(g.cols - 1);
}

fn :: vinterp _map_fy(g :: vinterp.Grid, y :: Float64,
                      ymin :: Float64, ymax :: Float64) -> Float64 {
    if ymax == ymin { return 0.0; }
    return (y - ymin) / (ymax - ymin) * float64(g.rows - 1);
}

fn :: vinterp nearest2d(g :: vinterp.Grid, x :: Float64, y :: Float64,
                      xmin :: Float64, xmax :: Float64,
                      ymin :: Float64, ymax :: Float64) -> Float64 {
    return vinterp.nearest_at(
        g,
        vinterp._map_fx(g, x, xmin, xmax),
        vinterp._map_fy(g, y, ymin, ymax)
    );
}

fn :: vinterp bilinear2d(g :: vinterp.Grid, x :: Float64, y :: Float64,
                       xmin :: Float64, xmax :: Float64,
                       ymin :: Float64, ymax :: Float64) -> Float64 {
    return vinterp.bilinear_at(
        g,
        vinterp._map_fx(g, x, xmin, xmax),
        vinterp._map_fy(g, y, ymin, ymax)
    );
}

fn :: vinterp bicubic2d(g :: vinterp.Grid, x :: Float64, y :: Float64,
                      xmin :: Float64, xmax :: Float64,
                      ymin :: Float64, ymax :: Float64) -> Float64 {
    return vinterp.bicubic_at(
        g,
        vinterp._map_fx(g, x, xmin, xmax),
        vinterp._map_fy(g, y, ymin, ymax)
    );
}
