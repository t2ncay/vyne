# vstat/Types.vy - result and streaming-state interfaces.
#
# `Series` carries a typed Array<Float64> plus its length. Passing a
# Series to a native function keeps the pointer and the length together,
# which is what lets the wrappers call the k_* kernels with unboxed
# double* parameters. Construct one directly with
# `vstat.Series(data, data.size())`, or use `vstat.from_array(xs)` when
# you have a boxed Array.

ruleset { dynamic_casting };

module vstat;

interface Series {
    data :: Array<Float64>,
    n    :: Int64,
}

interface Summary {
    n        :: Int64,
    mean     :: Float64,
    variance :: Float64,
    stddev   :: Float64,
    minimum  :: Float64,
    maximum  :: Float64,
    median   :: Float64,
}

interface LinearFit {
    slope     :: Float64,
    intercept :: Float64,
    r2        :: Float64,
}

interface Histogram {
    edges  :: Array<Float64>,
    counts :: Array<Int64>,
}

interface Welford {
    n    :: Int64,
    mean :: Float64,
    m2   :: Float64,
}

interface WelfordCov {
    n      :: Int64,
    mean_x :: Float64,
    mean_y :: Float64,
    m2_x   :: Float64,
    m2_y   :: Float64,
    c      :: Float64,
}
