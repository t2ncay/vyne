# vinterp/Types.vy - interpolation data types.

ruleset { dynamic_casting };

module vinterp;

interface Curve {
    xs :: Array<Float64>,
    ys :: Array<Float64>,
    n  :: Int64,
}

interface Spline {
    xs :: Array<Float64>,
    ys :: Array<Float64>,
    m  :: Array<Float64>,   # second derivatives at the knots
    n  :: Int64,
}

interface Grid {
    data :: Array<Float64>,
    rows :: Int64,
    cols :: Int64,
}
