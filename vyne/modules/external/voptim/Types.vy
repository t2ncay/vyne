# voptim/Types.vy - optimizer state and the flat-vector view.
#
# `Series` keeps a typed Array<Float64> and its length together. Passing a
# Series to a wrapper lets the k_* kernels see unboxed `double*` payloads.
# The same shape works for a vlin Matrix: pass `matrix.data` and
# `matrix.size()` through `voptim.Series(data, n)`.
#
# Optimizer state is mutable: the parameter and moment arrays are shared
# through every reference, so a step updates the caller's buffers in place.
# Construct state once, then call the step functions in the training loop.

ruleset { dynamic_casting };

module voptim;

interface Series {
    data :: Array<Float64>,
    n    :: Int64,
}

interface SGDState {
    params :: Array<Float64>,
    n      :: Int64,
}

interface MomentumState {
    params   :: Array<Float64>,
    velocity :: Array<Float64>,
    momentum :: Float64,
    n        :: Int64,
}

interface AdamState {
    params       :: Array<Float64>,
    m            :: Array<Float64>,
    v            :: Array<Float64>,
    beta1        :: Float64,
    beta2        :: Float64,
    eps          :: Float64,
    weight_decay :: Float64,
    t            :: Int64,
    n            :: Int64,
}

interface RMSPropState {
    params :: Array<Float64>,
    cache  :: Array<Float64>,
    decay  :: Float64,
    eps    :: Float64,
    n      :: Int64,
}

interface AdagradState {
    params :: Array<Float64>,
    accum  :: Array<Float64>,
    eps    :: Float64,
    n      :: Int64,
}
